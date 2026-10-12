#include "iris/nes/nes_system.hpp"

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
    if (cartridge.mapper() != 0 || cartridge.submapper() != 0) {
        error = "Mapper " + std::to_string(cartridge.mapper()) + " is not supported. This version plays NROM (mapper 0) only.";
        return false;
    }
    const std::size_t prg = cartridge.prg().size();
    if (prg != 16 * 1024 && prg != 32 * 1024) {
        error = "NROM requires 16 KiB or 32 KiB of PRG ROM.";
        return false;
    }
    if (cartridge.chr().size() != 8 * 1024) {
        error = "NROM requires 8 KiB of CHR ROM or CHR RAM.";
        return false;
    }

    cartridge_ = std::move(cartridge);
    mapper_ = std::make_unique<Nrom>(cartridge_);
    bus_.connect(mapper_.get());
    ppu_.connect(mapper_.get(), cartridge_.mirroring());
    title_ = cartridge_.name();
    loaded_ = true;
    paused_ = false;
    cpuCycles_ = 0;
    nmiLevel_ = false;
    audio_.clear();
    bus_.power();
    ppu_.power();
    apu_.reset();
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
    apu_.reset();
    bus_.reset();
    ppu_.reset();
    cpu_.reset(bus_);
}

void NesSystem::tick() {
    ++cpuCycles_;
    ppu_.clock();
    ppu_.clock();
    ppu_.clock();
    const bool level = ppu_.nmiLine();
    if (!nmiLevel_ && level) {
        cpu_.signalNmi();
    }
    nmiLevel_ = level;
    apu_.clock(audio_);
}

void NesSystem::runFrame() {
    if (!loaded_ || paused_) {
        return;
    }
    audio_.clear();
    const std::uint64_t target = ppu_.frame() + 1;
    int budget = kMaxCyclesPerFrame;
    while (ppu_.frame() < target && budget > 0) {
        const bool levelBefore = ppu_.nmiLine();
        const int cycles = cpu_.step(bus_);
        if (!levelBefore && ppu_.nmiLine()) {
            cpu_.signalNmi();
        }
        nmiLevel_ = ppu_.nmiLine();
        for (int index = 0; index < cycles; ++index) {
            tick();
        }
        const int dma = bus_.takeDmaCycles(cpuCycles_);
        for (int index = 0; index < dma; ++index) {
            tick();
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
    info.detail = "NROM, NTSC";
    info.batteryBacked = loaded_ && cartridge_.battery();
    return info;
}

std::unique_ptr<IEmulatedSystem> createNesSystem() { return std::make_unique<NesSystem>(); }

}  // namespace iris::systems::nes
