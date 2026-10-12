#include "iris/nes/mmc3.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::size_t kPrgBank = 8 * 1024;

}  // namespace

void Mmc3::reset() {
    banks_.fill(0);
    select_ = 0;
    latch_ = 0;
    counter_ = 0;
    reload_ = false;
    irqEnabled_ = false;
    irq_ = false;
    horizontal_ = cartridge_.mirroring() == Mirroring::Horizontal;
}

int Mmc3::prgBank(std::uint16_t address) const {
    const int count = static_cast<int>(cartridge_.prg().size() / kPrgBank);
    const bool swap = (select_ & 0x40) != 0;
    int bank = count - 1;
    if (address < 0xA000) {
        bank = swap ? count - 2 : banks_[6];
    } else if (address < 0xC000) {
        bank = banks_[7];
    } else if (address < 0xE000) {
        bank = swap ? banks_[6] : count - 2;
    }
    if (count == 0) {
        return 0;
    }
    return bank % count;
}

std::size_t Mmc3::chrOffset(std::uint16_t address) const {
    const bool swap = (select_ & 0x80) != 0;
    const std::uint16_t page = static_cast<std::uint16_t>(address & 0x1FFF);
    std::size_t offset = 0;
    if (!swap) {
        if (page < 0x0800) {
            offset = static_cast<std::size_t>(banks_[0] & 0xFE) * 1024 + page;
        } else if (page < 0x1000) {
            offset = static_cast<std::size_t>(banks_[1] & 0xFE) * 1024 + (page - 0x0800);
        } else if (page < 0x1400) {
            offset = static_cast<std::size_t>(banks_[2]) * 1024 + (page - 0x1000);
        } else if (page < 0x1800) {
            offset = static_cast<std::size_t>(banks_[3]) * 1024 + (page - 0x1400);
        } else if (page < 0x1C00) {
            offset = static_cast<std::size_t>(banks_[4]) * 1024 + (page - 0x1800);
        } else {
            offset = static_cast<std::size_t>(banks_[5]) * 1024 + (page - 0x1C00);
        }
    } else if (page < 0x0400) {
        offset = static_cast<std::size_t>(banks_[2]) * 1024 + page;
    } else if (page < 0x0800) {
        offset = static_cast<std::size_t>(banks_[3]) * 1024 + (page - 0x0400);
    } else if (page < 0x0C00) {
        offset = static_cast<std::size_t>(banks_[4]) * 1024 + (page - 0x0800);
    } else if (page < 0x1000) {
        offset = static_cast<std::size_t>(banks_[5]) * 1024 + (page - 0x0C00);
    } else if (page < 0x1800) {
        offset = static_cast<std::size_t>(banks_[0] & 0xFE) * 1024 + (page - 0x1000);
    } else {
        offset = static_cast<std::size_t>(banks_[1] & 0xFE) * 1024 + (page - 0x1800);
    }
    return cartridge_.chr().empty() ? 0 : offset % cartridge_.chr().size();
}

std::uint8_t Mmc3::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        if (address < 0x6000 || cartridge_.prgRam().empty()) {
            return openBus;
        }
        return cartridge_.prgRam()[address - 0x6000];
    }
    const std::size_t offset = static_cast<std::size_t>(prgBank(address)) * kPrgBank + (address & 0x1FFF);
    return cartridge_.prg()[offset];
}

void Mmc3::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        if (address >= 0x6000 && !cartridge_.prgRam().empty()) {
            cartridge_.prgRam()[address - 0x6000] = value;
        }
        return;
    }
    const bool odd = (address & 0x0001) != 0;
    if (address < 0xA000) {
        if (!odd) {
            select_ = value;
        } else {
            banks_[select_ & 0x07] = value;
        }
    } else if (address < 0xC000) {
        if (!odd && cartridge_.mirroring() != Mirroring::FourScreen) {
            horizontal_ = (value & 0x01) != 0;
        }
    } else if (address < 0xE000) {
        if (!odd) {
            latch_ = value;
        } else {
            reload_ = true;
        }
    } else if (!odd) {
        irqEnabled_ = false;
        irq_ = false;
    } else {
        irqEnabled_ = true;
        irq_ = false;
    }
}

std::uint8_t Mmc3::readChr(std::uint16_t address) {
    if (cartridge_.chr().empty()) {
        return 0;
    }
    return cartridge_.chr()[chrOffset(address)];
}

void Mmc3::writeChr(std::uint16_t address, std::uint8_t value) {
    if (cartridge_.chrRam() && !cartridge_.chr().empty()) {
        cartridge_.chr()[chrOffset(address)] = value;
    }
}

Mirroring Mmc3::mirroring() const {
    if (cartridge_.mirroring() == Mirroring::FourScreen) {
        return Mirroring::FourScreen;
    }
    return horizontal_ ? Mirroring::Horizontal : Mirroring::Vertical;
}

void Mmc3::onA12Rise() {
    if (counter_ == 0 || reload_) {
        counter_ = latch_;
        reload_ = false;
    } else {
        --counter_;
    }
    if (counter_ == 0 && irqEnabled_) {
        irq_ = true;
    }
}

void Mmc3::captureState(ByteWriter& writer) const {
    for (const std::uint8_t bank : banks_) {
        writer.u8(bank);
    }
    writer.u8(select_);
    writer.u8(latch_);
    writer.u8(counter_);
    writer.boolean(reload_);
    writer.boolean(irqEnabled_);
    writer.boolean(irq_);
    writer.boolean(horizontal_);
}

bool Mmc3::applyState(ByteReader& reader) {
    std::array<std::uint8_t, 8> banks{};
    for (std::uint8_t& bank : banks) {
        bank = reader.u8();
    }
    const std::uint8_t select = reader.u8();
    const std::uint8_t latch = reader.u8();
    const std::uint8_t counter = reader.u8();
    const bool reload = reader.boolean();
    const bool irqEnabled = reader.boolean();
    const bool irq = reader.boolean();
    const bool horizontal = reader.boolean();
    if (!reader.ok() || !reader.exhausted()) {
        return false;
    }
    banks_ = banks;
    select_ = select;
    latch_ = latch;
    counter_ = counter;
    reload_ = reload;
    irqEnabled_ = irqEnabled;
    irq_ = irq;
    horizontal_ = horizontal;
    return true;
}

}  // namespace iris::systems::nes
