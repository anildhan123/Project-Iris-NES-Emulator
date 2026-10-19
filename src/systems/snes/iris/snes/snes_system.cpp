#include "iris/snes/snes_system.hpp"

#include "iris/sha256.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>

namespace iris::systems::snes {
namespace {

constexpr int kDotsPerLine = 1364;
constexpr int kVblankLine = 225;
constexpr std::int64_t kNtscFrameNs = 1364LL * 262 * 1000000000LL / 21477272LL;
constexpr std::int64_t kPalFrameNs = 1364LL * 312 * 1000000000LL / 21281370LL;

bool readAll(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "The Super NES ROM could not be opened.";
        return false;
    }
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    if (length < 0 || static_cast<std::size_t>(length) > 8 * 1024 * 1024 + 512) {
        error = "The Super NES ROM is too large.";
        return false;
    }
    input.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!input) {
        error = "The Super NES ROM could not be read.";
        return false;
    }
    return true;
}

}  // namespace

SnesSystem::SnesSystem() {
    bus_.power();
}

bool SnesSystem::loadContent(const std::filesystem::path& path, std::string& error) {
    std::vector<std::uint8_t> bytes;
    if (!readAll(path, bytes, error)) {
        return false;
    }
    return loadBytes(bytes.data(), bytes.size(), error);
}

bool SnesSystem::loadBytes(const std::uint8_t* data, std::size_t size, std::string& error) {
    const Result<Cartridge> parsed = parseCartridge(data, size);
    if (!parsed.ok()) {
        error = parsed.error().message;
        return false;
    }
    return adopt(parsed.value(), error);
}

bool SnesSystem::loadIplFile(const std::filesystem::path& path, std::string& error) {
    std::vector<std::uint8_t> bytes;
    std::string readError;
    if (!readAll(path, bytes, readError)) {
        error = "The IPL ROM could not be opened.";
        return false;
    }
    if (bytes.size() != Apu::kIplSize) {
        error = "The IPL ROM must be exactly 64 bytes.";
        return false;
    }
    if (!bus_.installIpl(bytes.data(), bytes.size())) {
        error = "The IPL ROM could not be installed.";
        return false;
    }
    bus_.power();
    if (loaded_) {
        bus_.map(cartridge_.rom, cartridge_.hiRom, cartridge_.exHiRom, cartridge_.sramBytes, cartridge_.chip);
        applyRegion();
        cpu_.reset(bus_);
        bus_.resetChips();
        sa1_.resetTo(0);
    }
    error.clear();
    return true;
}

bool SnesSystem::hasIpl() const { return bus_.hasIpl(); }

bool SnesSystem::adopt(Cartridge cartridge, std::string& error) {
    if (cartridge.rom.size() < 32 * 1024) {
        error = "The Super NES ROM is too small.";
        return false;
    }
    cartridge_ = std::move(cartridge);
    contentId_ = sha256Hex(cartridge_.rom.data(), cartridge_.rom.size());
    standard_ = cartridge_.standard;
    bus_.power();
    bus_.map(cartridge_.rom, cartridge_.hiRom, cartridge_.exHiRom, cartridge_.sramBytes, cartridge_.chip);
    applyRegion();
    cpu_.reset(bus_);
    loaded_ = true;
    paused_ = false;
    error.clear();
    return true;
}

void SnesSystem::unload() {
    loaded_ = false;
    paused_ = false;
    contentId_.clear();
    cartridge_ = Cartridge{};
    bus_.power();
}

void SnesSystem::reset() {
    if (!loaded_) {
        return;
    }
    cpu_.reset(bus_);
    bus_.ppu().reset();
    bus_.resetApu();
    bus_.resetChips();
    sa1_.resetTo(0);
}

void SnesSystem::powerCycle() {
    if (!loaded_) {
        return;
    }
    std::vector<std::uint8_t> saved;
    bus_.copySram(saved);
    bus_.power();
    bus_.map(cartridge_.rom, cartridge_.hiRom, cartridge_.exHiRom, cartridge_.sramBytes, cartridge_.chip);
    if (!saved.empty()) {
        (void)bus_.restoreSram(saved.data(), saved.size());
    }
    applyRegion();
    cpu_.reset(bus_);
}

void SnesSystem::applyRegion() {
    if (regionChoice_ == RegionChoice::Pal) {
        standard_ = TvStandard::Pal;
    } else if (regionChoice_ == RegionChoice::Ntsc || regionChoice_ == RegionChoice::Dendy) {
        standard_ = TvStandard::Ntsc;
    } else {
        standard_ = cartridge_.standard;
    }
}

void SnesSystem::setRegionChoice(RegionChoice choice) {
    regionChoice_ = choice;
    if (loaded_) {
        applyRegion();
    }
}

void SnesSystem::runFrame() {
    if (!loaded_) {
        return;
    }
    const int lines = standard_ == TvStandard::Pal ? 312 : 262;
    // NTSC shortens one scanline by four master clocks each frame. DRAM refresh
    // removes forty clocks inside every line, which is when the CPU cannot run.
    const int shortLine = standard_ == TvStandard::Ntsc ? 240 : -1;
    int frameCycles = 0;
    for (int line = 0; line < lines; ++line) {
        frameCycles += line == shortLine ? kDotsPerLine - 4 : kDotsPerLine;
    }
    const int nmiCycle = kDotsPerLine * kVblankLine;
    const int clock = standard_ == TvStandard::Pal ? 21281370 : 21477272;
    bus_.beginFrame();
    int cycles = 0;
    int lineStart = 0;
    bool signaled = false;
    bool vIrqDone = false;
    bool hdmaStarted = false;
    int hIrqLine = -1;
    for (int line = 0; line < lines; ++line) {
        const int lineDots = line == shortLine ? kDotsPerLine - 4 : kDotsPerLine;
        const int lineEnd = lineStart + lineDots;
        constexpr int kRefreshStart = 1292;
        constexpr int kRefreshClocks = 40;
        bool hIrqDone = false;
        while (cycles < lineEnd && cycles < frameCycles && !cpu_.stopped()) {
            const int dot = cycles - lineStart;
            if (dot >= kRefreshStart && dot < kRefreshStart + kRefreshClocks) {
                cycles = lineStart + kRefreshStart + kRefreshClocks;
                if (cycles > lineEnd) {
                    cycles = lineEnd;
                }
                bus_.setCycle(cycles);
                const int caught = cycles < frameCycles ? cycles : frameCycles;
                bus_.catchUpApu(caught, clock);
                continue;
            }
            bus_.setCycle(cycles);
            if (!signaled && cycles >= nmiCycle) {
                bus_.raiseNmi();
                if (bus_.nmiEnabled()) {
                    cpu_.signalNmi();
                }
                signaled = true;
            }
            const int activeDot = cycles - lineStart;
            const bool vOn = bus_.virqEnabled();
            const bool hOn = bus_.hirqEnabled();
            if (hIrqLine != line) {
                hIrqDone = false;
                hIrqLine = line;
            }
            if (vOn && !hOn && !vIrqDone && line >= bus_.irqLine()) {
                bus_.raiseIrq();
                cpu_.signalIrq();
                vIrqDone = true;
            } else if (hOn && !vOn && !hIrqDone && activeDot >= bus_.irqDot() * 4) {
                bus_.raiseIrq();
                cpu_.signalIrq();
                hIrqDone = true;
            } else if (vOn && hOn && !vIrqDone && line >= bus_.irqLine() && activeDot >= bus_.irqDot() * 4) {
                bus_.raiseIrq();
                cpu_.signalIrq();
                vIrqDone = true;
            }
            if (cpu_.parked()) {
                int wake = lineEnd;
                if (!signaled && nmiCycle > cycles && nmiCycle < wake) {
                    wake = nmiCycle;
                }
                const int irqAt = lineStart + bus_.irqDot() * 4;
                const bool irqDue = (vOn && !hOn && !vIrqDone && line >= bus_.irqLine()) ||
                                    (hOn && !vOn && !hIrqDone && irqAt >= cycles) ||
                                    (vOn && hOn && !vIrqDone && line >= bus_.irqLine() && irqAt >= cycles);
                if (irqDue) {
                    const int irqWake = (vOn && !hOn) ? cycles : irqAt;
                    if (irqWake >= cycles && irqWake < wake) {
                        wake = irqWake;
                    }
                }
                if (wake > cycles) {
                    cycles = wake;
                    const int caught = cycles < frameCycles ? cycles : frameCycles;
                    bus_.catchUpApu(caught, clock);
                    continue;
                }
            }
            const int used = cpu_.step(bus_);
            if (used == 0 && cpu_.stopped()) {
                break;
            }
            cycles += used > 0 ? used : 1;
            cycles += bus_.takeDmaStall();
            if (cpu_.consumeSpin() && cycles < lineEnd) {
                int wake = lineEnd;
                if (!signaled && nmiCycle > cycles && nmiCycle < wake) {
                    wake = nmiCycle;
                }
                const int irqAt = lineStart + bus_.irqDot() * 4;
                const bool irqDue = (vOn && !hOn && !vIrqDone && line >= bus_.irqLine() && cycles >= lineStart) ||
                                    (hOn && !vOn && !hIrqDone && irqAt > cycles) ||
                                    (vOn && hOn && !vIrqDone && line >= bus_.irqLine() && irqAt > cycles);
                if (irqDue) {
                    const int irqWake = (vOn && !hOn) ? cycles : irqAt;
                    if (irqWake > cycles && irqWake < wake) {
                        wake = irqWake;
                    }
                }
                if (wake > cycles) {
                    cycles = wake;
                }
            }
            const int caught = cycles < frameCycles ? cycles : frameCycles;
            bus_.catchUpApu(caught, clock);
            if (bus_.takeSa1Start()) {
                sa1_.resetTo(bus_.sa1ResetVector());
            }
            if (bus_.sa1Running() && !sa1_.stopped()) {
                bus_.setSa1Access(true);
                for (int step = 0; step < 4 && !sa1_.stopped(); ++step) {
                    sa1_.step(bus_);
                }
                bus_.setSa1Access(false);
            }
            bus_.stepGsu(8);
        }
        if (!signaled && cycles >= nmiCycle) {
            bus_.raiseNmi();
            if (bus_.nmiEnabled()) {
                cpu_.signalNmi();
            }
            signaled = true;
        }
        if (line < Ppu::kHeight) {
            if (!hdmaStarted) {
                cycles += bus_.beginHdma();
                hdmaStarted = true;
            }
            cycles += bus_.transferHdma();
            bus_.drawLine(line);
        }
        lineStart = lineEnd;
        if (cycles < lineStart) {
            cycles = lineStart;
        }
    }
    bus_.catchUpApu(frameCycles, clock);
    bus_.finishAudio(frameCycles, clock);
}

FrameView SnesSystem::frame() const {
    FrameView view;
    view.rgba = bus_.ppu().pixels();
    view.width = bus_.ppu().width();
    view.height = Ppu::kHeight;
    return view;
}

AudioView SnesSystem::audio() const {
    if (bus_.audioCount() <= 0 || bus_.audioSamples() == nullptr) {
        return {};
    }
    return AudioView{bus_.audioSamples(), bus_.audioCount(), Apu::kOutputRate, bus_.audioChannels()};
}

void SnesSystem::setController(int port, std::uint8_t buttons) {
    if (port == 0) {
        bus_.setController(buttons);
    }
}

std::string SnesSystem::detailText() const {
    std::string detail = cartridge_.chip == Chip::Sa1
                             ? "SA-1, "
                             : (cartridge_.exHiRom ? "ExHiROM, " : (cartridge_.hiRom ? "HiROM, " : "LoROM, "));
    detail += standard_ == TvStandard::Pal ? "PAL" : "NTSC";
    if (cartridge_.chip == Chip::Dsp1) {
        detail += ", DSP-1";
    } else if (cartridge_.chip == Chip::Dsp2) {
        detail += ", DSP-2";
    } else if (cartridge_.chip == Chip::Dsp3) {
        detail += ", DSP-3";
    } else if (cartridge_.chip == Chip::Dsp4) {
        detail += ", DSP-4";
    } else if (cartridge_.chip == Chip::SuperFx) {
        detail += ", Super FX";
    }
    if (cpu_.stopped()) {
        char opcode[8] = {};
        std::snprintf(opcode, sizeof(opcode), "$%02X", cpu_.stopOpcode());
        detail += ". Stopped on unimplemented instruction ";
        detail += opcode;
    }
    return detail;
}

SystemInfo SnesSystem::info() const {
    SystemInfo info;
    if (!loaded_) {
        return info;
    }
    info.title = cartridge_.title;
    info.detail = detailText();
    info.batteryBacked = cartridge_.sramBytes > 0;
    return info;
}

bool SnesSystem::describeContent(
    const std::filesystem::path& path, ContentDescription& description, std::string& error) const {
    std::vector<std::uint8_t> bytes;
    if (!readAll(path, bytes, error)) {
        return false;
    }
    const Result<Cartridge> parsed = parseCartridge(bytes.data(), bytes.size());
    if (!parsed.ok()) {
        error = parsed.error().message;
        return false;
    }
    description.title = parsed.value().title;
    description.detail = boardText(parsed.value());
    description.contentId = sha256Hex(parsed.value().rom.data(), parsed.value().rom.size());
    return true;
}

std::int64_t SnesSystem::framePeriodNs() const {
    return standard_ == TvStandard::Pal ? kPalFrameNs : kNtscFrameNs;
}

bool SnesSystem::copyBattery(std::vector<std::uint8_t>& bytes) const {
    if (!loaded_ || cartridge_.sramBytes == 0) {
        bytes.clear();
        return false;
    }
    bus_.copySram(bytes);
    return !bytes.empty();
}

bool SnesSystem::restoreBattery(const std::uint8_t* data, std::size_t size, std::string& error) {
    if (!loaded_ || cartridge_.sramBytes == 0) {
        error = "This Super NES cartridge has no battery RAM.";
        return false;
    }
    if (!bus_.restoreSram(data, size)) {
        error = "The battery save does not match this cartridge and was not loaded.";
        return false;
    }
    error.clear();
    return true;
}

std::uint8_t SnesSystem::debugRead(std::uint32_t address) {
    return bus_.read24(address);
}

std::unique_ptr<IEmulatedSystem> createSnesSystem() {
    return std::make_unique<SnesSystem>();
}

}  // namespace iris::systems::snes
