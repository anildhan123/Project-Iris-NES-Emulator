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
    apu_.power();
    mulA_ = 0;
    dividend_ = 0;
    quotient_ = 0;
    product_ = 0;
    vtime_ = 0;
    htime_ = 0;
    dmaStall_ = 0;
    cycle_ = 0;
    nmiFlag_ = false;
    irqFlag_ = false;
    ccnt_ = 0;
    crv_ = 0;
    sa1Running_ = false;
    sa1Start_ = false;
    sa1Access_ = false;
}

void Bus::map(std::vector<std::uint8_t> rom, bool hiRom, bool exHiRom, std::size_t sramBytes, Chip chip) {
    rom_ = std::move(rom);
    hiRom_ = hiRom;
    exHiRom_ = exHiRom;
    chip_ = chip;
    sram_.assign(sramBytes, 0);
    dsp_.power();
    gsu_.power();
    gsu_.bind(&rom_);
    iram_.clear();
    bwram_.clear();
    ccnt_ = 0;
    crv_ = 0;
    sa1Running_ = false;
    sa1Start_ = false;
    sa1Access_ = false;
    if (chip_ == Chip::Sa1) {
        iram_.assign(2048, 0);
        bwram_.assign(256 * 1024, 0);
    }
}

void Bus::resetApu() {
    apu_.power();
}

void Bus::resetChips() {
    dsp_.power();
    gsu_.halt();
    ccnt_ = 0;
    sa1Running_ = false;
    sa1Start_ = false;
    sa1Access_ = false;
}

void Bus::stepGsu(int budget) {
    if (chip_ == Chip::SuperFx && gsu_.running()) {
        gsu_.run(budget);
    }
}

bool Bus::takeSa1Start() {
    const bool start = sa1Start_;
    sa1Start_ = false;
    return start;
}

std::uint8_t Bus::readSa1Reg(std::uint16_t address) const {
    if (address == 0x2200 || address == 0x2300) {
        return ccnt_;
    }
    if (address == 0x2203) {
        return static_cast<std::uint8_t>(crv_);
    }
    if (address == 0x2204) {
        return static_cast<std::uint8_t>(crv_ >> 8);
    }
    return 0;
}

void Bus::writeSa1Reg(std::uint16_t address, std::uint8_t value) {
    if (address == 0x2200) {
        const bool was = (ccnt_ & 0x20) != 0;
        ccnt_ = value;
        sa1Running_ = (value & 0x20) != 0;
        sa1Start_ = sa1Running_ && !was;
        return;
    }
    if (address == 0x2203) {
        crv_ = static_cast<std::uint16_t>((crv_ & 0xFF00) | value);
    } else if (address == 0x2204) {
        crv_ = static_cast<std::uint16_t>((crv_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
    }
}

bool Bus::readSa1(std::uint8_t bank, std::uint16_t address, std::uint8_t& value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    const auto bwByte = [&](std::uint8_t page) {
        const std::size_t index = (static_cast<std::size_t>(page & 0x0F) << 16) | address;
        value = index < bwram_.size() ? bwram_[index] : 0;
    };
    if (sa1Access_) {
        if (mapped < 0x40) {
            if (address < 0x0800 || (address >= 0x3000 && address < 0x3800)) {
                value = iram_[address & 0x07FF];
                return true;
            }
            if (address >= 0x2200 && address <= 0x23FF) {
                value = readSa1Reg(address);
                return true;
            }
            if (address >= 0x8000) {
                value = romByte(mapped, address);
                return true;
            }
            value = 0;
            return true;
        }
        if (mapped >= 0x40 && mapped <= 0x4F) {
            bwByte(mapped);
            return true;
        }
        if (bank >= 0xC0) {
            const std::size_t offset = (static_cast<std::size_t>(bank & 0x3F) << 16) | address;
            value = offset < rom_.size() ? rom_[offset] : 0;
            return true;
        }
        value = 0;
        return true;
    }
    if (mapped < 0x40 && address >= 0x2200 && address <= 0x23FF) {
        value = readSa1Reg(address);
        return true;
    }
    if (mapped < 0x40 && address >= 0x3000 && address < 0x3800) {
        value = iram_[address & 0x07FF];
        return true;
    }
    if (mapped >= 0x40 && mapped <= 0x4F) {
        bwByte(mapped);
        return true;
    }
    if (bank >= 0xC0) {
        const std::size_t offset = (static_cast<std::size_t>(bank & 0x3F) << 16) | address;
        value = offset < rom_.size() ? rom_[offset] : 0;
        return true;
    }
    return false;
}

bool Bus::writeSa1(std::uint8_t bank, std::uint16_t address, std::uint8_t value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    const auto bwStore = [&](std::uint8_t page) {
        const std::size_t index = (static_cast<std::size_t>(page & 0x0F) << 16) | address;
        if (index < bwram_.size()) {
            bwram_[index] = value;
        }
    };
    if (sa1Access_) {
        if (mapped < 0x40) {
            if (address < 0x0800 || (address >= 0x3000 && address < 0x3800)) {
                iram_[address & 0x07FF] = value;
                return true;
            }
            if (address >= 0x2200 && address <= 0x23FF) {
                writeSa1Reg(address, value);
                return true;
            }
            return true;
        }
        if (mapped >= 0x40 && mapped <= 0x4F) {
            bwStore(mapped);
            return true;
        }
        return true;
    }
    if (mapped < 0x40 && address >= 0x2200 && address <= 0x23FF) {
        writeSa1Reg(address, value);
        return true;
    }
    if (mapped < 0x40 && address >= 0x3000 && address < 0x3800) {
        iram_[address & 0x07FF] = value;
        return true;
    }
    if (mapped >= 0x40 && mapped <= 0x4F) {
        bwStore(mapped);
        return true;
    }
    return false;
}

void Bus::beginFrame() {
    apu_.beginFrame();
}

void Bus::catchUpApu(int masterCycle, int masterClock) {
    apu_.catchUp(masterCycle, masterClock);
}

void Bus::finishAudio(int masterCycles, int masterClock) {
    apu_.finishAudio(masterCycles, masterClock);
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
    bool mirror = false;
    if (exHiRom_) {
        const bool systemBank = (bank & 0x40) == 0;
        if (systemBank && address < 0x8000) {
            return 0;
        }
        offset = (static_cast<std::size_t>(bank & 0x3F) << 16) | address;
        if (bank < 0xC0) {
            offset += 0x400000;
        }
        mirror = true;
    } else if (!hiRom_) {
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
    if (mirror && !rom_.empty() && (rom_.size() & (rom_.size() - 1)) == 0) {
        offset &= rom_.size() - 1;
    }
    if (offset >= rom_.size()) {
        return 0;
    }
    return rom_[offset];
}

std::uint8_t Bus::read(std::uint8_t bank, std::uint16_t address) {
    if (chip_ == Chip::SuperFx) {
        std::uint8_t value = 0;
        if (gsu_.cpuRead(bank, address, value)) {
            return value;
        }
    }
    if (chip_ == Chip::Dsp1) {
        std::uint8_t value = 0;
        if (dsp_.read(bank, address, hiRom_, value)) {
            return value;
        }
    }
    if (chip_ == Chip::Sa1) {
        std::uint8_t value = 0;
        if (readSa1(bank, address, value)) {
            return value;
        }
    }
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
            return apu_.readPort(address & 0x03);
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
    if (bank == 0x7E) {
        return wram_[address];
    }
    if (bank == 0x7F) {
        return wram_[0x10000 + address];
    }
    if (hiRom_) {
        return romByte(bank, address);
    }
    if (address >= 0x8000) {
        return romByte(bank, address);
    }
    return 0;
}

void Bus::write(std::uint8_t bank, std::uint16_t address, std::uint8_t value) {
    if (chip_ == Chip::SuperFx && gsu_.cpuWrite(bank, address, value)) {
        return;
    }
    if (chip_ == Chip::Dsp1 && dsp_.write(bank, address, hiRom_, value)) {
        return;
    }
    if (chip_ == Chip::Sa1 && writeSa1(bank, address, value)) {
        return;
    }
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
            apu_.writePort(address & 0x03, value);
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
        if (address == 0x4207) {
            htime_ = static_cast<std::uint16_t>((htime_ & 0xFF00) | value);
            return;
        }
        if (address == 0x4208) {
            htime_ = static_cast<std::uint16_t>((htime_ & 0x00FF) | (static_cast<std::uint16_t>(value & 0x01) << 8));
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
    if (bank == 0x7E) {
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
        const int bytes = channel.count == 0 ? 65536 : channel.count;
        dmaStall_ += 8 + bytes * 8;
        int remaining = bytes;
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

int Bus::takeDmaStall() {
    const int stalled = dmaStall_;
    dmaStall_ = 0;
    return stalled;
}

void Bus::present() {}

void Bus::drawLine(int line) {
    ppu_.renderLine(line);
}

int Bus::beginHdma() {
    int cost = 0;
    for (int index = 0; index < 8; ++index) {
        DmaChannel& channel = dma_[index];
        channel.hdmaActive = false;
        if ((hdmaen_ & (1 << index)) == 0) {
            continue;
        }
        channel.table = static_cast<std::uint16_t>(channel.source);
        channel.hdmaActive = true;
        hdmaLoad(channel);
        cost += 8;
    }
    return cost;
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

int Bus::transferHdma() {
    int cost = 0;
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
            cost += 8;
        }
        channel.lineCounter = static_cast<std::uint8_t>(
            (channel.lineCounter & 0x80) | ((channel.lineCounter & 0x7F) - 1));
        channel.hdmaTransfer = (channel.lineCounter & 0x80) != 0;
        if ((channel.lineCounter & 0x7F) == 0) {
            hdmaLoad(channel);
            cost += 8;
        }
    }
    return cost;
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
