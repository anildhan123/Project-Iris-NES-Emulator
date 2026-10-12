#include "iris/nes/mmc1.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::size_t kPrgBank = 16 * 1024;
constexpr std::size_t kChr4k = 4 * 1024;

}  // namespace

void Mmc1::reset() {
    shift_ = 0;
    shiftCount_ = 0;
    ignoreWrite_ = false;
    control_ = 0x0C;
    chrBank0_ = 0;
    chrBank1_ = 0;
    prgBank_ = 0;
    prgRamDisabled_ = false;
}

int Mmc1::prgBankCount() const { return static_cast<int>(cartridge_.prg().size() / kPrgBank); }

std::size_t Mmc1::chrOffset(std::uint16_t address) const {
    if ((control_ & 0x10) == 0) {
        const std::size_t bank = static_cast<std::size_t>(chrBank0_ & 0x1E) * kChr4k;
        return bank + address;
    }
    const std::uint8_t bank = address < 0x1000 ? chrBank0_ : chrBank1_;
    return static_cast<std::size_t>(bank) * kChr4k + (address & 0x0FFF);
}

std::uint8_t Mmc1::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        if (address < 0x6000 || cartridge_.prgRam().empty() || prgRamDisabled_) {
            return openBus;
        }
        return cartridge_.prgRam()[address - 0x6000];
    }
    const int banks = prgBankCount();
    int bank = 0;
    std::uint16_t offset = 0;
    if ((control_ & 0x08) == 0) {
        bank = prgBank_ & 0x0E;
        offset = static_cast<std::uint16_t>(address - 0x8000);
    } else if ((control_ & 0x04) == 0) {
        if (address < 0xC000) {
            bank = 0;
            offset = static_cast<std::uint16_t>(address - 0x8000);
        } else {
            bank = prgBank_ % banks;
            offset = static_cast<std::uint16_t>(address - 0xC000);
        }
    } else if (address < 0xC000) {
        bank = prgBank_ % banks;
        offset = static_cast<std::uint16_t>(address - 0x8000);
    } else {
        bank = banks - 1;
        offset = static_cast<std::uint16_t>(address - 0xC000);
    }
    const std::size_t index = static_cast<std::size_t>(bank) * kPrgBank + offset;
    return cartridge_.prg()[index % cartridge_.prg().size()];
}

void Mmc1::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        if (address >= 0x6000 && !cartridge_.prgRam().empty() && !prgRamDisabled_) {
            cartridge_.prgRam()[address - 0x6000] = value;
        }
        return;
    }
    if (ignoreWrite_) {
        return;
    }
    ignoreWrite_ = true;
    if ((value & 0x80) != 0) {
        shift_ = 0;
        shiftCount_ = 0;
        control_ = static_cast<std::uint8_t>(control_ | 0x0C);
        return;
    }
    shift_ = static_cast<std::uint8_t>((shift_ >> 1) | ((value & 0x01) << 4));
    ++shiftCount_;
    if (shiftCount_ == 5) {
        loadRegister(address, shift_);
        shift_ = 0;
        shiftCount_ = 0;
    }
}

void Mmc1::loadRegister(std::uint16_t address, std::uint8_t value) {
    switch (address & 0xE000) {
    case 0x8000:
        control_ = value;
        break;
    case 0xA000:
        chrBank0_ = value;
        break;
    case 0xC000:
        chrBank1_ = value;
        break;
    default:
        prgBank_ = static_cast<std::uint8_t>(value & 0x0F);
        prgRamDisabled_ = (value & 0x10) != 0;
        break;
    }
}

std::uint8_t Mmc1::readChr(std::uint16_t address) {
    if (cartridge_.chr().empty()) {
        return 0;
    }
    return cartridge_.chr()[chrOffset(address) % cartridge_.chr().size()];
}

void Mmc1::writeChr(std::uint16_t address, std::uint8_t value) {
    if (cartridge_.chrRam() && !cartridge_.chr().empty()) {
        cartridge_.chr()[chrOffset(address) % cartridge_.chr().size()] = value;
    }
}

Mirroring Mmc1::mirroring() const {
    if (cartridge_.mirroring() == Mirroring::FourScreen) {
        return Mirroring::FourScreen;
    }
    switch (control_ & 0x03) {
    case 0:
        return Mirroring::SingleScreenLower;
    case 1:
        return Mirroring::SingleScreenUpper;
    case 2:
        return Mirroring::Vertical;
    default:
        return Mirroring::Horizontal;
    }
}

void Mmc1::captureState(ByteWriter& writer) const {
    writer.u8(shift_);
    writer.u8(static_cast<std::uint8_t>(shiftCount_));
    writer.boolean(ignoreWrite_);
    writer.u8(control_);
    writer.u8(chrBank0_);
    writer.u8(chrBank1_);
    writer.u8(prgBank_);
    writer.boolean(prgRamDisabled_);
}

bool Mmc1::applyState(ByteReader& reader) {
    const std::uint8_t shift = reader.u8();
    const std::uint8_t shiftCount = reader.u8();
    const bool ignoreWrite = reader.boolean();
    const std::uint8_t control = reader.u8();
    const std::uint8_t chrBank0 = reader.u8();
    const std::uint8_t chrBank1 = reader.u8();
    const std::uint8_t prgBank = reader.u8();
    const bool prgRamDisabled = reader.boolean();
    if (!reader.ok() || !reader.exhausted() || shiftCount > 5 || prgBank > 0x0F) {
        return false;
    }
    shift_ = shift;
    shiftCount_ = shiftCount;
    ignoreWrite_ = ignoreWrite;
    control_ = control;
    chrBank0_ = chrBank0;
    chrBank1_ = chrBank1;
    prgBank_ = prgBank;
    prgRamDisabled_ = prgRamDisabled;
    return true;
}

}  // namespace iris::systems::nes
