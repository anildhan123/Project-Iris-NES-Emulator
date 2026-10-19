#include "iris/nes/mapper_pack.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::size_t k1K = 1024;
constexpr std::size_t k2K = 2 * 1024;
constexpr std::size_t k4K = 4 * 1024;
constexpr std::size_t k8K = 8 * 1024;
constexpr std::size_t k16K = 16 * 1024;

std::uint8_t readRam(Cartridge& cartridge, std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x6000 || cartridge.prgRam().empty()) {
        return openBus;
    }
    return cartridge.prgRam()[(address - 0x6000) % cartridge.prgRam().size()];
}

void writeRam(Cartridge& cartridge, std::uint16_t address, std::uint8_t value) {
    if (address >= 0x6000 && address < 0x8000 && !cartridge.prgRam().empty()) {
        cartridge.prgRam()[(address - 0x6000) % cartridge.prgRam().size()] = value;
    }
}

std::uint8_t readBank(const std::vector<std::uint8_t>& rom, int bank, std::size_t bankSize, std::uint16_t address) {
    if (rom.empty() || bankSize == 0) {
        return 0;
    }
    const int banks = static_cast<int>(rom.size() / bankSize);
    const int selected = banks == 0 ? 0 : ((bank % banks) + banks) % banks;
    const std::size_t offset = static_cast<std::size_t>(selected) * bankSize + (address & (bankSize - 1));
    return offset < rom.size() ? rom[offset] : 0;
}

std::uint8_t readChrBank(Cartridge& cartridge, int bank, std::size_t bankSize, std::uint16_t address) {
    return readBank(cartridge.chr(), bank, bankSize, address);
}

void writeChrRam(Cartridge& cartridge, std::uint16_t address, std::uint8_t value) {
    if (cartridge.chrRam() && !cartridge.chr().empty()) {
        cartridge.chr()[address % cartridge.chr().size()] = value;
    }
}

Mirroring mirrorFrom(int mode) {
    switch (mode & 0x03) {
    case 0:
        return Mirroring::Vertical;
    case 1:
        return Mirroring::Horizontal;
    case 2:
        return Mirroring::SingleScreenLower;
    default:
        return Mirroring::SingleScreenUpper;
    }
}

int vrcNibble(std::uint16_t address, bool swap) {
    const int a0 = (address >> (swap ? 1 : 0)) & 1;
    const int a1 = (address >> (swap ? 0 : 1)) & 1;
    return a0 | (a1 << 1);
}

void clockSimpleIrq(int& counter, int latch, bool enable, bool& pending) {
    if (!enable) {
        return;
    }
    if (counter == 0xFFFF) {
        counter = latch;
        pending = true;
    } else {
        ++counter;
    }
}

}  // namespace


Mmc5::Mmc5(Cartridge& cartridge) : cartridge_(cartridge) { reset(); }

void Mmc5::reset() {
    prgMode_ = 3;
    chrMode_ = 0;
    prg_.fill(0);
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    prg_[4] = static_cast<std::uint8_t>(banks > 0 ? banks - 1 : 0);
    prg_[3] = static_cast<std::uint8_t>(banks > 1 ? banks - 2 : 0);
    prg_[2] = static_cast<std::uint8_t>(banks > 2 ? banks - 3 : 0);
    prg_[1] = 0;
    chr_.fill(0);
    mirror_ = 0;
    mulA_ = 0xFF;
    mulB_ = 0xFF;
    irqTarget_ = 0;
    irqCounter_ = 0;
    irqScan_ = 0;
    irqDot_ = 0;
    irqEnable_ = false;
    irqPending_ = false;
    exram_.fill(0);
}

std::uint8_t Mmc5::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address >= 0x5C00 && address < 0x6000) {
        return exram_[address - 0x5C00];
    }
    if (address == 0x5204) {
        const std::uint8_t value = static_cast<std::uint8_t>((irqPending_ ? 0x80 : 0) | 0x40);
        irqPending_ = false;
        return value;
    }
    if (address == 0x5205) {
        return static_cast<std::uint8_t>((mulA_ * mulB_) & 0xFF);
    }
    if (address == 0x5206) {
        return static_cast<std::uint8_t>((mulA_ * mulB_) >> 8);
    }
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    auto bankAt = [&](int slot) {
        const int bank = prg_[static_cast<std::size_t>(slot)] & 0x7F;
        return banks == 0 ? 0 : bank % banks;
    };
    if (prgMode_ <= 1) {
        if (address < 0xC000) {
            return readBank(cartridge_.prg(), bankAt(2) >> 1, k16K, address);
        }
        return readBank(cartridge_.prg(), bankAt(4) >> 1, k16K, address);
    }
    if (prgMode_ == 2) {
        if (address < 0xC000) {
            return readBank(cartridge_.prg(), bankAt(2) >> 1, k16K, address);
        }
        if (address < 0xE000) {
            return readBank(cartridge_.prg(), bankAt(3), k8K, address);
        }
        return readBank(cartridge_.prg(), bankAt(4), k8K, address);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), bankAt(1), k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), bankAt(2), k8K, address);
    }
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), bankAt(3), k8K, address);
    }
    return readBank(cartridge_.prg(), bankAt(4), k8K, address);
}

void Mmc5::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address >= 0x5C00 && address < 0x6000) {
        exram_[address - 0x5C00] = value;
        return;
    }
    if (address == 0x5100) {
        prgMode_ = value & 0x03;
        return;
    }
    if (address == 0x5101) {
        chrMode_ = value & 0x03;
        return;
    }
    if (address == 0x5105) {
        mirror_ = value;
        return;
    }
    if (address >= 0x5113 && address <= 0x5117) {
        prg_[address - 0x5113] = value;
        return;
    }
    if (address >= 0x5120 && address <= 0x512B) {
        chr_[address - 0x5120] = value;
        return;
    }
    if (address == 0x5203) {
        irqTarget_ = value;
        return;
    }
    if (address == 0x5204) {
        irqEnable_ = (value & 0x80) != 0;
        if (!irqEnable_) {
            irqPending_ = false;
        }
        return;
    }
    if (address == 0x5205) {
        mulA_ = value;
        return;
    }
    if (address == 0x5206) {
        mulB_ = value;
        return;
    }
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
    }
}

std::uint8_t Mmc5::readChr(std::uint16_t address) {
    const std::uint16_t page = static_cast<std::uint16_t>(address & 0x1FFF);
    auto bank = [&](int index) { return static_cast<int>(chr_[static_cast<std::size_t>(index)]); };
    if (chrMode_ == 0) {
        return readChrBank(cartridge_, bank(7), k8K, page);
    }
    if (chrMode_ == 1) {
        return readChrBank(cartridge_, bank(page < 0x1000 ? 3 : 7), k4K, page);
    }
    if (chrMode_ == 2) {
        const int slot = page / 0x800;
        return readChrBank(cartridge_, bank(slot * 2 + 1), k2K, page);
    }
    return readChrBank(cartridge_, bank(page / 0x400), k1K, page);
}

void Mmc5::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address & 0x1FFF, value); }

Mirroring Mmc5::mirroring() const { return mirrorFrom(mirror_ & 0x03); }

void Mmc5::cpuCycle() {
    ++irqDot_;
    if (irqDot_ < 114) {
        return;
    }
    irqDot_ = 0;
    if (irqScan_ < 240) {
        ++irqScan_;
        if (irqEnable_ && irqScan_ == irqTarget_) {
            irqPending_ = true;
        }
    } else {
        irqScan_ = 0;
    }
}

void Mmc5::captureState(ByteWriter& writer) const {
    writer.u8(prgMode_);
    writer.u8(chrMode_);
    writer.bytes(prg_.data(), prg_.size());
    writer.bytes(chr_.data(), chr_.size());
    writer.u8(mirror_);
    writer.u8(mulA_);
    writer.u8(mulB_);
    writer.u16(static_cast<std::uint16_t>(irqTarget_));
    writer.u16(static_cast<std::uint16_t>(irqCounter_));
    writer.u16(static_cast<std::uint16_t>(irqScan_));
    writer.u16(static_cast<std::uint16_t>(irqDot_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.bytes(exram_.data(), exram_.size());
}

bool Mmc5::applyState(ByteReader& reader) {
    prgMode_ = reader.u8();
    chrMode_ = reader.u8();
    reader.bytes(prg_.data(), prg_.size());
    reader.bytes(chr_.data(), chr_.size());
    mirror_ = reader.u8();
    mulA_ = reader.u8();
    mulB_ = reader.u8();
    irqTarget_ = reader.u16();
    irqCounter_ = reader.u16();
    irqScan_ = reader.u16();
    irqDot_ = reader.u16();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    reader.bytes(exram_.data(), exram_.size());
    return reader.ok() && reader.exhausted() && prgMode_ <= 3 && chrMode_ <= 3;
}


Vrc2::Vrc2(Cartridge& cartridge, bool swapA0A1) : cartridge_(cartridge), swap_(swapA0A1) {
    prg_[0] = 0;
    prg_[1] = 0;
    for (int& bank : chr_) {
        bank = 0;
    }
}

std::uint8_t Vrc2::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), prg_[0], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_[1], k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), banks >= 2 ? banks - 2 : 0, k8K, address);
    }
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Vrc2::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int nibble = vrcNibble(address, swap_);
    if (address < 0x9000) {
        prg_[0] = value & 0x1F;
    } else if (address < 0xA000) {
        mirror_ = value & 0x01;
    } else if (address < 0xB000) {
        prg_[1] = value & 0x1F;
    } else {
        const int index = ((address - 0xB000) / 0x1000) * 2 + ((nibble & 0x02) != 0 ? 1 : 0);
        if (index >= 0 && index < 8) {
            if ((nibble & 0x01) == 0) {
                chr_[index] = (chr_[index] & 0xF0) | (value & 0x0F);
            } else {
                chr_[index] = (chr_[index] & 0x0F) | ((value & 0x0F) << 4);
            }
        }
    }
}

std::uint8_t Vrc2::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Vrc2::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Vrc2::mirroring() const { return mirror_ ? Mirroring::Horizontal : Mirroring::Vertical; }

void Vrc2::captureState(ByteWriter& writer) const {
    writer.boolean(swap_);
    writer.u8(static_cast<std::uint8_t>(prg_[0]));
    writer.u8(static_cast<std::uint8_t>(prg_[1]));
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
}

bool Vrc2::applyState(ByteReader& reader) {
    swap_ = reader.boolean();
    prg_[0] = reader.u8();
    prg_[1] = reader.u8();
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    mirror_ = reader.u8();
    return reader.ok() && reader.exhausted();
}


Vrc3::Vrc3(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Vrc3::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_, k16K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k16K, address);
}

void Vrc3::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address < 0x9000) {
        irqLatch_ = (irqLatch_ & 0xFFF0) | (value & 0x0F);
    } else if (address < 0xA000) {
        irqLatch_ = (irqLatch_ & 0xFF0F) | ((value & 0x0F) << 4);
    } else if (address < 0xB000) {
        irqLatch_ = (irqLatch_ & 0xF0FF) | ((value & 0x0F) << 8);
    } else if (address < 0xC000) {
        irqLatch_ = (irqLatch_ & 0x0FFF) | ((value & 0x0F) << 12);
    } else if (address < 0xD000) {
        irqPending_ = false;
        irqEnable_ = (value & 0x02) != 0;
        if ((value & 0x01) != 0) {
            irqCounter_ = irqLatch_;
        }
    } else if (address < 0xE000) {
        irqPending_ = false;
        irqEnable_ = (value & 0x01) != 0;
    } else if (address < 0xF000) {
        prg_ = value & 0x07;
    }
}

std::uint8_t Vrc3::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Vrc3::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Vrc3::cpuCycle() { clockSimpleIrq(irqCounter_, irqLatch_, irqEnable_, irqPending_); }

void Vrc3::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u16(static_cast<std::uint16_t>(irqLatch_));
    writer.u16(static_cast<std::uint16_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
}

bool Vrc3::applyState(ByteReader& reader) {
    prg_ = reader.u8();
    irqLatch_ = reader.u16();
    irqCounter_ = reader.u16();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    return reader.ok() && reader.exhausted();
}


Vrc4::Vrc4(Cartridge& cartridge, bool swapA0A1) : cartridge_(cartridge), swap_(swapA0A1) {}

std::uint8_t Vrc4::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), prg_[0], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_[1], k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), banks >= 2 ? banks - 2 : 0, k8K, address);
    }
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Vrc4::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int nibble = vrcNibble(address, swap_);
    if (address < 0x9000) {
        prg_[0] = value & 0x1F;
    } else if (address < 0xA000) {
        if (nibble == 0 || nibble == 1) {
            mirror_ = value & 0x03;
        } else if (nibble == 2 || nibble == 3) {
            // PRG mode bit ignored in this host-depth path; keep 8K+8K+fixed.
        }
    } else if (address < 0xB000) {
        prg_[1] = value & 0x1F;
    } else if (address < 0xF000) {
        const int index = ((address - 0xB000) / 0x1000) * 2 + ((nibble & 0x02) != 0 ? 1 : 0);
        if (index >= 0 && index < 8) {
            if ((nibble & 0x01) == 0) {
                chr_[index] = (chr_[index] & ~0x0F) | (value & 0x0F);
            } else {
                chr_[index] = (chr_[index] & ~0x1F0) | ((value & 0x1F) << 4);
            }
        }
    } else if (nibble == 0) {
        irqLatch_ = (irqLatch_ & 0xF0) | (value & 0x0F);
    } else if (nibble == 1) {
        irqLatch_ = (irqLatch_ & 0x0F) | ((value & 0x0F) << 4);
    } else if (nibble == 2) {
        irqPending_ = false;
        irqEnable_ = (value & 0x02) != 0;
        if ((value & 0x01) != 0) {
            irqCounter_ = irqLatch_;
            irqPrescale_ = 341;
        }
    } else {
        irqPending_ = false;
        irqEnable_ = (value & 0x01) != 0;
    }
}

std::uint8_t Vrc4::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Vrc4::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Vrc4::mirroring() const { return mirrorFrom(mirror_); }

void Vrc4::cpuCycle() {
    if (!irqEnable_) {
        return;
    }
    irqPrescale_ -= 3;
    if (irqPrescale_ > 0) {
        return;
    }
    irqPrescale_ += 341;
    if (irqCounter_ == 0xFF) {
        irqCounter_ = irqLatch_;
        irqPending_ = true;
    } else {
        ++irqCounter_;
    }
}

void Vrc4::captureState(ByteWriter& writer) const {
    writer.boolean(swap_);
    writer.u8(static_cast<std::uint8_t>(prg_[0]));
    writer.u8(static_cast<std::uint8_t>(prg_[1]));
    for (int bank : chr_) {
        writer.u16(static_cast<std::uint16_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
    writer.u8(static_cast<std::uint8_t>(irqLatch_));
    writer.u8(static_cast<std::uint8_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.u16(static_cast<std::uint16_t>(irqPrescale_));
}

bool Vrc4::applyState(ByteReader& reader) {
    swap_ = reader.boolean();
    prg_[0] = reader.u8();
    prg_[1] = reader.u8();
    for (int& bank : chr_) {
        bank = reader.u16();
    }
    mirror_ = reader.u8();
    irqLatch_ = reader.u8();
    irqCounter_ = reader.u8();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    irqPrescale_ = reader.u16();
    return reader.ok() && reader.exhausted();
}


Vrc6::Vrc6(Cartridge& cartridge, bool swapA0A1) : cartridge_(cartridge), swap_(swapA0A1) {}

std::uint8_t Vrc6::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg16_, k16K, address);
    }
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), prg8_, k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Vrc6::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int nibble = vrcNibble(address, swap_);
    if (address < 0x9000) {
        prg16_ = value & 0x0F;
    } else if (address < 0xA000) {
        if (nibble == 0) {
            pulse_[0] = value;
        } else if (nibble == 1) {
            pulsePeriod_[0] = (pulsePeriod_[0] & 0x0F00) | value;
        } else if (nibble == 2) {
            pulsePeriod_[0] = (pulsePeriod_[0] & 0x00FF) | ((value & 0x0F) << 8);
        }
    } else if (address < 0xB000) {
        if (nibble == 0) {
            pulse_[1] = value;
        } else if (nibble == 1) {
            pulsePeriod_[1] = (pulsePeriod_[1] & 0x0F00) | value;
        } else if (nibble == 2) {
            pulsePeriod_[1] = (pulsePeriod_[1] & 0x00FF) | ((value & 0x0F) << 8);
        }
    } else if (address < 0xC000) {
        if (nibble == 0) {
            saw_ = value;
        } else if (nibble == 1) {
            sawPeriod_ = (sawPeriod_ & 0x0F00) | value;
        } else if (nibble == 2) {
            sawPeriod_ = (sawPeriod_ & 0x00FF) | ((value & 0x0F) << 8);
        }
    } else if (address < 0xD000) {
        prg8_ = value & 0x1F;
    } else if (address < 0xF000) {
        const int index = ((address - 0xD000) / 0x1000) * 4 + nibble;
        if (index >= 0 && index < 8) {
            chr_[index] = value;
        }
    } else if (nibble == 0) {
        mirror_ = value & 0x0C;
    } else if (nibble == 1) {
        irqLatch_ = value;
    } else if (nibble == 2) {
        irqPending_ = false;
        irqEnable_ = (value & 0x02) != 0;
        if ((value & 0x01) != 0) {
            irqCounter_ = irqLatch_;
        }
    } else {
        irqPending_ = false;
        irqEnable_ = (value & 0x01) != 0;
    }
}

std::uint8_t Vrc6::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Vrc6::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Vrc6::mirroring() const {
    switch ((mirror_ >> 2) & 0x03) {
    case 0:
        return Mirroring::Vertical;
    case 1:
        return Mirroring::Horizontal;
    case 2:
        return Mirroring::SingleScreenLower;
    default:
        return Mirroring::SingleScreenUpper;
    }
}

void Vrc6::cpuCycle() {
    for (int channel = 0; channel < 2; ++channel) {
        if (pulseTimer_[channel] == 0) {
            pulseTimer_[channel] = pulsePeriod_[channel];
        } else {
            --pulseTimer_[channel];
        }
    }
    if (sawTimer_ == 0) {
        sawTimer_ = sawPeriod_;
        sawAccum_ = (sawAccum_ + (saw_ & 0x3F)) & 0xFF;
    } else {
        --sawTimer_;
    }
    if (!irqEnable_) {
        return;
    }
    if (irqCounter_ == 0xFF) {
        irqCounter_ = irqLatch_;
        irqPending_ = true;
    } else {
        ++irqCounter_;
    }
}

float Vrc6::expansionSample() const {
    float level = 0.0f;
    for (int channel = 0; channel < 2; ++channel) {
        if ((pulse_[channel] & 0x80) != 0) {
            level += static_cast<float>(pulse_[channel] & 0x0F) / 15.0f * 0.12f;
        }
    }
    level += static_cast<float>((sawAccum_ >> 3) & 0x1F) / 31.0f * 0.10f;
    return level;
}

void Vrc6::captureState(ByteWriter& writer) const {
    writer.boolean(swap_);
    writer.u8(static_cast<std::uint8_t>(prg16_));
    writer.u8(static_cast<std::uint8_t>(prg8_));
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
    writer.u8(static_cast<std::uint8_t>(irqLatch_));
    writer.u8(static_cast<std::uint8_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.u8(pulse_[0]);
    writer.u8(pulse_[1]);
    writer.u16(pulsePeriod_[0]);
    writer.u16(pulsePeriod_[1]);
    writer.u8(saw_);
    writer.u16(sawPeriod_);
}

bool Vrc6::applyState(ByteReader& reader) {
    swap_ = reader.boolean();
    prg16_ = reader.u8();
    prg8_ = reader.u8();
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    mirror_ = reader.u8();
    irqLatch_ = reader.u8();
    irqCounter_ = reader.u8();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    pulse_[0] = reader.u8();
    pulse_[1] = reader.u8();
    pulsePeriod_[0] = reader.u16();
    pulsePeriod_[1] = reader.u16();
    saw_ = reader.u8();
    sawPeriod_ = reader.u16();
    return reader.ok() && reader.exhausted();
}


Vrc7::Vrc7(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Vrc7::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), prg_[0], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_[1], k8K, address);
    }
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), prg_[2], k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Vrc7::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address == 0x9010) {
        fmReg_ = value & 0x3F;
        return;
    }
    if (address == 0x9030) {
        fmData_[fmReg_] = value;
        fmLevel_ = static_cast<float>((value & 0x0F)) / 15.0f * 0.08f;
        return;
    }
    if (address >= 0xA000 && address < 0xE000) {
        const int index = (address - 0xA000) / 0x1000;
        if (index >= 0 && index < 8) {
            chr_[index] = value;
        }
        return;
    }
    const std::uint16_t reg = static_cast<std::uint16_t>(address & 0xF010);
    if (reg == 0x8000) {
        prg_[0] = value & 0x3F;
    } else if (reg == 0x8010) {
        prg_[1] = value & 0x3F;
    } else if (reg == 0x9000) {
        prg_[2] = value & 0x3F;
    } else if (reg == 0xE000) {
        mirror_ = value & 0x03;
    } else if (reg == 0xE010) {
        irqLatch_ = value;
    } else if (reg == 0xF000) {
        irqPending_ = false;
        irqEnable_ = (value & 0x02) != 0;
        if ((value & 0x01) != 0) {
            irqCounter_ = irqLatch_;
        }
    } else if (reg == 0xF010) {
        irqPending_ = false;
        irqEnable_ = (value & 0x01) != 0;
    }
}

std::uint8_t Vrc7::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Vrc7::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Vrc7::mirroring() const { return mirrorFrom(mirror_); }

void Vrc7::cpuCycle() {
    if (!irqEnable_) {
        return;
    }
    if (irqCounter_ == 0xFF) {
        irqCounter_ = irqLatch_;
        irqPending_ = true;
    } else {
        ++irqCounter_;
    }
}

float Vrc7::expansionSample() const { return fmLevel_; }

void Vrc7::captureState(ByteWriter& writer) const {
    for (int bank : prg_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
    writer.u8(static_cast<std::uint8_t>(irqLatch_));
    writer.u8(static_cast<std::uint8_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.u8(fmReg_);
    writer.bytes(fmData_, 64);
}

bool Vrc7::applyState(ByteReader& reader) {
    for (int& bank : prg_) {
        bank = reader.u8();
    }
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    mirror_ = reader.u8();
    irqLatch_ = reader.u8();
    irqCounter_ = reader.u8();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    fmReg_ = reader.u8();
    reader.bytes(fmData_, 64);
    fmLevel_ = static_cast<float>((fmData_[fmReg_] & 0x0F)) / 15.0f * 0.08f;
    return reader.ok() && reader.exhausted();
}


Fds::Fds(Cartridge& cartridge) : cartridge_(cartridge) {
    wave_.fill(0x20);
}

std::uint8_t Fds::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address == 0x4030) {
        const std::uint8_t value = static_cast<std::uint8_t>(irqPending_ ? 0x01 : 0);
        irqPending_ = false;
        return value;
    }
    if (address == 0x4031) {
        if (cartridge_.prg().empty()) {
            return 0;
        }
        const std::uint8_t value = cartridge_.prg()[static_cast<std::size_t>(diskPtr_) % cartridge_.prg().size()];
        ++diskPtr_;
        return value;
    }
    if (address == 0x4032) {
        return 0x40;  // disk inserted, not writing
    }
    if (address == 0x4033) {
        return 0x80;  // battery OK
    }
    if (address >= 0x4040 && address < 0x4080) {
        return wave_[address - 0x4040];
    }
    if (address < 0x6000) {
        return openBus;
    }
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    // FDS BIOS/PRG window: map cartridge PRG as 32K at $8000.
    return readBank(cartridge_.prg(), 0, k8K * 4, address);
}

void Fds::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address == 0x4020) {
        irqReload_ = (irqReload_ & 0xFF00) | value;
        return;
    }
    if (address == 0x4021) {
        irqReload_ = (irqReload_ & 0x00FF) | (value << 8);
        return;
    }
    if (address == 0x4022) {
        irqEnable_ = (value & 0x02) != 0;
        if (irqEnable_) {
            irqTimer_ = irqReload_;
        }
        irqPending_ = false;
        return;
    }
    if (address == 0x4025) {
        // motor / transfer control — ignored beyond IRQ clear
        return;
    }
    if (address == 0x4080) {
        waveVol_ = value & 0x3F;
        waveEnable_ = (value & 0x80) == 0;
        return;
    }
    if (address == 0x4082) {
        wavePeriod_ = (wavePeriod_ & 0x0F00) | value;
        return;
    }
    if (address == 0x4083) {
        wavePeriod_ = (wavePeriod_ & 0x00FF) | ((value & 0x0F) << 8);
        return;
    }
    if (address >= 0x4040 && address < 0x4080) {
        wave_[address - 0x4040] = value & 0x3F;
        return;
    }
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
    }
}

std::uint8_t Fds::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Fds::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Fds::cpuCycle() {
    if (irqEnable_) {
        if (irqTimer_ == 0) {
            irqPending_ = true;
            irqTimer_ = irqReload_;
        } else {
            --irqTimer_;
        }
    }
    if (!waveEnable_ || wavePeriod_ == 0) {
        return;
    }
    if (waveTimer_ == 0) {
        waveTimer_ = wavePeriod_;
        wavePos_ = (wavePos_ + 1) & 63;
    } else {
        --waveTimer_;
    }
}

float Fds::expansionSample() const {
    if (!waveEnable_) {
        return 0.0f;
    }
    return static_cast<float>(wave_[static_cast<std::size_t>(wavePos_)] * waveVol_) / (63.0f * 63.0f) * 0.15f;
}

void Fds::captureState(ByteWriter& writer) const {
    writer.u16(static_cast<std::uint16_t>(irqTimer_));
    writer.u16(static_cast<std::uint16_t>(irqReload_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.bytes(wave_.data(), wave_.size());
    writer.u8(static_cast<std::uint8_t>(wavePos_));
    writer.u16(static_cast<std::uint16_t>(wavePeriod_));
    writer.u8(waveVol_);
    writer.boolean(waveEnable_);
    writer.u32(static_cast<std::uint32_t>(diskPtr_));
}

bool Fds::applyState(ByteReader& reader) {
    irqTimer_ = reader.u16();
    irqReload_ = reader.u16();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    reader.bytes(wave_.data(), wave_.size());
    wavePos_ = reader.u8();
    wavePeriod_ = reader.u16();
    waveVol_ = reader.u8();
    waveEnable_ = reader.boolean();
    diskPtr_ = static_cast<int>(reader.u32());
    return reader.ok() && reader.exhausted();
}


Namco163::Namco163(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Namco163::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address == 0x4800) {
        const std::uint8_t value = soundRam_[soundAddr_ & 0x7F];
        if (soundAuto_) {
            soundAddr_ = static_cast<std::uint8_t>((soundAddr_ + 1) & 0x7F);
        }
        return value;
    }
    if (address == 0x5000) {
        return static_cast<std::uint8_t>(irqCounter_ & 0xFF);
    }
    if (address == 0x5800) {
        const std::uint8_t value = static_cast<std::uint8_t>(((irqCounter_ >> 8) & 0x7F) | (irqEnable_ ? 0x80 : 0));
        return value;
    }
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), prg_[0], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_[1], k8K, address);
    }
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), prg_[2], k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Namco163::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address == 0x4800) {
        soundRam_[soundAddr_ & 0x7F] = value;
        if (soundAuto_) {
            soundAddr_ = static_cast<std::uint8_t>((soundAddr_ + 1) & 0x7F);
        }
        soundLevel_ = static_cast<float>(value & 0x0F) / 15.0f * 0.10f;
        return;
    }
    if (address == 0x5000) {
        irqCounter_ = (irqCounter_ & 0x7F00) | value;
        irqPending_ = false;
        return;
    }
    if (address == 0x5800) {
        irqCounter_ = (irqCounter_ & 0x00FF) | ((value & 0x7F) << 8);
        irqEnable_ = (value & 0x80) != 0;
        irqPending_ = false;
        return;
    }
    if (address == 0xF800) {
        soundAddr_ = value & 0x7F;
        soundAuto_ = (value & 0x80) != 0;
        return;
    }
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address < 0xC000) {
        chr_[(address - 0x8000) / 0x800] = value;
    } else if (address < 0xE000) {
        // nametable CHR selects treated as CHR banks 4-7
        chr_[4 + (address - 0xC000) / 0x800] = value;
    } else if (address < 0xE800) {
        prg_[0] = value & 0x3F;
    } else if (address < 0xF000) {
        prg_[1] = value & 0x3F;
    } else if (address < 0xF800) {
        prg_[2] = value & 0x3F;
        mirror_ = (value >> 6) & 0x03;
    }
}

std::uint8_t Namco163::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Namco163::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Namco163::mirroring() const { return mirrorFrom(mirror_); }

void Namco163::cpuCycle() {
    if (!irqEnable_) {
        return;
    }
    if (irqCounter_ == 0x7FFF) {
        irqPending_ = true;
    } else {
        ++irqCounter_;
    }
}

float Namco163::expansionSample() const { return soundLevel_; }

void Namco163::captureState(ByteWriter& writer) const {
    for (int bank : prg_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
    writer.u16(static_cast<std::uint16_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.bytes(soundRam_, 128);
    writer.u8(soundAddr_);
    writer.boolean(soundAuto_);
}

bool Namco163::applyState(ByteReader& reader) {
    for (int& bank : prg_) {
        bank = reader.u8();
    }
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    mirror_ = reader.u8();
    irqCounter_ = reader.u16();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    reader.bytes(soundRam_, 128);
    soundAddr_ = reader.u8();
    soundAuto_ = reader.boolean();
    return reader.ok() && reader.exhausted();
}

Namco210::Namco210(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Namco210::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), prg_[0], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_[1], k8K, address);
    }
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), prg_[2], k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Namco210::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address < 0xC000) {
        chr_[(address - 0x8000) / 0x800] = value;
    } else if (address < 0xE000) {
        chr_[4 + (address - 0xC000) / 0x800] = value;
    } else if (address < 0xE800) {
        prg_[0] = value & 0x3F;
    } else if (address < 0xF000) {
        prg_[1] = value & 0x3F;
    } else if (address < 0xF800) {
        prg_[2] = value & 0x3F;
        mirror_ = (value >> 6) & 0x03;
    }
}

std::uint8_t Namco210::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Namco210::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Namco210::mirroring() const { return mirrorFrom(mirror_); }

void Namco210::captureState(ByteWriter& writer) const {
    for (int bank : prg_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
}

bool Namco210::applyState(ByteReader& reader) {
    for (int& bank : prg_) {
        bank = reader.u8();
    }
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    mirror_ = reader.u8();
    return reader.ok() && reader.exhausted();
}


Sunsoft3::Sunsoft3(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Sunsoft3::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_, k16K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k16K, address);
}

void Sunsoft3::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address < 0x8800) {
        chr_[0] = value;
    } else if (address < 0x9000) {
        chr_[1] = value;
    } else if (address < 0x9800) {
        chr_[2] = value;
    } else if (address < 0xA000) {
        chr_[3] = value;
    } else if (address < 0xA800) {
        // mirroring ignored; keep cartridge default
    } else if (address < 0xB000) {
        irqEnable_ = false;
        irqPending_ = false;
    } else if (address < 0xB800) {
        irqCounter_ = (irqCounter_ & 0xFF00) | value;
    } else if (address < 0xC000) {
        irqCounter_ = (irqCounter_ & 0x00FF) | (value << 8);
        irqEnable_ = true;
        irqPending_ = false;
    } else if (address < 0xC800) {
        prg_ = value & 0x0F;
    }
}

std::uint8_t Sunsoft3::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 11) & 3], k2K, address);
}

void Sunsoft3::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Sunsoft3::cpuCycle() {
    if (!irqEnable_) {
        return;
    }
    if (irqCounter_ == 0xFFFF) {
        irqPending_ = true;
        irqEnable_ = false;
    } else {
        ++irqCounter_;
    }
}

void Sunsoft3::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u16(static_cast<std::uint16_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
}

bool Sunsoft3::applyState(ByteReader& reader) {
    prg_ = reader.u8();
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    irqCounter_ = reader.u16();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    return reader.ok() && reader.exhausted();
}

Sunsoft4::Sunsoft4(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Sunsoft4::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_, k16K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k16K, address);
}

void Sunsoft4::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address < 0x9000) {
        chr_[0] = value;
    } else if (address < 0xA000) {
        chr_[1] = value;
    } else if (address < 0xB000) {
        name_[0] = value;
    } else if (address < 0xC000) {
        name_[1] = value;
    } else if (address < 0xD000) {
        mirror_ = value & 0x03;
    } else if (address < 0xE000) {
        prg_ = value & 0x0F;
    }
}

std::uint8_t Sunsoft4::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 12) & 1], k4K, address);
}

void Sunsoft4::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Sunsoft4::mirroring() const { return mirrorFrom(mirror_); }

void Sunsoft4::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chr_[0]));
    writer.u8(static_cast<std::uint8_t>(chr_[1]));
    writer.u8(static_cast<std::uint8_t>(name_[0]));
    writer.u8(static_cast<std::uint8_t>(name_[1]));
    writer.u8(static_cast<std::uint8_t>(mirror_));
}

bool Sunsoft4::applyState(ByteReader& reader) {
    prg_ = reader.u8();
    chr_[0] = reader.u8();
    chr_[1] = reader.u8();
    name_[0] = reader.u8();
    name_[1] = reader.u8();
    mirror_ = reader.u8();
    return reader.ok() && reader.exhausted();
}

Fme7::Fme7(Cartridge& cartridge) : cartridge_(cartridge) {
    const int banks = static_cast<int>(cartridge.prg().size() / k8K);
    prg_[0] = 0;
    prg_[1] = 0;
    prg_[2] = banks >= 2 ? banks - 2 : 0;
    prg_[3] = banks >= 1 ? banks - 1 : 0;
}

std::uint8_t Fme7::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x6000) {
        return openBus;
    }
    if (address < 0x8000) {
        return readBank(cartridge_.prg(), prg_[0], k8K, address);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), prg_[1], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), prg_[2], k8K, address);
    }
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), prg_[3], k8K, address);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    return readBank(cartridge_.prg(), banks >= 1 ? banks - 1 : 0, k8K, address);
}

void Fme7::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        return;
    }
    if (address < 0xA000) {
        command_ = value & 0x0F;
        return;
    }
    if (address < 0xC000) {
        switch (command_) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            chr_[command_] = value;
            break;
        case 8:
        case 9:
        case 10:
        case 11:
            prg_[command_ - 8] = value & 0x3F;
            break;
        case 12:
            mirror_ = value & 0x03;
            break;
        case 13:
            irqPending_ = false;
            irqEnable_ = (value & 0x01) != 0;
            break;
        case 14:
            irqCounter_ = (irqCounter_ & 0xFF00) | value;
            break;
        case 15:
            irqCounter_ = (irqCounter_ & 0x00FF) | (value << 8);
            break;
        default:
            break;
        }
        return;
    }
    if (address < 0xE000) {
        ay_[command_ & 0x0F] = value;
        ayLevel_ = static_cast<float>(ay_[8] & 0x0F) / 15.0f * 0.08f;
    }
}

std::uint8_t Fme7::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, chr_[(address >> 10) & 7], k1K, address);
}

void Fme7::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Fme7::mirroring() const { return mirrorFrom(mirror_); }

void Fme7::cpuCycle() {
    if (!irqEnable_) {
        return;
    }
    if (irqCounter_ == 0) {
        irqPending_ = true;
    } else {
        --irqCounter_;
    }
}

float Fme7::expansionSample() const { return ayLevel_; }

void Fme7::captureState(ByteWriter& writer) const {
    writer.u8(command_);
    for (int bank : prg_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    for (int bank : chr_) {
        writer.u8(static_cast<std::uint8_t>(bank));
    }
    writer.u8(static_cast<std::uint8_t>(mirror_));
    writer.u16(static_cast<std::uint16_t>(irqCounter_));
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
    writer.bytes(ay_, 16);
}

bool Fme7::applyState(ByteReader& reader) {
    command_ = reader.u8();
    for (int& bank : prg_) {
        bank = reader.u8();
    }
    for (int& bank : chr_) {
        bank = reader.u8();
    }
    mirror_ = reader.u8();
    irqCounter_ = reader.u16();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    reader.bytes(ay_, 16);
    ayLevel_ = static_cast<float>(ay_[8] & 0x0F) / 15.0f * 0.08f;
    return reader.ok() && reader.exhausted();
}


std::uint8_t Cprom::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), 0, k8K * 4, address);
}

void Cprom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    bank_ = value & 0x03;
}

std::uint8_t Cprom::readChr(std::uint16_t address) {
    if (address < 0x1000) {
        return readChrBank(cartridge_, 0, k4K, address);
    }
    return readChrBank(cartridge_, bank_, k4K, address);
}

void Cprom::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Cprom::captureState(ByteWriter& writer) const { writer.u8(static_cast<std::uint8_t>(bank_)); }

bool Cprom::applyState(ByteReader& reader) {
    bank_ = reader.u8();
    return reader.ok() && reader.exhausted();
}

Mapper76::Mapper76(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Mapper76::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), banks_[6], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), banks_[7], k8K, address);
    }
    const int count = static_cast<int>(cartridge_.prg().size() / k8K);
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), count >= 2 ? count - 2 : 0, k8K, address);
    }
    return readBank(cartridge_.prg(), count >= 1 ? count - 1 : 0, k8K, address);
}

void Mapper76::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if ((address & 1) == 0) {
        select_ = value & 0x07;
    } else {
        banks_[select_] = value;
    }
}

std::uint8_t Mapper76::readChr(std::uint16_t address) {
    const int slot = (address >> 11) & 3;
    return readChrBank(cartridge_, banks_[slot + 2], k2K, address);
}

void Mapper76::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper76::captureState(ByteWriter& writer) const {
    writer.bytes(banks_, 8);
    writer.u8(select_);
}

bool Mapper76::applyState(ByteReader& reader) {
    reader.bytes(banks_, 8);
    select_ = reader.u8();
    return reader.ok() && reader.exhausted();
}

Mapper88::Mapper88(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Mapper88::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), banks_[6], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), banks_[7], k8K, address);
    }
    const int count = static_cast<int>(cartridge_.prg().size() / k8K);
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), count >= 2 ? count - 2 : 0, k8K, address);
    }
    return readBank(cartridge_.prg(), count >= 1 ? count - 1 : 0, k8K, address);
}

void Mapper88::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if ((address & 1) == 0) {
        select_ = value & 0x07;
    } else {
        banks_[select_] = value;
    }
}

std::uint8_t Mapper88::readChr(std::uint16_t address) {
    const bool high = address >= 0x1000;
    int bank = 0;
    if (address < 0x0800) {
        bank = banks_[0] & 0x3E;
    } else if (address < 0x1000) {
        bank = banks_[1] & 0x3E;
    } else if (address < 0x1400) {
        bank = banks_[2] | 0x40;
    } else if (address < 0x1800) {
        bank = banks_[3] | 0x40;
    } else if (address < 0x1C00) {
        bank = banks_[4] | 0x40;
    } else {
        bank = banks_[5] | 0x40;
    }
    (void)high;
    return readChrBank(cartridge_, bank, k1K, address);
}

void Mapper88::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper88::captureState(ByteWriter& writer) const {
    writer.bytes(banks_, 8);
    writer.u8(select_);
}

bool Mapper88::applyState(ByteReader& reader) {
    reader.bytes(banks_, 8);
    select_ = reader.u8();
    return reader.ok() && reader.exhausted();
}

Mapper95::Mapper95(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Mapper95::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xA000) {
        return readBank(cartridge_.prg(), banks_[6], k8K, address);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), banks_[7], k8K, address);
    }
    const int count = static_cast<int>(cartridge_.prg().size() / k8K);
    if (address < 0xE000) {
        return readBank(cartridge_.prg(), count >= 2 ? count - 2 : 0, k8K, address);
    }
    return readBank(cartridge_.prg(), count >= 1 ? count - 1 : 0, k8K, address);
}

void Mapper95::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if ((address & 1) == 0) {
        select_ = value & 0x07;
    } else {
        banks_[select_] = value;
    }
}

std::uint8_t Mapper95::readChr(std::uint16_t address) {
    int bank = 0;
    if (address < 0x0800) {
        bank = banks_[0] & 0xFE;
    } else if (address < 0x1000) {
        bank = banks_[1] & 0xFE;
    } else if (address < 0x1400) {
        bank = banks_[2];
    } else if (address < 0x1800) {
        bank = banks_[3];
    } else if (address < 0x1C00) {
        bank = banks_[4];
    } else {
        bank = banks_[5];
    }
    return readChrBank(cartridge_, bank, k1K, address);
}

void Mapper95::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Mapper95::mirroring() const {
    return (banks_[0] & 0x20) != 0 ? Mirroring::SingleScreenUpper : Mirroring::SingleScreenLower;
}

void Mapper95::captureState(ByteWriter& writer) const {
    writer.bytes(banks_, 8);
    writer.u8(select_);
}

bool Mapper95::applyState(ByteReader& reader) {
    reader.bytes(banks_, 8);
    select_ = reader.u8();
    return reader.ok() && reader.exhausted();
}

Mapper118::Mapper118(Cartridge& cartridge) : cartridge_(cartridge) {}

std::uint8_t Mapper118::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int count = static_cast<int>(cartridge_.prg().size() / k8K);
    int bank = count - 1;
    if (address < 0xA000) {
        bank = prgMode_ ? (count >= 2 ? count - 2 : 0) : banks_[6];
    } else if (address < 0xC000) {
        bank = banks_[7];
    } else if (address < 0xE000) {
        bank = prgMode_ ? banks_[6] : (count >= 2 ? count - 2 : 0);
    }
    return readBank(cartridge_.prg(), bank, k8K, address);
}

void Mapper118::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const bool odd = (address & 1) != 0;
    if (address < 0xA000) {
        if (!odd) {
            select_ = value;
            prgMode_ = (value & 0x40) != 0;
            chrMode_ = (value & 0x80) != 0;
        } else {
            banks_[select_ & 0x07] = value;
        }
    } else if (address < 0xC000) {
        // TxSROM ignores $A000 mirroring writes; CHR bit selects nametable.
    } else if (address < 0xE000) {
        if (!odd) {
            irqLatch_ = value;
        } else {
            irqReload_ = true;
        }
    } else {
        if (!odd) {
            irqEnable_ = false;
            irqPending_ = false;
        } else {
            irqEnable_ = true;
        }
    }
}

std::uint8_t Mapper118::readChr(std::uint16_t address) {
    int bank = 0;
    if (!chrMode_) {
        if (address < 0x0800) {
            bank = banks_[0] & 0xFE;
        } else if (address < 0x1000) {
            bank = banks_[1] & 0xFE;
        } else if (address < 0x1400) {
            bank = banks_[2];
        } else if (address < 0x1800) {
            bank = banks_[3];
        } else if (address < 0x1C00) {
            bank = banks_[4];
        } else {
            bank = banks_[5];
        }
    } else if (address < 0x0400) {
        bank = banks_[2];
    } else if (address < 0x0800) {
        bank = banks_[3];
    } else if (address < 0x0C00) {
        bank = banks_[4];
    } else if (address < 0x1000) {
        bank = banks_[5];
    } else if (address < 0x1800) {
        bank = banks_[0] & 0xFE;
    } else {
        bank = banks_[1] & 0xFE;
    }
    return readChrBank(cartridge_, bank, k1K, address);
}

void Mapper118::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Mapper118::mirroring() const {
    return (banks_[0] & 0x80) != 0 ? Mirroring::SingleScreenUpper : Mirroring::SingleScreenLower;
}

void Mapper118::onA12Rise() {
    if (irqCounter_ == 0 || irqReload_) {
        irqCounter_ = irqLatch_;
        irqReload_ = false;
    } else {
        --irqCounter_;
    }
    if (irqCounter_ == 0 && irqEnable_) {
        irqPending_ = true;
    }
}

void Mapper118::captureState(ByteWriter& writer) const {
    writer.bytes(banks_, 8);
    writer.u8(select_);
    writer.boolean(prgMode_);
    writer.boolean(chrMode_);
    writer.u8(static_cast<std::uint8_t>(irqLatch_));
    writer.u8(static_cast<std::uint8_t>(irqCounter_));
    writer.boolean(irqReload_);
    writer.boolean(irqEnable_);
    writer.boolean(irqPending_);
}

bool Mapper118::applyState(ByteReader& reader) {
    reader.bytes(banks_, 8);
    select_ = reader.u8();
    prgMode_ = reader.boolean();
    chrMode_ = reader.boolean();
    irqLatch_ = reader.u8();
    irqCounter_ = reader.u8();
    irqReload_ = reader.boolean();
    irqEnable_ = reader.boolean();
    irqPending_ = reader.boolean();
    return reader.ok() && reader.exhausted();
}

std::uint8_t Mapper180::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    if (address < 0xC000) {
        return readBank(cartridge_.prg(), 0, k16K, address);
    }
    return readBank(cartridge_.prg(), bank_, k16K, address);
}

void Mapper180::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    bank_ = value & 0x0F;
}

std::uint8_t Mapper180::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Mapper180::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper180::captureState(ByteWriter& writer) const { writer.u8(static_cast<std::uint8_t>(bank_)); }

bool Mapper180::applyState(ByteReader& reader) {
    bank_ = reader.u8();
    return reader.ok() && reader.exhausted();
}

std::uint8_t Mapper185::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), 0, cartridge_.prg().size() >= k8K * 2 ? k8K * 2 : k8K, address);
}

void Mapper185::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    chrEnable_ = (value & 0x0F) != 0 && (value & 0xF0) != 0;
}

std::uint8_t Mapper185::readChr(std::uint16_t address) {
    if (!chrEnable_) {
        return 0xFF;
    }
    return readChrBank(cartridge_, 0, k8K, address);
}

void Mapper185::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper185::captureState(ByteWriter& writer) const { writer.boolean(chrEnable_); }

bool Mapper185::applyState(ByteReader& reader) {
    chrEnable_ = reader.boolean();
    return reader.ok() && reader.exhausted();
}

std::uint8_t Mapper228::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), prg_, k16K, address);
}

void Mapper228::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    prg_ = (address >> 6) & 0x7F;
    chr_ = ((address & 0x0F) << 2) | (value & 0x03);
    vertical_ = (address & 0x2000) == 0;
}

std::uint8_t Mapper228::readChr(std::uint16_t address) { return readChrBank(cartridge_, chr_, k8K, address); }

void Mapper228::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Mapper228::mirroring() const { return vertical_ ? Mirroring::Vertical : Mirroring::Horizontal; }

void Mapper228::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chr_));
    writer.boolean(vertical_);
}

bool Mapper228::applyState(ByteReader& reader) {
    prg_ = reader.u8();
    chr_ = reader.u8();
    vertical_ = reader.boolean();
    return reader.ok() && reader.exhausted();
}

}
