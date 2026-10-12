#include "iris/nes/cnrom.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::size_t kChrBank = 8 * 1024;

}  // namespace

std::uint8_t Cnrom::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        if (address < 0x6000 || cartridge_.prgRam().empty()) {
            return openBus;
        }
        return cartridge_.prgRam()[address - 0x6000];
    }
    std::size_t offset = static_cast<std::size_t>(address - 0x8000);
    if (cartridge_.prg().size() == 16 * 1024) {
        offset &= 0x3FFF;
    }
    return cartridge_.prg()[offset];
}

void Cnrom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        if (address >= 0x6000 && !cartridge_.prgRam().empty()) {
            cartridge_.prgRam()[address - 0x6000] = value;
        }
        return;
    }
    if (cartridge_.submapper() == 0) {
        value = static_cast<std::uint8_t>(value & readCpu(address, 0xFF));
    }
    const int banks = static_cast<int>(cartridge_.chr().size() / kChrBank);
    bank_ = banks == 0 ? 0 : static_cast<int>(value) % banks;
}

std::uint8_t Cnrom::readChr(std::uint16_t address) {
    if (cartridge_.chr().empty()) {
        return 0;
    }
    const std::size_t offset = static_cast<std::size_t>(bank_) * kChrBank + (address & 0x1FFF);
    return cartridge_.chr()[offset];
}

void Cnrom::writeChr(std::uint16_t address, std::uint8_t value) {
    if (cartridge_.chrRam() && !cartridge_.chr().empty()) {
        cartridge_.chr()[static_cast<std::size_t>(bank_) * kChrBank + (address & 0x1FFF)] = value;
    }
}

}  // namespace iris::systems::nes
