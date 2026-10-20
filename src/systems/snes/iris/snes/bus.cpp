#include "iris/snes/bus.hpp"

#include "iris/byte_io.hpp"
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
    for (Pad& pad : pad_) {
        pad = Pad{};
    }
    device_[0] = InputDevice::Pad;
    device_[1] = InputDevice::Pad;
    pointerX_ = 0;
    pointerY_ = 0;
    joy1Low_ = 0;
    joy1High_ = 0;
    joy2Low_ = 0;
    joy2High_ = 0;
    joy3Low_ = 0;
    joy3High_ = 0;
    joy4Low_ = 0;
    joy4High_ = 0;
    joyStrobe_ = false;
    autoJoyPending_ = false;
    joyBusyUntil_ = 0;
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
    nmiService_ = false;
    irqFlag_ = false;
    ccnt_ = 0;
    crv_ = 0;
    sa1Running_ = false;
    sa1Start_ = false;
    sa1Access_ = false;
    sa1BitData_ = 0;
    sa1BitCount_ = 0;
    sa1BitmapReg_ = 0;
    sa1ArithMode_ = 0;
    sa1Ma_ = 0;
    sa1Mb_ = 0;
    sa1ArithProduct_ = 0;
    sa1ArithRemainder_ = 0;
    sa1Cdma_ = 0;
    std::fill(std::begin(sa1Brf_), std::end(sa1Brf_), static_cast<std::uint8_t>(0));
    sa1CharRow_ = 0;
    sa1BbF_ = 0;
    sa1BitStream_ = 0;
    sa1BitLeft_ = 0;
    expansion_.power();
}

void Bus::map(std::vector<std::uint8_t> rom, bool hiRom, bool exHiRom, std::size_t sramBytes, Chip chip, bool fastRom) {
    rom_ = std::move(rom);
    hiRom_ = hiRom;
    exHiRom_ = exHiRom;
    fastRom_ = fastRom;
    chip_ = chip;
    sram_.assign(sramBytes, 0);
    dsp_.power();
    dsp_.setChip(chip_);
    gsu_.power();
    gsu_.bind(&rom_);
    expansion_.power();
    expansion_.setChip(chip_);
    expansion_.bind(&rom_);
    iram_.clear();
    bwram_.clear();
    ccnt_ = 0;
    crv_ = 0;
    sa1Running_ = false;
    sa1Start_ = false;
    sa1Access_ = false;
    sa1BitData_ = 0;
    sa1BitCount_ = 0;
    sa1BitmapReg_ = 0;
    sa1ArithMode_ = 0;
    sa1Ma_ = 0;
    sa1Mb_ = 0;
    sa1ArithProduct_ = 0;
    sa1ArithRemainder_ = 0;
    sa1Cdma_ = 0;
    std::fill(std::begin(sa1Brf_), std::end(sa1Brf_), static_cast<std::uint8_t>(0));
    sa1CharRow_ = 0;
    sa1BbF_ = 0;
    sa1BitStream_ = 0;
    sa1BitLeft_ = 0;
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
    dsp_.setChip(chip_);
    gsu_.halt();
    expansion_.power();
    expansion_.setChip(chip_);
    expansion_.bind(&rom_);
    ccnt_ = 0;
    sa1Running_ = false;
    sa1Start_ = false;
    sa1Access_ = false;
    sa1BitData_ = 0;
    sa1BitCount_ = 0;
    sa1BitmapReg_ = 0;
    sa1ArithMode_ = 0;
    sa1Ma_ = 0;
    sa1Mb_ = 0;
    sa1ArithProduct_ = 0;
    sa1ArithRemainder_ = 0;
    sa1Cdma_ = 0;
    std::fill(std::begin(sa1Brf_), std::end(sa1Brf_), static_cast<std::uint8_t>(0));
    sa1CharRow_ = 0;
    sa1BbF_ = 0;
    sa1BitStream_ = 0;
    sa1BitLeft_ = 0;
}

void Bus::tickAccess(std::uint8_t bank, std::uint16_t address, bool pageCross) {
    // Half-scale of the hardware 6/8 master-clock costs so one-frame setup
    // programs still finish. FastROM ($80–$FF, $8000+) is 3; SlowROM, WRAM,
    // and MMIO stay 4. Unaligned 16-bit page crosses add one more half-cycle.
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    int cost = 4;
    if (fastRom_ && bank >= 0x80 && address >= 0x8000) {
        cost = 3;
    }
    if (mapped < 0x40 && address < 0x8000) {
        cost = 4;
    }
    if (pageCross) {
        ++cost;
    }
    cycle_ += cost;
    lineDot_ += cost;
    // Defer APU catch-up for the rest of the instruction once any $2140–$217F
    // access happens, so 16-bit and multi-port command writes commit together.
    const bool apuIo = mapped < 0x40 && address >= 0x2140 && address <= 0x217F;
    if (apuIo) {
        apuIoHold_ = true;
    }
    if (masterClock_ > 0 && !apuIoHold_) {
        apu_.catchUp(cycle_, masterClock_);
    }
}

void Bus::stepGsu(int budget) {
    if (chip_ == Chip::SuperFx && gsu_.running()) {
        gsu_.run(budget);
    }
}

int Bus::gsuBudgetForLine() const {
    return gsu_.budgetForLine();
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
    if (address == 0x2301) {
        // Variable-length bitstream: return the next packed bits.
        return static_cast<std::uint8_t>(sa1BitData_ & 0xFF);
    }
    if (address == 0x2302) {
        return static_cast<std::uint8_t>((sa1BitData_ >> 8) & 0xFF);
    }
    if (address == 0x2303) {
        return static_cast<std::uint8_t>(sa1BitCount_);
    }
    if (address == 0x2306) {
        return static_cast<std::uint8_t>(sa1ArithProduct_);
    }
    if (address == 0x2307) {
        return static_cast<std::uint8_t>(sa1ArithProduct_ >> 8);
    }
    if (address == 0x2308) {
        return static_cast<std::uint8_t>((sa1ArithMode_ & 0x01) != 0 ? sa1ArithRemainder_
                                                                      : (sa1ArithProduct_ >> 16));
    }
    if (address == 0x2309) {
        return static_cast<std::uint8_t>((sa1ArithMode_ & 0x01) != 0 ? (sa1ArithRemainder_ >> 8)
                                                                      : (sa1ArithProduct_ >> 24));
    }
    if (address == 0x230A) {
        return 0;
    }
    if (address == 0x230E) {
        return sa1BitmapReg_;
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
        return;
    }
    if (address == 0x2204) {
        crv_ = static_cast<std::uint16_t>((crv_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        return;
    }
    if (address == 0x2225) {
        // BB F: BW-RAM bitmap format (2/4 bpp select in low bits).
        sa1BbF_ = value;
        return;
    }
    if (address == 0x2230) {
        sa1DmaControl_ = value;
        // Bit 7 enables DMA. Bit 5 selects character conversion (handled via $2240–$224F
        // for type 2, or BW-RAM→I-RAM rearrange for type 1). Normal DMA is a flat copy.
        if ((value & 0xA0) == 0xA0 && (value & 0x10) == 0) {
            // Type-1: DMAEN+CDEN without CDSEL — rearrange BW-RAM into I-RAM CHR.
            runSa1CharConv1();
            sa1DmaControl_ = static_cast<std::uint8_t>(value & 0x7F);
            return;
        }
        if ((value & 0x80) != 0 && (value & 0x20) == 0 && sa1DmaCount_ != 0 && !iram_.empty() &&
            !bwram_.empty()) {
            const bool toIram = (value & 0x04) == 0;
            int left = sa1DmaCount_ == 0 ? 65536 : sa1DmaCount_;
            std::uint32_t source = sa1DmaSource_;
            std::uint32_t dest = sa1DmaDest_;
            while (left > 0) {
                if (toIram) {
                    const std::uint8_t byte = source < bwram_.size() ? bwram_[source] : 0;
                    iram_[dest & 0x07FF] = byte;
                } else {
                    const std::uint8_t byte = iram_[source & 0x07FF];
                    if (dest < bwram_.size()) {
                        bwram_[dest] = byte;
                    }
                }
                source = (source + 1) & 0x3FFFF;
                dest = (dest + 1) & 0x3FFFF;
                --left;
            }
            sa1DmaControl_ = static_cast<std::uint8_t>(value & 0x7F);
        }
        if ((value & 0xA0) == 0xA0) {
            sa1CharRow_ = 0;
        }
        return;
    }
    if (address == 0x2231) {
        sa1Cdma_ = value;
        if ((value & 0x80) != 0) {
            sa1CharRow_ = 0;
        }
        return;
    }
    if (address == 0x2232) {
        sa1DmaSource_ = (sa1DmaSource_ & 0xFFFF00u) | value;
        return;
    }
    if (address == 0x2233) {
        sa1DmaSource_ = (sa1DmaSource_ & 0xFF00FFu) | (static_cast<std::uint32_t>(value) << 8);
        return;
    }
    if (address == 0x2234) {
        sa1DmaSource_ = (sa1DmaSource_ & 0x00FFFFu) | (static_cast<std::uint32_t>(value & 0x03) << 16);
        return;
    }
    if (address == 0x2235) {
        sa1DmaDest_ = (sa1DmaDest_ & 0xFFFF00u) | value;
        return;
    }
    if (address == 0x2236) {
        sa1DmaDest_ = (sa1DmaDest_ & 0xFF00FFu) | (static_cast<std::uint32_t>(value) << 8);
        return;
    }
    if (address == 0x2237) {
        sa1DmaDest_ = (sa1DmaDest_ & 0x00FFFFu) | (static_cast<std::uint32_t>(value & 0x03) << 16);
        return;
    }
    if (address == 0x2238) {
        sa1DmaCount_ = static_cast<std::uint16_t>((sa1DmaCount_ & 0xFF00) | value);
        return;
    }
    if (address == 0x2239) {
        sa1DmaCount_ = static_cast<std::uint16_t>((sa1DmaCount_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        return;
    }
    if (address >= 0x2240 && address <= 0x224F) {
        sa1Brf_[address - 0x2240] = value;
        // Type-2 character conversion: DMAEN + CDEN + CDSEL ($2230 bits 7,5,4).
        if (address == 0x224F && (sa1DmaControl_ & 0xB0) == 0xB0) {
            runSa1CharConv2();
        }
        return;
    }
    if (address == 0x2250) {
        sa1ArithMode_ = static_cast<std::uint8_t>(value & 0x03);
        return;
    }
    if (address == 0x2251) {
        sa1Ma_ = static_cast<std::uint16_t>((sa1Ma_ & 0xFF00) | value);
        return;
    }
    if (address == 0x2252) {
        sa1Ma_ = static_cast<std::uint16_t>((sa1Ma_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        return;
    }
    if (address == 0x2253) {
        sa1Mb_ = static_cast<std::uint16_t>((sa1Mb_ & 0xFF00) | value);
        return;
    }
    if (address == 0x2254) {
        sa1Mb_ = static_cast<std::uint16_t>((sa1Mb_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        runSa1Arithmetic();
        return;
    }
    if (address == 0x2258) {
        sa1BitData_ = (sa1BitData_ & 0xFFFFFF00u) | value;
        sa1BitStream_ = sa1BitData_;
        sa1BitLeft_ = 24;
        return;
    }
    if (address == 0x2259) {
        sa1BitData_ = (sa1BitData_ & 0xFFFF00FFu) | (static_cast<std::uint32_t>(value) << 8);
        sa1BitStream_ = sa1BitData_;
        sa1BitLeft_ = 24;
        return;
    }
    if (address == 0x225A) {
        sa1BitData_ = (sa1BitData_ & 0xFF00FFFFu) | (static_cast<std::uint32_t>(value) << 16);
        sa1BitStream_ = sa1BitData_;
        sa1BitLeft_ = 24;
        return;
    }
    if (address == 0x225B) {
        sa1BitCount_ = value & 0x1F;
        extractSa1Bits(sa1BitCount_);
        return;
    }
}

void Bus::extractSa1Bits(int count) {
    if (count <= 0 || count >= 32) {
        sa1BitmapReg_ = 0;
        return;
    }
    if (sa1BitLeft_ < count) {
        // Refill from the latched 24-bit word when the shifting window runs dry.
        sa1BitStream_ = sa1BitData_;
        sa1BitLeft_ = 24;
    }
    const std::uint32_t mask = (1u << count) - 1u;
    const std::uint32_t extracted = sa1BitStream_ & mask;
    sa1BitStream_ >>= count;
    sa1BitLeft_ -= count;
    sa1BitData_ = sa1BitStream_;
    sa1BitmapReg_ = static_cast<std::uint8_t>(extracted);
    // BW-RAM bitmap format can widen the reported pixel for 4bpp mode.
    if ((sa1BbF_ & 0x80) != 0 && (sa1BbF_ & 0x03) == 1) {
        sa1BitmapReg_ = static_cast<std::uint8_t>(extracted & 0x0F);
    }
}

void Bus::runSa1Arithmetic() {
    if ((sa1ArithMode_ & 0x01) == 0) {
        const auto product = static_cast<std::int32_t>(static_cast<std::int16_t>(sa1Ma_)) *
                             static_cast<std::int32_t>(static_cast<std::int16_t>(sa1Mb_));
        sa1ArithProduct_ = static_cast<std::uint32_t>(product);
        sa1ArithRemainder_ = 0;
        return;
    }
    if (sa1Mb_ == 0) {
        sa1ArithProduct_ = 0;
        sa1ArithRemainder_ = sa1Ma_;
        return;
    }
    sa1ArithProduct_ = static_cast<std::uint16_t>(sa1Ma_ / sa1Mb_);
    sa1ArithRemainder_ = static_cast<std::uint16_t>(sa1Ma_ % sa1Mb_);
}

void Bus::runSa1CharConv1() {
    // Type-1: eight BW-RAM source rows → one I-RAM CHR tile (2/4/8 bpp from CDMA).
    if (iram_.empty() || bwram_.empty()) {
        return;
    }
    const int colorMode = sa1Cdma_ & 0x03;
    const int bpp = colorMode == 0 ? 8 : (colorMode == 1 ? 4 : 2);
    const auto dest = static_cast<std::uint16_t>(sa1DmaDest_ & 0x07FF);
    const std::uint32_t sourceBase = sa1DmaSource_ & 0x3FFFF;
    for (int row = 0; row < 8; ++row) {
        std::uint8_t pixels[8] = {};
        for (int pixel = 0; pixel < 8; ++pixel) {
            const std::size_t index = (sourceBase + static_cast<std::uint32_t>(row * 8 + pixel)) % bwram_.size();
            pixels[pixel] = static_cast<std::uint8_t>(bwram_[index] & ((1 << bpp) - 1));
        }
        for (int plane = 0; plane < bpp && plane < 8; ++plane) {
            std::uint8_t packed = 0;
            for (int pixel = 0; pixel < 8; ++pixel) {
                if ((pixels[pixel] & (1 << plane)) != 0) {
                    packed = static_cast<std::uint8_t>(packed | (1 << (7 - pixel)));
                }
            }
            const int offset = bpp == 2 ? row * 2 + plane
                                        : (bpp == 4 ? ((plane & 1) + row * 2 + (plane & 2) * 8) : row * 8 + plane);
            iram_[(dest + offset) & 0x07FF] = packed;
        }
    }
    sa1CharRow_ = 0;
}

void Bus::runSa1CharConv2() {
    if (iram_.empty()) {
        return;
    }
    const int colorMode = sa1Cdma_ & 0x03;
    const int bpp = colorMode == 0 ? 8 : (colorMode == 1 ? 4 : 2);
    const int planes = bpp;
    const auto dest = static_cast<std::uint16_t>(sa1DmaDest_ & 0x07FF);
    const int row = sa1CharRow_ & 7;
    for (int plane = 0; plane < planes && plane < 8; ++plane) {
        std::uint8_t packed = 0;
        for (int pixel = 0; pixel < 8; ++pixel) {
            const int value = sa1Brf_[pixel] & ((1 << bpp) - 1);
            if ((value & (1 << plane)) != 0) {
                packed = static_cast<std::uint8_t>(packed | (1 << (7 - pixel)));
            }
        }
        // SNES CHR: for 2bpp, row bytes are at +row*2 / +row*2+1; 4bpp uses two pairs.
        const int offset = bpp == 2 ? row * 2 + plane
                                    : (bpp == 4 ? ((plane & 1) + row * 2 + (plane & 2) * 8) : row * 8 + plane);
        iram_[(dest + offset) & 0x07FF] = packed;
    }
    sa1CharRow_ = (sa1CharRow_ + 1) & 7;
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
    autoJoyPending_ = false;
    joyBusyUntil_ = 0;
    lineDot_ = 0;
    scanline_ = 0;
    apuIoHold_ = false;
}

void Bus::catchUpApu(int masterCycle, int masterClock) {
    apu_.catchUp(masterCycle, masterClock);
}

void Bus::finishAudio(int masterCycles, int masterClock) {
    apu_.finishAudio(masterCycles, masterClock);
}

void Bus::setCycle(int cycle, int lineDot, int scanline) {
    cycle_ = cycle;
    lineDot_ = lineDot;
    scanline_ = scanline;
    pollAutoJoy();
}

void Bus::raiseNmi() {
    nmiFlag_ = true;
    ppu_.reloadOam();
    // $4200 bit 0 enables the automatic read. Snapshot pads into JOY1/JOY2 at the
    // start (NMI handlers often read before the busy flag clears), then keep the
    // busy bit for ~4224 master clocks while the serial clocks would run.
    if ((nmitimen_ & 0x01) != 0) {
        commitAutoJoy();
        joyBusyUntil_ = cycle_ + 4224;
        autoJoyPending_ = true;
    }
}

bool Bus::autoJoyBusy() const {
    return autoJoyPending_ && cycle_ < joyBusyUntil_;
}

void Bus::pollAutoJoy() {
    if (autoJoyPending_ && cycle_ >= joyBusyUntil_) {
        autoJoyPending_ = false;
    }
}

void Bus::commitAutoJoy() {
    joy1Low_ = pad_[0].low;
    joy1High_ = pad_[0].high;
    joy2Low_ = pad_[1].low;
    joy2High_ = pad_[1].high;
    // Multitap on a host port exposes the extra pad on that port's second data line (JOY3/JOY4).
    if (device_[0] == InputDevice::Multitap) {
        joy3Low_ = pad_[2].low;
        joy3High_ = pad_[2].high;
    } else {
        joy3Low_ = 0;
        joy3High_ = 0;
    }
    if (device_[1] == InputDevice::Multitap) {
        joy4Low_ = pad_[3].low;
        joy4High_ = pad_[3].high;
    } else {
        joy4Low_ = 0;
        joy4High_ = 0;
    }
    // Auto-read leaves both ports clocked through 16 bits (further $4016/$4017 reads stay high).
    for (Pad& pad : pad_) {
        pad.shift = 0;
        pad.bitsLeft = 0;
    }
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

void Bus::encodePad(Pad& pad, std::uint8_t buttons) const {
    // $4218/$421A is A, X, L, R and a zero signature. $4219/$421B is B, Y, Select, Start, D-pad.
    pad.low = 0;
    pad.high = 0;
    if ((buttons & kButtonA) != 0) {
        pad.low = static_cast<std::uint8_t>(pad.low | 0x80);
        // The shell has one A key. Super NES games use Y for run and roll, and
        // often ignore A, so the same key holds Y as well.
        pad.high = static_cast<std::uint8_t>(pad.high | 0x40);
    }
    if ((buttons & kButtonB) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x80);
    }
    if ((buttons & kButtonSelect) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x20);
    }
    if ((buttons & kButtonStart) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x10);
    }
    if ((buttons & kButtonUp) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x08);
    }
    if ((buttons & kButtonDown) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x04);
    }
    if ((buttons & kButtonLeft) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x02);
    }
    if ((buttons & kButtonRight) != 0) {
        pad.high = static_cast<std::uint8_t>(pad.high | 0x01);
    }
}

void Bus::latchJoypad(int port) {
    if (port < 0 || port > 3) {
        return;
    }
    Pad& pad = pad_[port];
    pad.shift = static_cast<std::uint16_t>((static_cast<std::uint16_t>(pad.high) << 8) | pad.low);
    pad.bitsLeft = 16;
}

std::uint8_t Bus::readJoypad(bool portTwo) {
    const int port = portTwo ? 1 : 0;
    Pad& pad = pad_[port];
    // Bits leave most-significant first: B, then Y, Select, Start, and the D-pad, then A, X, L, R.
    // Reads after those sixteen bits stay high, which is how a game sees a connected controller.
    std::uint8_t bit = 1;
    if (joyStrobe_) {
        bit = (pad.high & 0x80) != 0 ? 1 : 0;
    } else if (pad.bitsLeft > 0) {
        bit = static_cast<std::uint8_t>((pad.shift >> 15) & 0x01);
        pad.shift = static_cast<std::uint16_t>(pad.shift << 1);
        --pad.bitsLeft;
    }
    // $4017 reports open-bus 1s in bits 2–4 alongside the serial bit.
    return portTwo ? static_cast<std::uint8_t>(0x1C | bit) : bit;
}

void Bus::setInputDevice(int port, InputDevice device) {
    if (port < 0 || port > 1) {
        return;
    }
    device_[port] = device;
}

void Bus::setPointer(int x, int y) {
    pointerX_ = x;
    pointerY_ = y;
}

void Bus::setController(int port, std::uint8_t buttons) {
    if (port < 0 || port > 3) {
        return;
    }
    const int host = port <= 1 ? port : (port == 2 ? 0 : 1);
    if (device_[host] == InputDevice::Mouse && port == host) {
        // Signature nibble in the high byte; low byte carries buttons and a coarse delta.
        pad_[port].low = static_cast<std::uint8_t>((buttons & 0x03) | ((pointerX_ & 0x0F) << 4));
        pad_[port].high = static_cast<std::uint8_t>(0x01 | ((pointerY_ & 0x0F) << 4));
    } else if ((device_[host] == InputDevice::SuperScope || device_[host] == InputDevice::Justifier) &&
               port == host) {
        encodePad(pad_[port], buttons);
        // Fire latches HV at the software pointer so light-gun games can read OPHCT/OPVCT.
        if ((buttons & kButtonA) != 0 || (buttons & kButtonB) != 0) {
            ppu_.latchCounters(pointerY_, pointerX_ * 4);
        }
    } else {
        encodePad(pad_[port], buttons);
    }
    // When auto-read is off, $4218–$421F track pads immediately (debug and polled games).
    if ((nmitimen_ & 0x01) == 0) {
        commitAutoJoy();
    }
    if (joyStrobe_ && port <= 1) {
        latchJoypad(port);
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
    // Instruction fetches are ROM or low work RAM. A cartridge with no
    // enhancement chip can return those without probing the coprocessors.
    if (chip_ == Chip::None) {
        if (bank == 0x7E) {
            return wram_[address];
        }
        if (bank == 0x7F) {
            return wram_[0x10000 + address];
        }
        const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
        if (mapped < 0x40 && address < 0x2000) {
            return wram_[address];
        }
        if (address >= 0x8000 && mapped < 0x7E) {
            return romByte(bank, address);
        }
    }
    if (chip_ == Chip::SuperFx) {
        std::uint8_t value = 0;
        if (gsu_.cpuRead(bank, address, value)) {
            return value;
        }
    }
    if (chip_ == Chip::Dsp1 || chip_ == Chip::Dsp2 || chip_ == Chip::Dsp3 || chip_ == Chip::Dsp4) {
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
    if (chip_ == Chip::Cx4 || chip_ == Chip::Sdd1 || chip_ == Chip::Spc7110 || chip_ == Chip::Obc1 ||
        chip_ == Chip::St010 || chip_ == Chip::St011 || chip_ == Chip::St018 || chip_ == Chip::Bsx) {
        std::uint8_t value = 0;
        if (expansion_.read(bank, address, hiRom_, value)) {
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
            if (address == 0x2137) {
                ppu_.latchCounters(scanline_, lineDot_);
            }
            return ppu_.read(address);
        }
        if (address >= 0x2180 && address <= 0x2183) {
            return readWramPort(address);
        }
        if (address >= 0x2140 && address <= 0x217F) {
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
            pollAutoJoy();
            // Use the current scanline, not cycle/1364 — NTSC shortens one line and
            // a modulo drifts HBlank for the rest of the frame (FF6 fade waits).
            std::uint8_t value = scanline_ >= vblankLine() ? 0x80 : 0;
            // Bit 6 is horizontal blank, the last portion of every scanline.
            if (lineDot_ >= 1096) {
                value = static_cast<std::uint8_t>(value | 0x40);
            }
            if (autoJoyBusy()) {
                value = static_cast<std::uint8_t>(value | 0x01);
            }
            return value;
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
        if (address == 0x4016 || address == 0x4017) {
            return readJoypad(address == 0x4017);
        }
        if (address == 0x4218) {
            pollAutoJoy();
            return joy1Low_;
        }
        if (address == 0x4219) {
            pollAutoJoy();
            return joy1High_;
        }
        if (address == 0x421A) {
            pollAutoJoy();
            return joy2Low_;
        }
        if (address == 0x421B) {
            pollAutoJoy();
            return joy2High_;
        }
        if (address == 0x421C) {
            pollAutoJoy();
            return joy3Low_;
        }
        if (address == 0x421D) {
            pollAutoJoy();
            return joy3High_;
        }
        if (address == 0x421E) {
            pollAutoJoy();
            return joy4Low_;
        }
        if (address == 0x421F) {
            pollAutoJoy();
            return joy4High_;
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
    if (chip_ == Chip::None) {
        if (bank == 0x7E) {
            wram_[address] = value;
            return;
        }
        if (bank == 0x7F) {
            wram_[0x10000 + address] = value;
            return;
        }
        const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
        if (mapped < 0x40 && address < 0x2000) {
            wram_[address] = value;
            return;
        }
    }
    if (chip_ == Chip::SuperFx && gsu_.cpuWrite(bank, address, value)) {
        return;
    }
    if ((chip_ == Chip::Dsp1 || chip_ == Chip::Dsp2 || chip_ == Chip::Dsp3 || chip_ == Chip::Dsp4) &&
        dsp_.write(bank, address, hiRom_, value)) {
        return;
    }
    if (chip_ == Chip::Sa1 && writeSa1(bank, address, value)) {
        return;
    }
    if ((chip_ == Chip::Cx4 || chip_ == Chip::Sdd1 || chip_ == Chip::Spc7110 || chip_ == Chip::Obc1 ||
         chip_ == Chip::St010 || chip_ == Chip::St011 || chip_ == Chip::St018 || chip_ == Chip::Bsx) &&
        expansion_.write(bank, address, hiRom_, value)) {
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
        if (address >= 0x2140 && address <= 0x217F) {
            apu_.writePort(address & 0x03, value);
            return;
        }
        if (address == 0x4016) {
            const bool strobe = (value & 0x01) != 0;
            if (strobe) {
                latchJoypad(0);
                latchJoypad(1);
            }
            joyStrobe_ = strobe;
            return;
        }
        if (address == 0x4200) {
            const std::uint8_t previous = nmitimen_;
            nmitimen_ = value;
            // Auto-read is tied to vblank, not to the NMI enable bit. A game that
            // turns the read on after this line's vblank edge still needs a latch.
            if ((previous & 0x01) == 0 && (value & 0x01) != 0 && scanline_ >= vblankLine() && !autoJoyPending_) {
                commitAutoJoy();
                joyBusyUntil_ = cycle_ + 4224;
                autoJoyPending_ = true;
            }
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

void Bus::saveState(iris::ByteWriter& out) const {
    out.bytes(wram_.data(), wram_.size());
    out.u32(static_cast<std::uint32_t>(sram_.size()));
    out.bytes(sram_.data(), sram_.size());
    out.u32(static_cast<std::uint32_t>(iram_.size()));
    out.bytes(iram_.data(), iram_.size());
    out.u32(static_cast<std::uint32_t>(bwram_.size()));
    out.bytes(bwram_.data(), bwram_.size());
    for (const DmaChannel& channel : dma_) {
        out.u8(channel.control);
        out.u8(channel.destination);
        out.u32(channel.source);
        out.u16(channel.count);
        out.u8(channel.indirectBank);
        out.u16(channel.table);
        out.u16(channel.indirect);
        out.u8(channel.lineCounter);
        out.boolean(channel.hdmaActive);
        out.boolean(channel.hdmaTransfer);
    }
    out.u32(wmadd_);
    out.u8(joy1Low_);
    out.u8(joy1High_);
    out.u8(nmitimen_);
    out.u8(hdmaen_);
    out.u8(ccnt_);
    out.u16(crv_);
    out.boolean(sa1Running_);
    out.boolean(sa1Start_);
    out.boolean(sa1Access_);
    out.u8(mulA_);
    out.u16(dividend_);
    out.u16(quotient_);
    out.u16(product_);
    out.u16(vtime_);
    out.u16(htime_);
    out.u32(static_cast<std::uint32_t>(dmaStall_));
    out.u32(static_cast<std::uint32_t>(cycle_));
    out.boolean(nmiFlag_);
    out.boolean(irqFlag_);
    out.boolean(fastRom_);
    out.u32(sa1BitData_);
    out.u32(static_cast<std::uint32_t>(sa1BitCount_));
    out.u8(sa1BitmapReg_);
    ppu_.saveState(out);
    apu_.saveState(out);
    gsu_.saveState(out);
    dsp_.saveState(out);
    expansion_.saveState(out);
}

bool Bus::loadState(iris::ByteReader& in) {
    if (wram_.size() != 128 * 1024) {
        return false;
    }
    std::vector<std::uint8_t> wram(wram_.size());
    in.bytes(wram.data(), wram.size());
    const auto sramBytes = in.u32();
    if (!in.ok() || sramBytes != sram_.size() || sramBytes > 512 * 1024) {
        return false;
    }
    std::vector<std::uint8_t> sram(sramBytes);
    in.bytes(sram.data(), sram.size());
    const auto iramBytes = in.u32();
    if (!in.ok() || iramBytes != iram_.size() || iramBytes > 2048) {
        return false;
    }
    std::vector<std::uint8_t> iram(iramBytes);
    in.bytes(iram.data(), iram.size());
    const auto bwramBytes = in.u32();
    if (!in.ok() || bwramBytes != bwram_.size() || bwramBytes > 256 * 1024) {
        return false;
    }
    std::vector<std::uint8_t> bwram(bwramBytes);
    in.bytes(bwram.data(), bwram.size());
    DmaChannel dma[8]{};
    for (DmaChannel& channel : dma) {
        channel.control = in.u8();
        channel.destination = in.u8();
        channel.source = in.u32();
        channel.count = in.u16();
        channel.indirectBank = in.u8();
        channel.table = in.u16();
        channel.indirect = in.u16();
        channel.lineCounter = in.u8();
        channel.hdmaActive = in.boolean();
        channel.hdmaTransfer = in.boolean();
    }
    const auto wmadd = in.u32();
    const auto joyLow = in.u8();
    const auto joyHigh = in.u8();
    const auto nmitimen = in.u8();
    const auto hdmaen = in.u8();
    const auto ccnt = in.u8();
    const auto crv = in.u16();
    const auto sa1Running = in.boolean();
    const auto sa1Start = in.boolean();
    const auto sa1Access = in.boolean();
    const auto mulA = in.u8();
    const auto dividend = in.u16();
    const auto quotient = in.u16();
    const auto product = in.u16();
    const auto vtime = in.u16();
    const auto htime = in.u16();
    const auto dmaStall = static_cast<int>(in.u32());
    const auto cycle = static_cast<int>(in.u32());
    const auto nmiFlag = in.boolean();
    const auto irqFlag = in.boolean();
    const auto fastRom = in.boolean();
    const auto sa1BitData = in.u32();
    const auto sa1BitCount = static_cast<int>(in.u32());
    const auto sa1BitmapReg = in.u8();
    if (!in.ok() || !ppu_.loadState(in) || !apu_.loadState(in) || !gsu_.loadState(in) || !dsp_.loadState(in) ||
        !expansion_.loadState(in)) {
        return false;
    }
    wram_ = std::move(wram);
    sram_ = std::move(sram);
    iram_ = std::move(iram);
    bwram_ = std::move(bwram);
    for (int index = 0; index < 8; ++index) {
        dma_[index] = dma[index];
    }
    wmadd_ = wmadd;
    joy1Low_ = joyLow;
    joy1High_ = joyHigh;
    joy2Low_ = 0;
    joy2High_ = 0;
    pad_[0] = Pad{};
    pad_[0].low = joyLow;
    pad_[0].high = joyHigh;
    pad_[1] = Pad{};
    joyStrobe_ = false;
    autoJoyPending_ = false;
    joyBusyUntil_ = 0;
    nmitimen_ = nmitimen;
    hdmaen_ = hdmaen;
    ccnt_ = ccnt;
    crv_ = crv;
    sa1Running_ = sa1Running;
    sa1Start_ = sa1Start;
    sa1Access_ = sa1Access;
    mulA_ = mulA;
    dividend_ = dividend;
    quotient_ = quotient;
    product_ = product;
    vtime_ = vtime;
    htime_ = htime;
    dmaStall_ = dmaStall;
    cycle_ = cycle;
    nmiFlag_ = nmiFlag;
    irqFlag_ = irqFlag;
    fastRom_ = fastRom;
    sa1BitData_ = sa1BitData;
    sa1BitCount_ = sa1BitCount;
    sa1BitmapReg_ = sa1BitmapReg;
    return true;
}

}  // namespace iris::systems::snes
