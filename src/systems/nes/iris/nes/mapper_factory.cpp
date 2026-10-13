#include "iris/nes/mapper_factory.hpp"

#include "iris/nes/boards.hpp"
#include "iris/nes/cnrom.hpp"
#include "iris/nes/mmc1.hpp"
#include "iris/nes/mmc3.hpp"
#include "iris/nes/nrom.hpp"
#include "iris/nes/uxrom.hpp"

namespace iris::systems::nes {
namespace {

constexpr const char* kSupported =
    "This version plays NROM, MMC1, UxROM, CNROM, MMC3, AxROM, Color Dreams, BNROM, GxROM, and Camerica.";

bool multipleOf(std::size_t size, std::size_t unit) { return unit != 0 && size % unit == 0; }

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
    case 7:
        return "AxROM";
    case 11:
        return "Color Dreams";
    case 34:
        return "BNROM";
    case 66:
        return "GxROM";
    case 71:
        return "Camerica";
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
        return std::make_unique<Mmc3>(cartridge);
    case 7:
        return std::make_unique<Axrom>(cartridge);
    case 11:
        return std::make_unique<ColorDreams>(cartridge);
    case 34:
        return std::make_unique<Bnrom>(cartridge);
    case 66:
        return std::make_unique<Gxrom>(cartridge);
    case 71:
        return std::make_unique<Camerica>(cartridge);
    default:
        return nullptr;
    }
}

}  // namespace iris::systems::nes
