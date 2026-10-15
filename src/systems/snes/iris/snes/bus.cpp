#include "iris/snes/bus.hpp"

#include "iris/emulated_system.hpp"

#include <algorithm>

namespace iris::systems::snes {

void Bus::power() {
    wram_.assign(128 * 1024, 0);
    ppu_.power();
    for (DmaChannel& channel : dma_) {
        channel = DmaChannel{};
    }
    wmadd_ = 0;
    joyLow_ = 0;
    joyHigh_ = 0;
    nmitimen_ = 0;
    hdmaen_ = 0;
    apu_[0] = 0xAA;
    apu_[1] = 0xBB;
    apu_[2] = 0;
    apu_[3] = 0;
    mulA_ = 0;
    dividend_ = 0;
    quotient_ = 0;
    product_ = 0;
    vtime_ = 0;
    cycle_ = 0;
    nmiFlag_ = false;
    irqFlag_ = false;
}

void Bus::map(std::vector<std::uint8_t> rom, bool hiRom, std::size_t sramBytes) {
    rom_ = std::move(rom);
    hiRom_ = hiRom;
    sram_.assign(sramBytes, 0);
}

void Bus::setCycle(int cycle) {
    cycle_ = cycle;
}

void Bus::raiseNmi() {
    nmiFlag_ = true;
}

void Bus::raiseIrq() {
    irqFlag_ = true;
}

void Bus::copySram(std::vector<std::uint8_t>& bytes) const {
    bytes = sram_;
}

bool Bus::restoreSram(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size != sram_.size()) {
        return false;
    }
    std::copy(data, data + size, sram_.begin());
    return true;
}

bool Bus::sramIndex(std::uint8_t bank, std::uint16_t address, std::size_t& index) const {
    if (sram_.empty()) {
        return false;
    }
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    std::size_t raw = 0;
    if (!hiRom_) {
        if (mapped < 0x70 || mapped >= 0x7E || address >= 0x8000) {
            return false;
        }
        raw = (static_cast<std::size_t>(mapped & 0x0F) << 15) | address;
    } else if (mapped >= 0x20 && mapped < 0x40 && address >= 0x6000 && address < 0x8000) {
        raw = (static_cast<std::size_t>(mapped & 0x1F) << 13) | static_cast<std::size_t>(address - 0x6000);
    } else {
        return false;
    }
    index = raw % sram_.size();
    return true;
}

void Bus::setController(std::uint8_t buttons) {
    joyLow_ = 0;
    joyHigh_ = 0;
    if ((buttons & kButtonB) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x01);
    }
    if ((buttons & kButtonSelect) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x04);
    }
    if ((buttons & kButtonStart) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x08);
    }
    if ((buttons & kButtonUp) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x10);
    }
    if ((buttons & kButtonDown) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x20);
    }
    if ((buttons & kButtonLeft) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x40);
    }
    if ((buttons & kButtonRight) != 0) {
        joyLow_ = static_cast<std::uint8_t>(joyLow_ | 0x80);
    }
    if ((buttons & kButtonA) != 0) {
        joyHigh_ = static_cast<std::uint8_t>(joyHigh_ | 0x01);
    }
}

std::uint8_t Bus::romByte(std::uint8_t bank, std::uint16_t address) const {
    if (rom_.empty()) {
        return 0;
    }
    std::size_t offset = 0;
    if (!hiRom_) {
        if (address < 0x8000) {
            return 0;
        }
        offset = (static_cast<std::size_t>(bank & 0x7F) << 15) | (address & 0x7FFF);
    } else {
        const bool systemBank = (bank & 0x40) == 0;
        if (systemBank && address < 0x8000) {
            return 0;
        }
        offset = (static_cast<std::size_t>(bank & 0x3F) << 16) | address;
    }
    if (offset >= rom_.size()) {
        return 0;
    }
    return rom_[offset];
}

std::uint8_t Bus::read(std::uint8_t bank, std::uint16_t address) {
    std::size_t sram = 0;
    if (sramIndex(bank, address, sram)) {
        return sram_[sram];
    }
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    if (mapped < 0x40) {
        if (address < 0x2000) {
            return wram_[address];
        }
        if (address >= 0x2100 && address <= 0x213F) {
            return ppu_.read(address);
        }
        if (address >= 0x2180 && address <= 0x2183) {
            return readWramPort(address);
        }
        if (address >= 0x2140 && address <= 0x2143) {
            return apu_[address & 0x03];
        }
        if (address == 0x4210) {
            const std::uint8_t value = static_cast<std::uint8_t>(0x02 | (nmiFlag_ ? 0x80 : 0));
            nmiFlag_ = false;
            return value;
        }
        if (address == 0x4211) {
            const std::uint8_t value = irqFlag_ ? 0x80 : 0;
            irqFlag_ = false;
            return value;
        }
        if (address == 0x4212) {
            return cycle_ >= 1364 * 225 ? 0x80 : 0;
        }
        if (address == 0x4214) {
            return static_cast<std::uint8_t>(quotient_);
        }
        if (address == 0x4215) {
            return static_cast<std::uint8_t>(quotient_ >> 8);
        }
        if (address == 0x4216) {
            return static_cast<std::uint8_t>(product_);
        }
        if (address == 0x4217) {
            return static_cast<std::uint8_t>(product_ >> 8);
        }
        if (address == 0x4218) {
            return joyLow_;
        }
        if (address == 0x4219) {
            return joyHigh_;
        }
        if (address >= 0x8000) {
            return romByte(mapped, address);
        }
        return 0;
    }
    if (hiRom_ && mapped < 0x7E) {
        return romByte(bank, address);
    }
    if (mapped == 0x7E) {
        return wram_[address];
    }
    if (bank == 0x7F) {
        return wram_[0x10000 + address];
    }
    if (address >= 0x8000) {
        return romByte(mapped, address);
    }
    return 0;
}

void Bus::write(std::uint8_t bank, std::uint16_t address, std::uint8_t value) {
    std::size_t sram = 0;
    if (sramIndex(bank, address, sram)) {
        sram_[sram] = value;
        return;
    }
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    if (mapped < 0x40) {
        if (address < 0x2000) {
            wram_[address] = value;
            return;
        }
        if (address >= 0x2100 && address <= 0x213F) {
            ppu_.write(address, value);
            return;
        }
        if (address >= 0x2180 && address <= 0x2183) {
            writeWramPort(address, value);
            return;
        }
        if (address >= 0x2140 && address <= 0x2143) {
            apu_[address & 0x03] = value;
            return;
        }
        if (address == 0x4200) {
            nmitimen_ = value;
            return;
        }
        if (address == 0x420B) {
            startDma(value);
            return;
        }
        if (address == 0x420C) {
            hdmaen_ = value;
            return;
        }
        if (address == 0x4202) {
            mulA_ = value;
            return;
        }
        if (address == 0x4203) {
            product_ = static_cast<std::uint16_t>(mulA_ * value);
            return;
        }
        if (address == 0x4204) {
            dividend_ = static_cast<std::uint16_t>((dividend_ & 0xFF00) | value);
            return;
        }
        if (address == 0x4205) {
            dividend_ = static_cast<std::uint16_t>((dividend_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
            return;
        }
        if (address == 0x4206) {
            if (value == 0) {
                quotient_ = 0xFFFF;
                product_ = dividend_;
            } else {
                quotient_ = static_cast<std::uint16_t>(dividend_ / value);
                product_ = static_cast<std::uint16_t>(dividend_ % value);
            }
            return;
        }
        if (address == 0x4209) {
            vtime_ = static_cast<std::uint16_t>((vtime_ & 0xFF00) | value);
            return;
        }
        if (address == 0x420A) {
            vtime_ = static_cast<std::uint16_t>((vtime_ & 0x00FF) | (static_cast<std::uint16_t>(value & 0x01) << 8));
            return;
        }
        if (address >= 0x4300 && address <= 0x437F) {
            writeDma(address, value);
            return;
        }
        return;
    }
    if (mapped == 0x7E) {
        wram_[address] = value;
        return;
    }
    if (bank == 0x7F) {
        wram_[0x10000 + address] = value;
    }
}

void Bus::writeDma(std::uint16_t address, std::uint8_t value) {
    DmaChannel& channel = dma_[(address >> 4) & 7];
    switch (address & 0x0F) {
    case 0:
        channel.control = value;
        break;
    case 1:
        channel.destination = value;
        break;
    case 2:
        channel.source = (channel.source & 0xFFFF00u) | value;
        break;
    case 3:
        channel.source = (channel.source & 0xFF00FFu) | (static_cast<std::uint32_t>(value) << 8);
        break;
    case 4:
        channel.source = (channel.source & 0x00FFFFu) | (static_cast<std::uint32_t>(value) << 16);
        break;
    case 5:
        channel.count = static_cast<std::uint16_t>((channel.count & 0xFF00) | value);
        break;
    case 6:
        channel.count = static_cast<std::uint16_t>((channel.count & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        break;
    case 7:
        channel.indirectBank = value;
        break;
    case 8:
        channel.table = static_cast<std::uint16_t>((channel.table & 0xFF00) | value);
        break;
    case 9:
        channel.table = static_cast<std::uint16_t>((channel.table & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        break;
    default:
        break;
    }
}

void Bus::startDma(std::uint8_t mask) {
    static constexpr int kUnit[8][4] = {
        {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
    };
    static constexpr int kUnitSize[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    for (int index = 0; index < 8; ++index) {
        if ((mask & (1 << index)) == 0) {
            continue;
        }
        DmaChannel& channel = dma_[index];
        int remaining = channel.count == 0 ? 65536 : channel.count;
        const int mode = channel.control & 0x07;
        const int stepCode = (channel.control >> 3) & 0x03;
        const int step = stepCode == 0 ? 1 : (stepCode == 2 ? -1 : 0);
        const bool toCpu = (channel.control & 0x80) != 0;
        int unit = 0;
        while (remaining > 0) {
            const auto dest = static_cast<std::uint16_t>(0x2100 + channel.destination + kUnit[mode][unit]);
            const auto sourceBank = static_cast<std::uint8_t>(channel.source >> 16);
            const auto sourceAddress = static_cast<std::uint16_t>(channel.source);
            if (toCpu) {
                write(sourceBank, sourceAddress, read(0, dest));
            } else {
                write(0, dest, read(sourceBank, sourceAddress));
            }
            channel.source = (channel.source & 0xFF0000u) |
                             ((static_cast<std::uint32_t>(sourceAddress) + static_cast<std::uint32_t>(step)) & 0xFFFFu);
            unit = (unit + 1) % kUnitSize[mode];
            --remaining;
        }
        channel.count = 0;
    }
}

void Bus::present() {
    beginHdma();
    for (int y = 0; y < Ppu::kHeight; ++y) {
        hdmaScanline();
        ppu_.renderLine(y);
    }
}

void Bus::beginHdma() {
    for (int index = 0; index < 8; ++index) {
        DmaChannel& channel = dma_[index];
        channel.hdmaActive = false;
        if ((hdmaen_ & (1 << index)) == 0) {
            continue;
        }
        channel.table = static_cast<std::uint16_t>(channel.source);
        channel.hdmaActive = true;
        hdmaLoad(channel);
    }
}

void Bus::hdmaLoad(DmaChannel& channel) {
    const auto bank = static_cast<std::uint8_t>(channel.source >> 16);
    const std::uint8_t count = read(bank, channel.table);
    channel.table = static_cast<std::uint16_t>(channel.table + 1);
    channel.lineCounter = count;
    if (count == 0) {
        channel.hdmaActive = false;
        return;
    }
    channel.hdmaTransfer = true;
    if ((channel.control & 0x40) != 0) {
        const std::uint8_t low = read(bank, channel.table);
        channel.table = static_cast<std::uint16_t>(channel.table + 1);
        const std::uint8_t high = read(bank, channel.table);
        channel.table = static_cast<std::uint16_t>(channel.table + 1);
        channel.indirect = static_cast<std::uint16_t>(low | (high << 8));
    }
}

void Bus::hdmaTransfer(DmaChannel& channel) {
    static constexpr int kUnit[8][4] = {
        {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
    };
    static constexpr int kUnitSize[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    const int mode = channel.control & 0x07;
    const bool indirect = (channel.control & 0x40) != 0;
    const auto bank = indirect ? channel.indirectBank : static_cast<std::uint8_t>(channel.source >> 16);
    for (int unit = 0; unit < kUnitSize[mode]; ++unit) {
        const std::uint16_t address = indirect ? channel.indirect : channel.table;
        const auto dest = static_cast<std::uint16_t>(0x2100 + channel.destination + kUnit[mode][unit]);
        write(0, dest, read(bank, address));
        if (indirect) {
            channel.indirect = static_cast<std::uint16_t>(channel.indirect + 1);
        } else {
            channel.table = static_cast<std::uint16_t>(channel.table + 1);
        }
    }
}

void Bus::hdmaScanline() {
    for (int index = 0; index < 8; ++index) {
        if ((hdmaen_ & (1 << index)) == 0) {
            continue;
        }
        DmaChannel& channel = dma_[index];
        if (!channel.hdmaActive || (channel.lineCounter & 0x7F) == 0) {
            continue;
        }
        if (channel.hdmaTransfer) {
            hdmaTransfer(channel);
        }
        channel.lineCounter = static_cast<std::uint8_t>(
            (channel.lineCounter & 0x80) | ((channel.lineCounter & 0x7F) - 1));
        channel.hdmaTransfer = (channel.lineCounter & 0x80) != 0;
        if ((channel.lineCounter & 0x7F) == 0) {
            hdmaLoad(channel);
        }
    }
}

void Bus::writeWramPort(std::uint16_t address, std::uint8_t value) {
    if (address == 0x2180) {
        wram_[wmadd_ & 0x1FFFFu] = value;
        wmadd_ = (wmadd_ + 1) & 0x1FFFFu;
        return;
    }
    if (address == 0x2181) {
        wmadd_ = (wmadd_ & 0x1FF00u) | value;
        return;
    }
    if (address == 0x2182) {
        wmadd_ = (wmadd_ & 0x100FFu) | (static_cast<std::uint32_t>(value) << 8);
        return;
    }
    wmadd_ = (wmadd_ & 0x0FFFFu) | (static_cast<std::uint32_t>(value & 0x01) << 16);
}

std::uint8_t Bus::readWramPort(std::uint16_t address) {
    if (address == 0x2180) {
        const std::uint8_t value = wram_[wmadd_ & 0x1FFFFu];
        wmadd_ = (wmadd_ + 1) & 0x1FFFFu;
        return value;
    }
    if (address == 0x2181) {
        return static_cast<std::uint8_t>(wmadd_);
    }
    if (address == 0x2182) {
        return static_cast<std::uint8_t>(wmadd_ >> 8);
    }
    return static_cast<std::uint8_t>(wmadd_ >> 16);
}

std::uint8_t Bus::read24(std::uint32_t address) {
    return read(static_cast<std::uint8_t>(address >> 16), static_cast<std::uint16_t>(address & 0xFFFF));
}

}  // namespace iris::systems::snes
