#include "iris/nes/nes_system.hpp"

#include "iris/nes/mapper_factory.hpp"

namespace iris::systems::nes {
namespace {

constexpr int kMaxCyclesPerFrame = 40000;

}  // namespace

NesSystem::NesSystem() : bus_(ppu_, apu_, port1_, port2_) {}

bool NesSystem::loadContent(const std::filesystem::path& path, std::string& error) {
    auto parsed = Cartridge::fromFile(path);
    if (!parsed.ok()) {
        error = parsed.error().message;
        return false;
    }
    return adopt(parsed.value(), error);
}

bool NesSystem::loadBytes(const std::uint8_t* data, std::size_t size, const std::string& name, std::string& error) {
    auto parsed = Cartridge::fromBytes(data, size, name);
    if (!parsed.ok()) {
        error = parsed.error().message;
        return false;
    }
    return adopt(parsed.value(), error);
}

bool NesSystem::adopt(Cartridge cartridge, std::string& error) {
    if (!mapperSupported(cartridge, error)) {
        return false;
    }

    cartridge_ = std::move(cartridge);
    mapper_ = createMapper(cartridge_);
    applyRegion();
    ppuAccum_ = 0;
    dmcStall_ = 0;
    bus_.connect(mapper_.get());
    ppu_.connect(mapper_.get(), cartridge_.mirroring());
    title_ = cartridge_.name();
    refreshContentId();
    loaded_ = true;
    paused_ = false;
    cpuCycles_ = 0;
    nmiLevel_ = false;
    audio_.clear();
    bus_.power();
    ppu_.power();
    apu_.reset();
    mapper_->reset();
    cpu_.reset(bus_);
    return true;
}

void NesSystem::unload() {
    loaded_ = false;
    paused_ = false;
    title_.clear();
    contentId_.clear();
    audio_.clear();
    mapper_.reset();
    bus_.connect(nullptr);
    ppu_.connect(nullptr, Mirroring::Horizontal);
}

void NesSystem::applyRegion() {
    Region region = cartridge_.region();
    switch (regionChoice_) {
    case iris::RegionChoice::Ntsc:
        region = Region::Ntsc;
        break;
    case iris::RegionChoice::Pal:
        region = Region::Pal;
        break;
    case iris::RegionChoice::Dendy:
        region = Region::Dendy;
        break;
    case iris::RegionChoice::Automatic:
        break;
    }
    const RegionTiming& timing = regionTiming(region);
    region_ = timing.region;
    ppuNumerator_ = timing.ppuNumerator;
    ppuDenominator_ = timing.ppuDenominator;
    ppu_.setTiming(timing.scanlines, timing.oddFrameSkip);
    ppu_.setRegion(region_);
    apu_.setRegion(region_);
}

void NesSystem::setRegionChoice(iris::RegionChoice choice) {
    regionChoice_ = choice;
    if (loaded_) {
        applyRegion();
    }
}

void NesSystem::reset() {
    if (!loaded_) {
        return;
    }
    paused_ = false;
    nmiLevel_ = false;
    ppuAccum_ = 0;
    dmcStall_ = 0;
    apu_.reset();
    bus_.reset();
    ppu_.reset();
    if (mapper_) {
        mapper_->reset();
    }
    cpu_.reset(bus_);
}

void NesSystem::powerCycle() {
    if (!loaded_) {
        return;
    }
    paused_ = false;
    nmiLevel_ = false;
    ppuAccum_ = 0;
    dmcStall_ = 0;
    cpuCycles_ = 0;
    audio_.clear();
    bus_.power();
    ppu_.power();
    applyRegion();
    apu_.reset();
    if (mapper_) {
        mapper_->reset();
    }
    cpu_.reset(bus_);
}

bool NesSystem::irqLine() const { return apu_.irq() || (mapper_ != nullptr && mapper_->irq()); }

void NesSystem::clockCpuCycle() {
    ++cpuCycles_;
    if (mapper_ != nullptr) {
        mapper_->cpuCycle();
    }
    ppuAccum_ += ppuNumerator_;
    while (ppuAccum_ >= ppuDenominator_) {
        ppuAccum_ -= ppuDenominator_;
        ppu_.clock();
        const bool level = ppu_.nmiLine();
        if (!nmiLevel_ && level) {
            cpu_.signalNmi();
        }
        nmiLevel_ = level;
    }
    const float expansion = mapper_ != nullptr ? mapper_->expansionSample() : 0.0f;
    apu_.clock(audio_, expansion);
    cpu_.setIrqLine(irqLine());
    std::uint16_t sampleAddress = 0;
    if (apu_.pollDmc(sampleAddress)) {
        apu_.supplyDmc(bus_.read(sampleAddress));
        // DMC sample DMA stalls the CPU for four cycles on this host path.
        dmcStall_ += 4;
    }
}

void NesSystem::runFrame() {
    if (!loaded_ || paused_) {
        return;
    }
    audio_.clear();
    const std::uint64_t target = ppu_.frame() + 1;
    int budget = kMaxCyclesPerFrame;
    while (ppu_.frame() < target && budget > 0) {
        cpu_.setIrqLine(irqLine());
        const bool levelBefore = ppu_.nmiLine();
        const int cycles = cpu_.step(bus_);
        if (!levelBefore && ppu_.nmiLine()) {
            cpu_.signalNmi();
        }
        nmiLevel_ = ppu_.nmiLine();
        for (int index = 0; index < cycles; ++index) {
            clockCpuCycle();
        }
        const int dma = bus_.takeDmaCycles(cpuCycles_);
        for (int index = 0; index < dma; ++index) {
            clockCpuCycle();
        }
        while (dmcStall_ > 0) {
            --dmcStall_;
            clockCpuCycle();
        }
        --budget;
    }
}

FrameView NesSystem::frame() const {
    if (!loaded_) {
        return {};
    }
    return FrameView{ppu_.pixels(), Ppu::kWidth, Ppu::kHeight};
}

AudioView NesSystem::audio() const {
    return AudioView{audio_.data(), static_cast<int>(audio_.size()), Apu::kSampleRate, 1};
}

void NesSystem::setInputDevice(int port, ControllerDevice device) {
    if (port == 0) {
        port1_.setDevice(device);
    } else if (port == 1) {
        port2_.setDevice(device);
    }
}

void NesSystem::setPointer(int x, int y) {
    pointerX_ = x;
    pointerY_ = y;
    refreshZapperLight();
}

void NesSystem::refreshZapperLight() {
    if (port2_.device() != ControllerDevice::Zapper) {
        return;
    }
    const bool light = loaded_ && ppu_.sampleLight(pointerX_, pointerY_);
    port2_.setZapper(false, light);
}

void NesSystem::setController(int port, std::uint8_t buttons) {
    if (port == 0) {
        port1_.setButtons(buttons);
    } else if (port == 1) {
        if (port2_.device() == ControllerDevice::Zapper) {
            port2_.setZapper((buttons & 0x01) != 0, ppu_.sampleLight(pointerX_, pointerY_));
        } else {
            port2_.setButtons(buttons);
        }
    } else if (port == 2) {
        port1_.setExtraButtons(buttons);
    } else if (port == 3) {
        port2_.setExtraButtons(buttons);
    }
}

bool NesSystem::describeContent(
    const std::filesystem::path& path, ContentDescription& description, std::string& error) const {
    auto parsed = Cartridge::fromFile(path);
    if (!parsed.ok()) {
        error = parsed.error().message;
        return false;
    }
    const Cartridge& cartridge = parsed.value();
    description.title = cartridge.name();
    description.contentId = contentIdForCartridge(cartridge);
    std::string supportError;
    if (!mapperSupported(cartridge, supportError)) {
        description.detail = std::move(supportError);
    } else {
        description.detail = std::string(mapperLabel(cartridge.mapper())) + ", " + regionLabel(cartridge.region());
    }
    return true;
}

SystemInfo NesSystem::info() const {
    SystemInfo info;
    info.title = title_.empty() ? "NES" : title_;
    if (loaded_) {
        info.detail = std::string(mapperLabel(cartridge_.mapper())) + ", " + regionLabel(region_);
    }
    info.batteryBacked = loaded_ && cartridge_.battery();
    return info;
}

std::int64_t NesSystem::framePeriodNs() const { return iris::systems::nes::framePeriodNs(loaded_ ? region_ : Region::Ntsc); }

std::uint8_t NesSystem::debugRead(std::uint16_t address) { return bus_.read(address); }

std::unique_ptr<IEmulatedSystem> createNesSystem() { return std::make_unique<NesSystem>(); }

}  // namespace iris::systems::nes
