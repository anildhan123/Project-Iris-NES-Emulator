#include "iris/nes/nrom.hpp"

namespace iris::systems::nes {

std::uint8_t Nrom::readCpu(std::uint16_t address, std::uint8_t openBus) {
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

void Nrom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address >= 0x6000 && address < 0x8000 && !cartridge_.prgRam().empty()) {
        cartridge_.prgRam()[address - 0x6000] = value;
    }
}

std::uint8_t Nrom::readChr(std::uint16_t address) {
    if (cartridge_.chr().empty()) {
        return 0;
    }
    return cartridge_.chr()[address & 0x1FFF];
}

void Nrom::writeChr(std::uint16_t address, std::uint8_t value) {
    if (cartridge_.chrRam() && !cartridge_.chr().empty()) {
        cartridge_.chr()[address & 0x1FFF] = value;
    }
}

}  // namespace iris::systems::nes
