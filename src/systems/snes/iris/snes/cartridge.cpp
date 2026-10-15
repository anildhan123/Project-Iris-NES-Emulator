#include "iris/snes/cartridge.hpp"

namespace iris::systems::snes {
namespace {

constexpr std::size_t kHeaderBytes = 512;
constexpr std::size_t kMinimumRom = 32 * 1024;
constexpr std::size_t kMaximumRom = 8 * 1024 * 1024;

std::string titleAt(const std::uint8_t* header) {
    std::string title;
    title.reserve(21);
    for (int index = 0; index < 21; ++index) {
        const unsigned char character = header[index];
        if (character < 32 || character > 126) {
            continue;
        }
        title.push_back(static_cast<char>(character));
    }
    while (!title.empty() && title.back() == ' ') {
        title.pop_back();
    }
    return title;
}

bool scoredHeader(const std::uint8_t* rom, std::size_t romSize, std::size_t base, int expectMap) {
    if (romSize < base + 0x20) {
        return false;
    }
    const unsigned complement = rom[base + 0x1C] | (static_cast<unsigned>(rom[base + 0x1D]) << 8);
    const unsigned checksum = rom[base + 0x1E] | (static_cast<unsigned>(rom[base + 0x1F]) << 8);
    return (complement ^ checksum) == 0xFFFF && (rom[base + 0x15] & 0x0F) == expectMap;
}

Chip chipFor(int mapNibble, int type) {
    if (mapNibble == 3 && (type == 0x32 || type == 0x33 || type == 0x34 || type == 0x35)) {
        return Chip::Sa1;
    }
    if (mapNibble == 0 && (type == 0x13 || type == 0x14 || type == 0x15 || type == 0x1A)) {
        return Chip::SuperFx;
    }
    if ((mapNibble == 0 || mapNibble == 1) && (type == 0x03 || type == 0x04)) {
        return Chip::Dsp1;
    }
    return Chip::None;
}

constexpr const char* kUnsupported =
    "This Super NES mapping is not supported yet. This build plays LoROM, HiROM, ExHiROM, DSP-1, Super FX, and SA-1.";

}  // namespace

Result<Cartridge> parseCartridge(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < kMinimumRom) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The file is too small to be a Super NES ROM.");
    }
    std::size_t offset = 0;
    if (size % 1024 == kHeaderBytes) {
        offset = kHeaderBytes;
    }
    const std::size_t romSize = size - offset;
    if (romSize < kMinimumRom || romSize > kMaximumRom) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The Super NES ROM size is not supported.");
    }
    if (romSize < 0x7FC0 + 0x30) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The file has no Super NES header.");
    }
    const std::uint8_t* rom = data + offset;
    const bool loHeader = scoredHeader(rom, romSize, 0x7FC0, 0);
    const bool hiHeader = scoredHeader(rom, romSize, 0xFFC0, 1);
    const bool saHeader = scoredHeader(rom, romSize, 0x7FC0, 3);
    const bool exHeaderLow = scoredHeader(rom, romSize, 0xFFC0, 5);
    const bool exHeaderHigh = romSize >= 0x40FFC0 + 0x20 && scoredHeader(rom, romSize, 0x40FFC0, 5);
    bool exHiRom = (exHeaderLow || exHeaderHigh) && !loHeader && !saHeader;
    bool hiRom = exHiRom || (hiHeader && !loHeader && !saHeader);
    if (!loHeader && !hiHeader && !saHeader && !exHiRom) {
        const int loMap = romSize > 0x7FD5 ? (rom[0x7FD5] & 0x0F) : -1;
        const int hiMap = romSize > 0xFFD5 ? (rom[0xFFD5] & 0x0F) : -1;
        const int loType = romSize > 0x7FD6 ? rom[0x7FD6] : -1;
        if (hiMap == 5 && romSize >= 0xFFC0 + 0x30) {
            exHiRom = true;
            hiRom = true;
        } else if (loMap == 1) {
            return Result<Cartridge>::failure(
                ErrorCode::ValidationFailure, "HiROM header does not fit in this file.");
        } else if (loMap == 3) {
            if (chipFor(3, loType) != Chip::Sa1) {
                return Result<Cartridge>::failure(ErrorCode::ValidationFailure, kUnsupported);
            }
        } else if (loMap != 0) {
            return Result<Cartridge>::failure(ErrorCode::ValidationFailure, kUnsupported);
        }
    }
    std::size_t header = hiRom ? 0xFFC0 : 0x7FC0;
    if (exHiRom && exHeaderHigh && (!exHeaderLow || romSize > 0x400000)) {
        header = 0x40FFC0;
    }
    if (romSize < header + 0x30) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The file has no Super NES header.");
    }
    Cartridge cartridge;
    cartridge.rom.assign(rom, rom + romSize);
    cartridge.hiRom = hiRom;
    cartridge.exHiRom = exHiRom;
    cartridge.chip = chipFor(rom[header + 0x15] & 0x0F, rom[header + 0x16]);
    if ((rom[header + 0x15] & 0x0F) == 3 && cartridge.chip != Chip::Sa1) {
        return Result<Cartridge>::failure(ErrorCode::ValidationFailure, kUnsupported);
    }
    cartridge.title = titleAt(rom + header);
    if (cartridge.title.empty()) {
        cartridge.title = "Super NES";
    }
    const int sramCode = rom[header + 0x18];
    if (sramCode >= 1 && sramCode <= 7) {
        cartridge.sramBytes = std::size_t{1024} << sramCode;
    }
    const int country = rom[header + 0x19];
    cartridge.standard = country >= 2 && country <= 12 ? TvStandard::Pal : TvStandard::Ntsc;
    return Result<Cartridge>::success(std::move(cartridge));
}

std::string boardText(const Cartridge& cartridge) {
    std::string text = cartridge.chip == Chip::Sa1 ? "SA-1, " : (cartridge.exHiRom ? "ExHiROM, " : (cartridge.hiRom ? "HiROM, " : "LoROM, "));
    text += cartridge.standard == TvStandard::Pal ? "PAL" : "NTSC";
    if (cartridge.chip == Chip::Dsp1) {
        text += ", DSP-1";
    } else if (cartridge.chip == Chip::SuperFx) {
        text += ", Super FX";
    }
    return text;
}

}  // namespace iris::systems::snes
