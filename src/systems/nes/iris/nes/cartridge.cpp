#include "iris/nes/cartridge.hpp"

#include <algorithm>
#include <fstream>

namespace iris::systems::nes {
namespace {

constexpr std::size_t kMaxRomBytes = 8 * 1024 * 1024;
constexpr std::size_t kMaxChipBytes = 2 * 1024 * 1024;

Result<std::size_t> decodeChipSize(std::uint8_t low, int highNibble, int unit, const char* label) {
    if (highNibble == 0xF) {
        const int exponent = low >> 2;
        const int multiplier = low & 0x03;
        if (exponent > 22) {
            return Result<std::size_t>::failure(ErrorCode::ValidationFailure, std::string(label) + " size is too large.");
        }
        const std::size_t size = (std::size_t{1} << exponent) * static_cast<std::size_t>(multiplier * 2 + 1);
        return Result<std::size_t>::success(size);
    }
    const std::size_t units = (static_cast<std::size_t>(highNibble) << 8) | low;
    return Result<std::size_t>::success(units * static_cast<std::size_t>(unit));
}

Result<std::size_t> decodeRamShift(int shift, const char* label) {
    if (shift < 0 || shift > 16) {
        return Result<std::size_t>::failure(ErrorCode::ValidationFailure, std::string(label) + " size is not supported.");
    }
    if (shift == 0) {
        return Result<std::size_t>::success(0);
    }
    return Result<std::size_t>::success(std::size_t{64} << shift);
}

}  // namespace

Result<Cartridge> Cartridge::fromFile(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return Result<Cartridge>::failure(ErrorCode::NotFound, "The ROM file was not found.");
    }
    const auto fileSize = std::filesystem::file_size(path, error);
    if (error || fileSize > kMaxRomBytes) {
        return Result<Cartridge>::failure(
            ErrorCode::ValidationFailure, "The ROM is larger than 8 MiB or could not be read.");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<Cartridge>::failure(ErrorCode::IoFailure, "The ROM file could not be opened.");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            return Result<Cartridge>::failure(ErrorCode::IoFailure, "The ROM file could not be read.");
        }
    }
    return fromBytes(bytes.data(), bytes.size(), path.stem().string());
}

Result<Cartridge> Cartridge::fromBytes(const std::uint8_t* data, std::size_t size, std::string name) {
    if (data == nullptr || size < 16) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The file is too small to be an iNES ROM.");
    }
    if (size > kMaxRomBytes) {
        return Result<Cartridge>::failure(ErrorCode::ValidationFailure, "The ROM is larger than 8 MiB.");
    }
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The file does not start with an iNES header.");
    }

    const int flag6 = data[6];
    const int flag7 = data[7];
    const int flag7Format = flag7 & 0x0C;
    if (flag7Format == 0x0C) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The iNES header format is not recognized.");
    }
    const bool nes20 = flag7Format == 0x08;
    const bool tailClean = data[12] == 0 && data[13] == 0 && data[14] == 0 && data[15] == 0;
    if (nes20 && (flag7 & 0x03) != 0) {
        return Result<Cartridge>::failure(
            ErrorCode::ValidationFailure, "This ROM is not a standard NES or Famicom cartridge.");
    }

    int mapper = flag6 >> 4;
    int submapper = 0;
    if (nes20) {
        mapper |= (flag7 & 0xF0) | ((data[8] & 0x0F) << 8);
        submapper = data[8] >> 4;
    } else if (flag7Format == 0x00 && tailClean) {
        mapper |= flag7 & 0xF0;
    }

    const int prgHigh = nes20 ? (data[9] & 0x0F) : 0;
    const int chrHigh = nes20 ? (data[9] >> 4) : 0;
    const auto prgSize = decodeChipSize(data[4], prgHigh, 16 * 1024, "PRG ROM");
    if (!prgSize.ok()) {
        return Result<Cartridge>::failure(prgSize.error().code, prgSize.error().message);
    }
    const auto chrSize = decodeChipSize(data[5], chrHigh, 8 * 1024, "CHR ROM");
    if (!chrSize.ok()) {
        return Result<Cartridge>::failure(chrSize.error().code, chrSize.error().message);
    }
    if (prgSize.value() == 0) {
        return Result<Cartridge>::failure(ErrorCode::ValidationFailure, "The ROM has no PRG data.");
    }
    if (prgSize.value() > kMaxChipBytes || chrSize.value() > kMaxChipBytes) {
        return Result<Cartridge>::failure(ErrorCode::ValidationFailure, "PRG or CHR data is larger than 2 MiB.");
    }

    const bool trainer = (flag6 & 0x04) != 0;
    const std::size_t expected = 16 + (trainer ? 512 : 0) + prgSize.value() + chrSize.value();
    if (size < expected) {
        return Result<Cartridge>::failure(ErrorCode::ParseFailure, "The ROM is shorter than its header describes.");
    }
    if (size != expected) {
        return Result<Cartridge>::failure(
            ErrorCode::ParseFailure, "The ROM length does not match the header. Trailing bytes were rejected.");
    }

    std::size_t prgRamSize = 0;
    std::size_t chrRamSize = 0;
    if (nes20) {
        const auto prgRam = decodeRamShift(data[10] & 0x0F, "PRG RAM");
        if (!prgRam.ok()) {
            return Result<Cartridge>::failure(prgRam.error().code, prgRam.error().message);
        }
        const auto prgNv = decodeRamShift(data[10] >> 4, "PRG RAM");
        if (!prgNv.ok()) {
            return Result<Cartridge>::failure(prgNv.error().code, prgNv.error().message);
        }
        prgRamSize = std::max(prgRam.value(), prgNv.value());
        if (chrSize.value() == 0) {
            const auto chrRam = decodeRamShift(data[11] & 0x0F, "CHR RAM");
            if (!chrRam.ok()) {
                return Result<Cartridge>::failure(chrRam.error().code, chrRam.error().message);
            }
            chrRamSize = chrRam.value();
        }
    } else {
        if (data[8] == 0) {
            prgRamSize = 8 * 1024;
        } else if (data[8] > 8) {
            return Result<Cartridge>::failure(ErrorCode::ValidationFailure, "PRG RAM size is not supported.");
        } else {
            prgRamSize = static_cast<std::size_t>(data[8]) * 8 * 1024;
        }
        if (chrSize.value() == 0) {
            chrRamSize = 8 * 1024;
        }
    }

    Cartridge cart;
    cart.mapper_ = mapper;
    cart.submapper_ = submapper;
    cart.nes20_ = nes20;
    if (nes20) {
        switch (data[12] & 0x03) {
        case 1:
            cart.region_ = Region::Pal;
            break;
        case 3:
            cart.region_ = Region::Dendy;
            break;
        default:
            cart.region_ = Region::Ntsc;
            break;
        }
    }
    cart.battery_ = (flag6 & 0x02) != 0;
    cart.name_ = std::move(name);
    if ((flag6 & 0x08) != 0) {
        cart.mirroring_ = Mirroring::FourScreen;
    } else if ((flag6 & 0x01) != 0) {
        cart.mirroring_ = Mirroring::Vertical;
    } else {
        cart.mirroring_ = Mirroring::Horizontal;
    }

    std::size_t cursor = 16;
    std::vector<std::uint8_t> trainerBytes;
    if (trainer) {
        trainerBytes.assign(data + cursor, data + cursor + 512);
        cursor += 512;
        if (prgRamSize < 8 * 1024) {
            prgRamSize = 8 * 1024;
        }
    }
    cart.prg_.assign(data + cursor, data + cursor + prgSize.value());
    cursor += prgSize.value();
    cart.prgRam_.assign(prgRamSize, 0);
    if (!trainerBytes.empty()) {
        std::copy(trainerBytes.begin(), trainerBytes.end(), cart.prgRam_.begin() + 0x1000);
    }
    if (chrSize.value() > 0) {
        cart.chr_.assign(data + cursor, data + cursor + chrSize.value());
        cart.chrRam_ = false;
    } else {
        cart.chr_.assign(chrRamSize, 0);
        cart.chrRam_ = true;
    }
    return Result<Cartridge>::success(std::move(cart));
}

}  // namespace iris::systems::nes
