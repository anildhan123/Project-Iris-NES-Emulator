#include "iris/nes/mapper_factory.hpp"

#include "iris/nes/boards.hpp"
#include "iris/nes/cnrom.hpp"
#include "iris/nes/mapper_pack.hpp"
#include "iris/nes/mmc1.hpp"
#include "iris/nes/mmc3.hpp"
#include "iris/nes/nrom.hpp"
#include "iris/nes/uxrom.hpp"

namespace iris::systems::nes {
namespace {

constexpr const char* kSupported =
    "This version plays NROM, MMC1, UxROM, CNROM, MMC3, MMC5, AxROM, MMC2, MMC4, Color Dreams, BNROM, GxROM, "
    "Camerica, VRC1–VRC7, FDS, Namco 163/210, Sunsoft 3/4/FME-7, CPROM, and other common iNES boards.";

bool multipleOf(std::size_t size, std::size_t unit) { return unit != 0 && size % unit == 0; }

bool requirePrg(const Cartridge& cartridge, std::size_t minBytes, std::size_t unit, const char* name, std::string& error) {
    const std::size_t prg = cartridge.prg().size();
    if (prg < minBytes || !multipleOf(prg, unit)) {
        error = std::string(name) + " requires PRG ROM in " + std::to_string(unit / 1024) + " KiB banks.";
        return false;
    }
    return true;
}

bool requireChr(const Cartridge& cartridge, std::size_t minBytes, std::size_t unit, const char* name, std::string& error) {
    const std::size_t chr = cartridge.chr().size();
    if (chr < minBytes || !multipleOf(chr, unit)) {
        error = std::string(name) + " requires CHR in " + std::to_string(unit) + "-byte banks.";
        return false;
    }
    return true;
}

}  // namespace

const char* mapperLabel(int mapper) {
    switch (mapper) {
    case 0:
        return "NROM";
    case 1:
        return "MMC1";
    case 2:
        return "UxROM";
    case 3:
        return "CNROM";
    case 4:
        return "MMC3";
    case 5:
        return "MMC5";
    case 7:
        return "AxROM";
    case 9:
        return "MMC2";
    case 10:
        return "MMC4";
    case 11:
        return "Color Dreams";
    case 13:
        return "CPROM";
    case 19:
        return "Namco 163";
    case 20:
        return "FDS";
    case 21:
        return "VRC4";
    case 22:
        return "VRC2";
    case 23:
        return "VRC2";
    case 24:
        return "VRC6";
    case 25:
        return "VRC4";
    case 26:
        return "VRC6";
    case 34:
        return "BNROM";
    case 66:
        return "GxROM";
    case 67:
        return "Sunsoft 3";
    case 68:
        return "Sunsoft 4";
    case 69:
        return "FME-7";
    case 71:
        return "Camerica";
    case 73:
        return "VRC3";
    case 75:
        return "VRC1";
    case 76:
        return "Mapper 76";
    case 79:
        return "NINA-003";
    case 85:
        return "VRC7";
    case 87:
        return "Mapper 87";
    case 88:
        return "Mapper 88";
    case 95:
        return "Mapper 95";
    case 118:
        return "TxSROM";
    case 119:
        return "TQROM";
    case 180:
        return "Mapper 180";
    case 185:
        return "Mapper 185";
    case 206:
        return "DxROM";
    case 210:
        return "Namco 210";
    case 228:
        return "Action 52";
    case 232:
        return "BF9096";
    default:
        return "NES";
    }
}

bool mapperSupported(const Cartridge& cartridge, std::string& error) {
    const int mapper = cartridge.mapper();
    const int submapper = cartridge.submapper();
    const std::size_t prg = cartridge.prg().size();
    const std::size_t chr = cartridge.chr().size();
    switch (mapper) {
    case 0:
        if (submapper != 0) {
            error = "NROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg != 16 * 1024 && prg != 32 * 1024) {
            error = "NROM requires 16 KiB or 32 KiB of PRG ROM.";
            return false;
        }
        if (chr != 8 * 1024) {
            error = "NROM requires 8 KiB of CHR ROM or CHR RAM.";
            return false;
        }
        return true;
    case 1:
        if (submapper != 0) {
            error = "MMC1 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 16 * 1024 || prg > 512 * 1024 || !multipleOf(prg, 16 * 1024)) {
            error = "MMC1 requires 16 KiB to 512 KiB of PRG ROM in 16 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 4 * 1024)) {
            error = "MMC1 requires at least 8 KiB of CHR in 4 KiB banks.";
            return false;
        }
        return true;
    case 2:
        if (submapper > 2) {
            error = "UxROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 16 * 1024 || !multipleOf(prg, 16 * 1024)) {
            error = "UxROM requires PRG ROM in 16 KiB banks.";
            return false;
        }
        if (chr != 8 * 1024) {
            error = "UxROM requires 8 KiB of CHR ROM or CHR RAM.";
            return false;
        }
        return true;
    case 3:
        if (submapper > 1) {
            error = "CNROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg != 16 * 1024 && prg != 32 * 1024) {
            error = "CNROM requires 16 KiB or 32 KiB of PRG ROM.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 8 * 1024)) {
            error = "CNROM requires CHR in 8 KiB banks.";
            return false;
        }
        return true;
    case 4:
    case 119:
        if (submapper != 0) {
            error = "MMC3 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 16 * 1024 || !multipleOf(prg, 8 * 1024)) {
            error = "MMC3 requires at least 16 KiB of PRG ROM in 8 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 1024)) {
            error = "MMC3 requires at least 8 KiB of CHR.";
            return false;
        }
        return true;
    case 5:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "MMC5", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "MMC5", error);
    case 7:
        if (submapper != 0) {
            error = "AxROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 32 * 1024 || prg > 256 * 1024 || !multipleOf(prg, 32 * 1024)) {
            error = "AxROM requires 32 KiB to 256 KiB of PRG ROM in 32 KiB banks.";
            return false;
        }
        if (chr != 8 * 1024) {
            error = "AxROM requires 8 KiB of CHR ROM or CHR RAM.";
            return false;
        }
        return true;
    case 9:
        if (submapper != 0) {
            error = "MMC2 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 128 * 1024 || !multipleOf(prg, 8 * 1024)) {
            error = "MMC2 requires at least 128 KiB of PRG ROM in 8 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 4 * 1024)) {
            error = "MMC2 requires CHR in 4 KiB banks.";
            return false;
        }
        return true;
    case 10:
        if (submapper != 0) {
            error = "MMC4 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 128 * 1024 || !multipleOf(prg, 16 * 1024)) {
            error = "MMC4 requires at least 128 KiB of PRG ROM in 16 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 4 * 1024)) {
            error = "MMC4 requires CHR in 4 KiB banks.";
            return false;
        }
        return true;
    case 11:
        if (submapper != 0) {
            error = "Color Dreams submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 32 * 1024 || !multipleOf(prg, 32 * 1024)) {
            error = "Color Dreams requires PRG ROM in 32 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 8 * 1024)) {
            error = "Color Dreams requires CHR in 8 KiB banks.";
            return false;
        }
        return true;
    case 13:
        return requirePrg(cartridge, 16 * 1024, 16 * 1024, "CPROM", error) &&
               requireChr(cartridge, 16 * 1024, 4 * 1024, "CPROM", error);
    case 19:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "Namco 163", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "Namco 163", error);
    case 20:
        return requirePrg(cartridge, 8 * 1024, 1024, "FDS", error);
    case 21:
    case 22:
    case 23:
    case 25:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "VRC2/4", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "VRC2/4", error);
    case 24:
    case 26:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "VRC6", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "VRC6", error);
    case 34:
        if (submapper != 0 && submapper != 2) {
            error = "BNROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 32 * 1024 || !multipleOf(prg, 32 * 1024)) {
            error = "BNROM requires PRG ROM in 32 KiB banks.";
            return false;
        }
        if (chr != 8 * 1024) {
            error = "BNROM requires 8 KiB of CHR ROM or CHR RAM.";
            return false;
        }
        return true;
    case 66:
        if (submapper != 0) {
            error = "GxROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 32 * 1024 || !multipleOf(prg, 32 * 1024)) {
            error = "GxROM requires PRG ROM in 32 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 8 * 1024)) {
            error = "GxROM requires CHR in 8 KiB banks.";
            return false;
        }
        return true;
    case 67:
        return requirePrg(cartridge, 16 * 1024, 16 * 1024, "Sunsoft 3", error) &&
               requireChr(cartridge, 8 * 1024, 2 * 1024, "Sunsoft 3", error);
    case 68:
        return requirePrg(cartridge, 16 * 1024, 16 * 1024, "Sunsoft 4", error) &&
               requireChr(cartridge, 8 * 1024, 4 * 1024, "Sunsoft 4", error);
    case 69:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "FME-7", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "FME-7", error);
    case 71:
        if (submapper > 1) {
            error = "Camerica submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 16 * 1024 || !multipleOf(prg, 16 * 1024)) {
            error = "Camerica requires PRG ROM in 16 KiB banks.";
            return false;
        }
        if (chr != 8 * 1024) {
            error = "Camerica requires 8 KiB of CHR ROM or CHR RAM.";
            return false;
        }
        return true;
    case 73:
        return requirePrg(cartridge, 16 * 1024, 16 * 1024, "VRC3", error);
    case 75:
        if (submapper != 0) {
            error = "VRC1 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 16 * 1024 || !multipleOf(prg, 8 * 1024)) {
            error = "VRC1 requires PRG ROM in 8 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 4 * 1024)) {
            error = "VRC1 requires CHR in 4 KiB banks.";
            return false;
        }
        return true;
    case 76:
    case 88:
    case 95:
    case 118:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "MMC3-like", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "MMC3-like", error);
    case 79:
        if (submapper != 0) {
            error = "NINA-003 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 32 * 1024 || !multipleOf(prg, 32 * 1024)) {
            error = "NINA-003 requires PRG ROM in 32 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 8 * 1024)) {
            error = "NINA-003 requires CHR in 8 KiB banks.";
            return false;
        }
        return true;
    case 85:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "VRC7", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "VRC7", error);
    case 87:
        if (submapper != 0) {
            error = "Mapper 87 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg != 16 * 1024 && prg != 32 * 1024) {
            error = "Mapper 87 requires 16 KiB or 32 KiB of PRG ROM.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 8 * 1024)) {
            error = "Mapper 87 requires CHR in 8 KiB banks.";
            return false;
        }
        return true;
    case 180:
        return requirePrg(cartridge, 16 * 1024, 16 * 1024, "Mapper 180", error);
    case 185:
        if (prg != 16 * 1024 && prg != 32 * 1024) {
            error = "Mapper 185 requires 16 KiB or 32 KiB of PRG ROM.";
            return false;
        }
        if (chr < 8 * 1024) {
            error = "Mapper 185 requires CHR.";
            return false;
        }
        return true;
    case 206:
        if (submapper != 0) {
            error = "DxROM submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 16 * 1024 || !multipleOf(prg, 8 * 1024)) {
            error = "DxROM requires PRG ROM in 8 KiB banks.";
            return false;
        }
        if (chr < 8 * 1024 || !multipleOf(chr, 1024)) {
            error = "DxROM requires CHR in 1 KiB banks.";
            return false;
        }
        return true;
    case 210:
        return requirePrg(cartridge, 16 * 1024, 8 * 1024, "Namco 210", error) &&
               requireChr(cartridge, 8 * 1024, 1024, "Namco 210", error);
    case 228:
        return requirePrg(cartridge, 16 * 1024, 16 * 1024, "Action 52", error) &&
               requireChr(cartridge, 8 * 1024, 8 * 1024, "Action 52", error);
    case 232:
        if (submapper != 0) {
            error = "BF9096 submapper " + std::to_string(submapper) + " is not supported.";
            return false;
        }
        if (prg < 64 * 1024 || !multipleOf(prg, 16 * 1024)) {
            error = "BF9096 requires at least 64 KiB of PRG ROM in 16 KiB banks.";
            return false;
        }
        if (chr != 8 * 1024) {
            error = "BF9096 requires 8 KiB of CHR ROM or CHR RAM.";
            return false;
        }
        return true;
    default:
        error = "Mapper " + std::to_string(mapper) + " is not supported. " + kSupported;
        return false;
    }
}

std::unique_ptr<Mapper> createMapper(Cartridge& cartridge) {
    switch (cartridge.mapper()) {
    case 0:
        return std::make_unique<Nrom>(cartridge);
    case 1:
        return std::make_unique<Mmc1>(cartridge);
    case 2:
        return std::make_unique<Uxrom>(cartridge);
    case 3:
        return std::make_unique<Cnrom>(cartridge);
    case 4:
    case 119:
        return std::make_unique<Mmc3>(cartridge);
    case 5:
        return std::make_unique<Mmc5>(cartridge);
    case 7:
        return std::make_unique<Axrom>(cartridge);
    case 9:
        return std::make_unique<Mmc2>(cartridge);
    case 10:
        return std::make_unique<Mmc4>(cartridge);
    case 11:
        return std::make_unique<ColorDreams>(cartridge);
    case 13:
        return std::make_unique<Cprom>(cartridge);
    case 19:
        return std::make_unique<Namco163>(cartridge);
    case 20:
        return std::make_unique<Fds>(cartridge);
    case 21:
        return std::make_unique<Vrc4>(cartridge, true);
    case 22:
        return std::make_unique<Vrc2>(cartridge, true);
    case 23:
        return std::make_unique<Vrc2>(cartridge, false);
    case 24:
        return std::make_unique<Vrc6>(cartridge, false);
    case 25:
        return std::make_unique<Vrc4>(cartridge, false);
    case 26:
        return std::make_unique<Vrc6>(cartridge, true);
    case 34:
        return std::make_unique<Bnrom>(cartridge);
    case 66:
        return std::make_unique<Gxrom>(cartridge);
    case 67:
        return std::make_unique<Sunsoft3>(cartridge);
    case 68:
        return std::make_unique<Sunsoft4>(cartridge);
    case 69:
        return std::make_unique<Fme7>(cartridge);
    case 71:
        return std::make_unique<Camerica>(cartridge);
    case 73:
        return std::make_unique<Vrc3>(cartridge);
    case 75:
        return std::make_unique<Vrc1>(cartridge);
    case 76:
        return std::make_unique<Mapper76>(cartridge);
    case 79:
        return std::make_unique<Nina003>(cartridge);
    case 85:
        return std::make_unique<Vrc7>(cartridge);
    case 87:
        return std::make_unique<Mapper87>(cartridge);
    case 88:
        return std::make_unique<Mapper88>(cartridge);
    case 95:
        return std::make_unique<Mapper95>(cartridge);
    case 118:
        return std::make_unique<Mapper118>(cartridge);
    case 180:
        return std::make_unique<Mapper180>(cartridge);
    case 185:
        return std::make_unique<Mapper185>(cartridge);
    case 206:
        return std::make_unique<Mapper206>(cartridge);
    case 210:
        return std::make_unique<Namco210>(cartridge);
    case 228:
        return std::make_unique<Mapper228>(cartridge);
    case 232:
        return std::make_unique<Mapper232>(cartridge);
    default:
        return nullptr;
    }
}

}  // namespace iris::systems::nes
