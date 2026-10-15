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

bool SnesSystem::adopt(Cartridge cartridge, std::string& error) {
    if (cartridge.rom.size() < 32 * 1024) {
        error = "The Super NES ROM is too small.";
        return false;
    }
    cartridge_ = std::move(cartridge);
    contentId_ = sha256Hex(cartridge_.rom.data(), cartridge_.rom.size());
    standard_ = cartridge_.standard;
    bus_.power();
    bus_.map(cartridge_.rom, cartridge_.hiRom, cartridge_.sramBytes);
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
}

void SnesSystem::powerCycle() {
    if (!loaded_) {
        return;
    }
    std::vector<std::uint8_t> saved;
    bus_.copySram(saved);
    bus_.power();
    bus_.map(cartridge_.rom, cartridge_.hiRom, cartridge_.sramBytes);
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
    const int frameCycles = kDotsPerLine * lines;
    const int nmiCycle = kDotsPerLine * kVblankLine;
    int cycles = 0;
    bool signaled = false;
    bool irqSignaled = false;
    while (cycles < frameCycles && !cpu_.stopped()) {
        bus_.setCycle(cycles);
        if (!signaled && cycles >= nmiCycle && bus_.nmiEnabled()) {
            bus_.raiseNmi();
            cpu_.signalNmi();
            signaled = true;
        }
        if (!irqSignaled && bus_.virqEnabled() && cycles >= kDotsPerLine * bus_.irqLine()) {
            bus_.raiseIrq();
            cpu_.signalIrq();
            irqSignaled = true;
        }
        const int used = cpu_.step(bus_);
        if (used == 0 && cpu_.stopped()) {
            break;
        }
        cycles += used > 0 ? used : 1;
    }
    bus_.present();
}

FrameView SnesSystem::frame() const {
    FrameView view;
    view.rgba = bus_.ppu().pixels();
    view.width = Ppu::kWidth;
    view.height = Ppu::kHeight;
    return view;
}

AudioView SnesSystem::audio() const {
    return {};
}

void SnesSystem::setController(int port, std::uint8_t buttons) {
    if (port == 0) {
        bus_.setController(buttons);
    }
}

std::string SnesSystem::detailText() const {
    std::string detail = cartridge_.hiRom ? "HiROM, " : "LoROM, ";
    detail += standard_ == TvStandard::Pal ? "PAL" : "NTSC";
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
    description.detail = std::string(parsed.value().hiRom ? "HiROM, " : "LoROM, ") +
                          (parsed.value().standard == TvStandard::Pal ? "PAL" : "NTSC");
    description.contentId = sha256Hex(parsed.value().rom.data(), parsed.value().rom.size());
    return true;
}

std::int64_t SnesSystem::framePeriodNs() const {
    return standard_ == TvStandard::Pal ? kPalFrameNs : kNtscFrameNs;
}

bool SnesSystem::saveState(std::vector<std::uint8_t>& bytes, std::string_view description, std::string& error) {
    (void)bytes;
    (void)description;
    error = "Super NES save states are not in this build yet.";
    return false;
}

bool SnesSystem::loadState(const std::uint8_t* data, std::size_t size, std::string& error) {
    (void)data;
    (void)size;
    error = "Super NES save states are not in this build yet.";
    return false;
}

void SnesSystem::inspectState(const std::uint8_t* data, std::size_t size, StatePreview& preview) const {
    (void)data;
    (void)size;
    preview = StatePreview{};
    preview.error = "Super NES save states are not in this build yet.";
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
