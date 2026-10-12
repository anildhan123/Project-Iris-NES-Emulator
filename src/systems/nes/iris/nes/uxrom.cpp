#include "iris/nes/uxrom.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::size_t kBank = 16 * 1024;

}  // namespace

std::uint8_t Uxrom::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        if (address < 0x6000 || cartridge_.prgRam().empty()) {
            return openBus;
        }
        return cartridge_.prgRam()[address - 0x6000];
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / kBank);
    const int bank = address < 0xC000 ? bank_ : banks - 1;
    const std::size_t offset = static_cast<std::size_t>(bank) * kBank + (address & 0x3FFF);
    return cartridge_.prg()[offset];
}

void Uxrom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        if (address >= 0x6000 && !cartridge_.prgRam().empty()) {
            cartridge_.prgRam()[address - 0x6000] = value;
        }
        return;
    }
    if (cartridge_.submapper() == 0) {
        value = static_cast<std::uint8_t>(value & readCpu(address, 0xFF));
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / kBank);
    bank_ = banks == 0 ? 0 : static_cast<int>(value) % banks;
}

std::uint8_t Uxrom::readChr(std::uint16_t address) {
    if (cartridge_.chr().empty()) {
        return 0;
    }
    return cartridge_.chr()[address & 0x1FFF];
}

void Uxrom::writeChr(std::uint16_t address, std::uint8_t value) {
    if (cartridge_.chrRam() && !cartridge_.chr().empty()) {
        cartridge_.chr()[address & 0x1FFF] = value;
    }
}

Mirroring Uxrom::mirroring() const { return cartridge_.mirroring(); }

void Uxrom::captureState(ByteWriter& writer) const { writer.u8(static_cast<std::uint8_t>(bank_)); }

bool Uxrom::applyState(ByteReader& reader) {
    const std::uint8_t bank = reader.u8();
    if (!reader.ok() || !reader.exhausted()) {
        return false;
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / (16 * 1024));
    if (bank >= banks) {
        return false;
    }
    bank_ = bank;
    return true;
}

}  // namespace iris::systems::nes
