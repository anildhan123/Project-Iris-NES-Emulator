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
    return adopt(std::move(parsed.value()), error);
}

bool NesSystem::loadBytes(const std::uint8_t* data, std::size_t size, const std::string& name, std::string& error) {
    auto parsed = Cartridge::fromBytes(data, size, name);
    if (!parsed.ok()) {
        error = parsed.error().message;
        return false;
    }
    return adopt(std::move(parsed.value()), error);
}

bool NesSystem::adopt(Cartridge cartridge, std::string& error) {
    if (!mapperSupported(cartridge, error)) {
        return false;
    }

    const RegionTiming& timing = regionTiming(cartridge.region());
    cartridge_ = std::move(cartridge);
    mapper_ = createMapper(cartridge_);
    region_ = timing.region;
    ppuNumerator_ = timing.ppuNumerator;
    ppuDenominator_ = timing.ppuDenominator;
    ppuAccum_ = 0;
    dmcStall_ = 0;
    bus_.connect(mapper_.get());
    ppu_.connect(mapper_.get(), cartridge_.mirroring());
    ppu_.setTiming(timing.scanlines, timing.oddFrameSkip);
    title_ = cartridge_.name();
    loaded_ = true;
    paused_ = false;
    cpuCycles_ = 0;
    nmiLevel_ = false;
    audio_.clear();
    bus_.power();
    ppu_.power();
    apu_.setRegion(region_);
    apu_.reset();
    mapper_->reset();
    cpu_.reset(bus_);
    return true;
}

void NesSystem::unload() {
    loaded_ = false;
    paused_ = false;
    title_.clear();
    audio_.clear();
    mapper_.reset();
    bus_.connect(nullptr);
    ppu_.connect(nullptr, Mirroring::Horizontal);
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
    apu_.clock(audio_);
    cpu_.setIrqLine(irqLine());
    std::uint16_t sampleAddress = 0;
    if (apu_.pollDmc(sampleAddress)) {
        apu_.supplyDmc(bus_.read(sampleAddress));
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
    return AudioView{audio_.data(), static_cast<int>(audio_.size()), Apu::kSampleRate};
}

void NesSystem::setController(int port, std::uint8_t buttons) {
    if (port == 0) {
        port1_.setButtons(buttons);
    } else if (port == 1) {
        port2_.setButtons(buttons);
    }
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
