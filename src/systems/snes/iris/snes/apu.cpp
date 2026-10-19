#include "iris/snes/apu.hpp"

#include "iris/byte_io.hpp"

#include <algorithm>
#include <cstdint>

namespace iris::systems::snes {
namespace {

constexpr int kNegative = 0x80;
constexpr int kOverflow = 0x40;
constexpr int kPage = 0x20;
constexpr int kHalf = 0x08;
constexpr int kZero = 0x02;
constexpr int kCarry = 0x01;

int clamp16(int value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return value;
}

int clip15(int value) {
    if (value > 0x3FFF) {
        return 0x3FFF;
    }
    if (value < -0x4000) {
        return -0x4000;
    }
    return value;
}

// Intermediate Gaussian sums wrap in 15 bits (S-DSP).
int wrap15(int value) {
    return static_cast<std::int16_t>(value << 1) >> 1;
}

// S-DSP Gaussian interpolation coefficients (public hardware documentation).
constexpr int kGauss[512] = {
    0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
    0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x002, 0x002, 0x002, 0x002, 0x002,
    0x002, 0x002, 0x003, 0x003, 0x003, 0x003, 0x003, 0x004, 0x004, 0x004, 0x004, 0x004, 0x005, 0x005, 0x005, 0x005,
    0x006, 0x006, 0x006, 0x006, 0x007, 0x007, 0x007, 0x008, 0x008, 0x008, 0x009, 0x009, 0x009, 0x00A, 0x00A, 0x00A,
    0x00B, 0x00B, 0x00B, 0x00C, 0x00C, 0x00D, 0x00D, 0x00E, 0x00E, 0x00F, 0x00F, 0x00F, 0x010, 0x010, 0x011, 0x011,
    0x012, 0x013, 0x013, 0x014, 0x014, 0x015, 0x015, 0x016, 0x017, 0x017, 0x018, 0x018, 0x019, 0x01A, 0x01B, 0x01B,
    0x01C, 0x01D, 0x01D, 0x01E, 0x01F, 0x020, 0x020, 0x021, 0x022, 0x023, 0x024, 0x024, 0x025, 0x026, 0x027, 0x028,
    0x029, 0x02A, 0x02B, 0x02C, 0x02D, 0x02E, 0x02F, 0x030, 0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x037, 0x038,
    0x03A, 0x03B, 0x03C, 0x03D, 0x03E, 0x040, 0x041, 0x042, 0x043, 0x045, 0x046, 0x047, 0x049, 0x04A, 0x04C, 0x04D,
    0x04E, 0x050, 0x051, 0x053, 0x054, 0x056, 0x057, 0x059, 0x05A, 0x05C, 0x05E, 0x05F, 0x061, 0x063, 0x064, 0x066,
    0x068, 0x06A, 0x06B, 0x06D, 0x06F, 0x071, 0x073, 0x075, 0x076, 0x078, 0x07A, 0x07C, 0x07E, 0x080, 0x082, 0x084,
    0x086, 0x089, 0x08B, 0x08D, 0x08F, 0x091, 0x093, 0x096, 0x098, 0x09A, 0x09C, 0x09F, 0x0A1, 0x0A3, 0x0A6, 0x0A8,
    0x0AB, 0x0AD, 0x0AF, 0x0B2, 0x0B4, 0x0B7, 0x0BA, 0x0BC, 0x0BF, 0x0C1, 0x0C4, 0x0C7, 0x0C9, 0x0CC, 0x0CF, 0x0D2,
    0x0D4, 0x0D7, 0x0DA, 0x0DD, 0x0E0, 0x0E3, 0x0E6, 0x0E9, 0x0EC, 0x0EF, 0x0F2, 0x0F5, 0x0F8, 0x0FB, 0x0FE, 0x101,
    0x104, 0x107, 0x10B, 0x10E, 0x111, 0x114, 0x118, 0x11B, 0x11E, 0x122, 0x125, 0x129, 0x12C, 0x130, 0x133, 0x137,
    0x13A, 0x13E, 0x141, 0x145, 0x148, 0x14C, 0x150, 0x153, 0x157, 0x15B, 0x15F, 0x162, 0x166, 0x16A, 0x16E, 0x172,
    0x176, 0x17A, 0x17D, 0x181, 0x185, 0x189, 0x18D, 0x191, 0x195, 0x19A, 0x19E, 0x1A2, 0x1A6, 0x1AA, 0x1AE, 0x1B2,
    0x1B7, 0x1BB, 0x1BF, 0x1C3, 0x1C8, 0x1CC, 0x1D0, 0x1D5, 0x1D9, 0x1DD, 0x1E2, 0x1E6, 0x1EB, 0x1EF, 0x1F3, 0x1F8,
    0x1FC, 0x201, 0x205, 0x20A, 0x20F, 0x213, 0x218, 0x21C, 0x221, 0x226, 0x22A, 0x22F, 0x233, 0x238, 0x23D, 0x241,
    0x246, 0x24B, 0x250, 0x254, 0x259, 0x25E, 0x263, 0x267, 0x26C, 0x271, 0x276, 0x27B, 0x280, 0x284, 0x289, 0x28E,
    0x293, 0x298, 0x29D, 0x2A2, 0x2A6, 0x2AB, 0x2B0, 0x2B5, 0x2BA, 0x2BF, 0x2C4, 0x2C9, 0x2CE, 0x2D3, 0x2D8, 0x2DC,
    0x2E1, 0x2E6, 0x2EB, 0x2F0, 0x2F5, 0x2FA, 0x2FF, 0x304, 0x309, 0x30E, 0x313, 0x318, 0x31D, 0x322, 0x326, 0x32B,
    0x330, 0x335, 0x33A, 0x33F, 0x344, 0x349, 0x34E, 0x353, 0x357, 0x35C, 0x361, 0x366, 0x36B, 0x370, 0x374, 0x379,
    0x37E, 0x383, 0x388, 0x38C, 0x391, 0x396, 0x39B, 0x39F, 0x3A4, 0x3A9, 0x3AD, 0x3B2, 0x3B7, 0x3BB, 0x3C0, 0x3C5,
    0x3C9, 0x3CE, 0x3D2, 0x3D7, 0x3DC, 0x3E0, 0x3E5, 0x3E9, 0x3ED, 0x3F2, 0x3F6, 0x3FB, 0x3FF, 0x403, 0x408, 0x40C,
    0x410, 0x415, 0x419, 0x41D, 0x421, 0x425, 0x42A, 0x42E, 0x432, 0x436, 0x43A, 0x43E, 0x442, 0x446, 0x44A, 0x44E,
    0x452, 0x455, 0x459, 0x45D, 0x461, 0x465, 0x468, 0x46C, 0x470, 0x473, 0x477, 0x47A, 0x47E, 0x481, 0x485, 0x488,
    0x48C, 0x48F, 0x492, 0x496, 0x499, 0x49C, 0x49F, 0x4A2, 0x4A6, 0x4A9, 0x4AC, 0x4AF, 0x4B2, 0x4B5, 0x4B7, 0x4BA,
    0x4BD, 0x4C0, 0x4C3, 0x4C5, 0x4C8, 0x4CB, 0x4CD, 0x4D0, 0x4D2, 0x4D5, 0x4D7, 0x4D9, 0x4DC, 0x4DE, 0x4E0, 0x4E3,
    0x4E5, 0x4E7, 0x4E9, 0x4EB, 0x4ED, 0x4EF, 0x4F1, 0x4F3, 0x4F5, 0x4F6, 0x4F8, 0x4FA, 0x4FB, 0x4FD, 0x4FF, 0x500,
    0x502, 0x503, 0x504, 0x506, 0x507, 0x508, 0x50A, 0x50B, 0x50C, 0x50D, 0x50E, 0x50F, 0x510, 0x511, 0x511, 0x512,
    0x513, 0x514, 0x514, 0x515, 0x516, 0x516, 0x517, 0x517, 0x517, 0x518, 0x518, 0x518, 0x518, 0x518, 0x519, 0x519};

// Samples between envelope steps for S-DSP rates 0–31. Rate 0 never advances.
constexpr int kEnvRate[32] = {0,    2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
                              64,   48,   40,   32,   24,  20,  16,  12,  10,  8,   6,   5,   4,   3,  2,  1};

int signed16(int low, int high) {
    int value = low | (high << 8);
    if ((value & 0x8000) != 0) {
        value |= ~0xFFFF;
    }
    return value;
}

}  // namespace

void Apu::power() {
    ram_.assign(65536, 0);
    rawLeft_.clear();
    rawRight_.clear();
    samples_.clear();
    echoLeft_.assign(8, 0);
    echoRight_.assign(8, 0);
    for (Voice& voice : voice_) {
        voice = Voice{};
    }
    std::fill(std::begin(dsp_), std::end(dsp_), static_cast<std::uint8_t>(0));
    // Soft reset + mute + echo write protect until a driver clears FLG.
    dsp_[0x6C] = 0xE0;
    std::fill(std::begin(cpuIn_), std::end(cpuIn_), static_cast<std::uint8_t>(0));
    control_ = hasIpl_ ? 0x80 : 0;
    dspAddress_ = 0;
    std::fill(std::begin(timerTarget_), std::end(timerTarget_), static_cast<std::uint8_t>(0));
    std::fill(std::begin(timerCount_), std::end(timerCount_), static_cast<std::uint8_t>(0));
    endx_ = 0;
    keyOn_ = 0;
    keyOff_ = 0;
    pc_ = 0;
    dest_ = 0;
    a_ = 0;
    x_ = 0;
    y_ = 0;
    sp_ = 0xEF;
    psw_ = 0;
    cursor_ = 0;
    clocked_ = 0;
    samplePhase_ = 0;
    std::fill(std::begin(timerDivider_), std::end(timerDivider_), 0);
    std::fill(std::begin(timerStage_), std::end(timerStage_), 0);
    echoPos_ = 0;
    echoDelay_ = 1;
    noise_ = 0x4000;
    noiseCounter_ = 0;
    cycleAccum_ = 0;
    outputAccum_ = 0;
    halted_ = false;
    sleeping_ = false;
    irqPending_ = false;
    if (hasIpl_) {
        booting_ = false;
        transferring_ = false;
        pc_ = 0xFFC0;
        cpuOut_[0] = 0;
        cpuOut_[1] = 0;
        cpuOut_[2] = 0;
        cpuOut_[3] = 0;
    } else {
        enterBoot();
    }
}

bool Apu::installIpl(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) {
        hasIpl_ = false;
        std::fill(std::begin(ipl_), std::end(ipl_), static_cast<std::uint8_t>(0));
        return true;
    }
    if (size != kIplSize) {
        return false;
    }
    std::copy(data, data + kIplSize, ipl_);
    hasIpl_ = true;
    return true;
}

void Apu::enterBoot() {
    // Software upload protocol: clear APU RAM like a fresh IPL entry. A user IPL
    // image keeps its own RAM; Nintendo's dump is never shipped.
    if (!hasIpl_ && !ram_.empty()) {
        std::fill(ram_.begin(), ram_.end(), static_cast<std::uint8_t>(0));
    }
    booting_ = true;
    transferring_ = false;
    halted_ = false;
    sleeping_ = false;
    irqPending_ = false;
    seen_ = cpuIn_[0];
    // Timers and CONTROL match a cold IPL entry (user IPL keeps ROM mapped via bit 7).
    std::fill(std::begin(timerCount_), std::end(timerCount_), static_cast<std::uint8_t>(0));
    std::fill(std::begin(timerDivider_), std::end(timerDivider_), 0);
    std::fill(std::begin(timerStage_), std::end(timerStage_), 0);
    control_ = hasIpl_ ? 0x80 : 0;
    cpuOut_[0] = 0xAA;
    cpuOut_[1] = 0xBB;
    cpuOut_[2] = 0;
    cpuOut_[3] = 0;
}

void Apu::beginFrame() {
    clocked_ = 0;
    rawLeft_.clear();
    rawRight_.clear();
}

void Apu::catchUp(int masterCycle, int masterClock) {
    // Do not bootPoll here: a 16-bit store to $2140 writes both port bytes in one
    // instruction, and the handshake must see them together after that instruction.
    if (masterClock <= 0 || masterCycle < clocked_) {
        return;
    }
    const int delta = masterCycle - clocked_;
    clocked_ = masterCycle;
    cycleAccum_ += static_cast<std::int64_t>(delta) * 1024000;
    int left = static_cast<int>(cycleAccum_ / masterClock);
    cycleAccum_ %= masterClock;
    while (left > 0) {
        int chunk = 32 - samplePhase_;
        if (chunk > left) {
            chunk = left;
        }
        run(chunk);
        samplePhase_ += chunk;
        left -= chunk;
        if (samplePhase_ >= 32) {
            samplePhase_ -= 32;
            float sampleLeft = 0.0f;
            float sampleRight = 0.0f;
            renderSample(sampleLeft, sampleRight);
            rawLeft_.push_back(sampleLeft);
            rawRight_.push_back(sampleRight);
        }
    }
}

void Apu::finishAudio(int masterCycles, int masterClock) {
    if (masterClock <= 0) {
        samples_.clear();
        return;
    }
    outputAccum_ += static_cast<std::int64_t>(kOutputRate) * masterCycles;
    int count = static_cast<int>(outputAccum_ / masterClock);
    outputAccum_ %= masterClock;
    if (count < 1) {
        count = 1;
    }
    samples_.assign(static_cast<std::size_t>(count) * 2u, 0.0f);
    if (rawLeft_.empty()) {
        return;
    }
    const int last = static_cast<int>(rawLeft_.size()) - 1;
    for (int index = 0; index < count; ++index) {
        const double position =
            static_cast<double>(index) * static_cast<double>(rawLeft_.size()) / static_cast<double>(count);
        const int base = static_cast<int>(position);
        const float fraction = static_cast<float>(position - base);
        const int next = base < last ? base + 1 : last;
        float left = rawLeft_[static_cast<std::size_t>(base)] +
                     (rawLeft_[static_cast<std::size_t>(next)] - rawLeft_[static_cast<std::size_t>(base)]) * fraction;
        float right = rawRight_[static_cast<std::size_t>(base)] +
                      (rawRight_[static_cast<std::size_t>(next)] - rawRight_[static_cast<std::size_t>(base)]) * fraction;
        if (left > 1.0f) left = 1.0f;
        if (left < -1.0f) left = -1.0f;
        if (right > 1.0f) right = 1.0f;
        if (right < -1.0f) right = -1.0f;
        samples_[static_cast<std::size_t>(index * 2)] = left;
        samples_[static_cast<std::size_t>(index * 2 + 1)] = right;
    }
}

std::uint8_t Apu::readPort(int index) const {
    if (index < 0 || index > 3) {
        return 0;
    }
    return cpuOut_[index];
}

void Apu::writePort(int index, std::uint8_t value) {
    if (index < 0 || index > 3) {
        return;
    }
    cpuIn_[index] = value;
}

void Apu::pollBoot() {
    if (booting_ && !hasIpl_) {
        bootPoll();
    }
}

void Apu::bootPoll() {
    if (!booting_) {
        return;
    }
    const std::uint8_t value = cpuIn_[0];
    if (value == seen_) {
        return;
    }
    seen_ = value;
    if (!transferring_) {
        if (value == 0xCC) {
            dest_ = static_cast<std::uint16_t>(cpuIn_[2] | (static_cast<std::uint16_t>(cpuIn_[3]) << 8));
            cursor_ = 0xFF;
            transferring_ = true;
            cpuOut_[0] = 0xCC;
        }
        return;
    }
    const auto delta = static_cast<std::uint8_t>(value - cursor_);
    if (delta == 0) {
        return;
    }
    if (delta == 1) {
        ram_[dest_] = cpuIn_[1];
        dest_ = static_cast<std::uint16_t>(dest_ + 1);
        cursor_ = value;
        cpuOut_[0] = value;
        return;
    }
    cpuOut_[0] = value;
    if (cpuIn_[1] == 0) {
        const auto entry = static_cast<std::uint16_t>(cpuIn_[2] | (static_cast<std::uint16_t>(cpuIn_[3]) << 8));
        transferring_ = false;
        if (entry == 0xFFC0) {
            enterBoot();
            return;
        }
        booting_ = false;
        transferring_ = false;
        halted_ = false;
        sleeping_ = false;
        irqPending_ = false;
        // Leave cpuIn_ as the kick bytes; the SPC program clears or handshake.
        // Echo the final port-0/1 values so a HiROM engine that polls $2140/$2141
        // sees the end-of-transfer ack before the first SPC instruction runs.
        seen_ = cpuIn_[0];
        cpuOut_[0] = cpuIn_[0];
        cpuOut_[1] = cpuIn_[1];
        pc_ = entry;
        sp_ = 0xEF;
        psw_ = 0;
        a_ = 0;
        x_ = 0;
        y_ = 0;
        return;
    }
    // Non-sequential $2140 with $2141 != 0 starts another transfer block.
    dest_ = static_cast<std::uint16_t>(cpuIn_[2] | (static_cast<std::uint16_t>(cpuIn_[3]) << 8));
    cursor_ = 0xFF;
    transferring_ = true;
}

void Apu::run(int cycles) {
    int left = cycles;
    while (left > 0) {
        if (booting_) {
            tickTimers(left);
            return;
        }
        if (halted_) {
            // STOP: clocks keep running for DSP catch-up, but the SPC does not.
            tickTimers(left);
            return;
        }
        if (irqPending_ && (psw_ & 0x04) != 0) {
            sleeping_ = false;
            serviceIrq();
            left -= 8;
            if (left < 0) {
                left = 0;
            }
            continue;
        }
        if (sleeping_) {
            tickTimers(1);
            --left;
            continue;
        }
        int used = instruction();
        if (used < 1) {
            used = 2;
        }
        tickTimers(used);
        left -= used;
    }
}

void Apu::serviceIrq() {
    irqPending_ = false;
    push(static_cast<std::uint8_t>(pc_ >> 8));
    push(static_cast<std::uint8_t>(pc_));
    push(psw_);
    // Timer IRQ clears I and does not set the break flag (unlike BRK).
    psw_ = static_cast<std::uint8_t>(psw_ & static_cast<std::uint8_t>(~0x04));
    pc_ = readWord(0xFFDE);
}

void Apu::tickTimers(int cycles) {
    for (int index = 0; index < 3; ++index) {
        if ((control_ & (1 << index)) == 0 || cycles <= 0) {
            continue;
        }
        timerDivider_[index] += cycles;
        const int period = index == 2 ? 16 : 128;
        while (timerDivider_[index] >= period) {
            timerDivider_[index] -= period;
            int target = timerTarget_[index];
            if (target == 0) {
                target = 256;
            }
            if (++timerStage_[index] >= target) {
                timerStage_[index] = 0;
                timerCount_[index] = static_cast<std::uint8_t>((timerCount_[index] + 1) & 0x0F);
                // Any timer overflow can raise IRQ when PSW.I is set.
                irqPending_ = true;
            }
        }
    }
}

std::uint16_t Apu::page() const {
    return (psw_ & kPage) != 0 ? 0x0100 : 0;
}

std::uint16_t Apu::direct(std::uint8_t offset) const {
    return static_cast<std::uint16_t>(page() + offset);
}

std::uint8_t Apu::read8(std::uint16_t address) {
    if (address >= 0x00F0 && address <= 0x00FF) {
        return readReg(address);
    }
    if (hasIpl_ && (control_ & 0x80) != 0 && address >= 0xFFC0) {
        return ipl_[address - 0xFFC0];
    }
    return ram_[address];
}

void Apu::write8(std::uint16_t address, std::uint8_t value) {
    if (address >= 0x00F0 && address <= 0x00FF) {
        writeReg(address, value);
        return;
    }
    ram_[address] = value;
}

std::uint8_t Apu::readReg(std::uint16_t address) {
    switch (address) {
    case 0x00F1:
        return control_;
    case 0x00F2:
        return dspAddress_;
    case 0x00F3:
        return (dspAddress_ & 0x7F) == 0x7C ? endx_ : dsp_[dspAddress_ & 0x7F];
    case 0x00F4:
    case 0x00F5:
    case 0x00F6:
    case 0x00F7:
        return cpuIn_[address - 0x00F4];
    case 0x00FA:
    case 0x00FB:
    case 0x00FC:
        return timerTarget_[address - 0x00FA];
    case 0x00FD:
    case 0x00FE:
    case 0x00FF: {
        const int index = address - 0x00FD;
        const std::uint8_t value = timerCount_[index];
        timerCount_[index] = 0;
        return value;
    }
    case 0x00F0:
        return 0;
    default:
        return ram_[address];
    }
}

void Apu::writeReg(std::uint16_t address, std::uint8_t value) {
    switch (address) {
    case 0x00F1: {
        const auto previous = control_;
        control_ = value;
        if ((value & 0x10) != 0) {
            cpuIn_[0] = 0;
            cpuIn_[1] = 0;
        }
        if ((value & 0x20) != 0) {
            cpuIn_[2] = 0;
            cpuIn_[3] = 0;
        }
        for (int index = 0; index < 3; ++index) {
            const bool wasOn = (previous & (1 << index)) != 0;
            const bool nowOn = (value & (1 << index)) != 0;
            // Disable clears the divider. Enable from off also starts a fresh stage.
            if (!nowOn || !wasOn) {
                timerDivider_[index] = 0;
                timerStage_[index] = 0;
            }
        }
        break;
    }
    case 0x00F2:
        // Bit 7 is not a DSP register, but it is readable. A driver can add
        // $10 until the address reads back as $8F, which never happens if the
        // bit is discarded on write.
        dspAddress_ = value;
        break;
    case 0x00F3: {
        const auto index = static_cast<std::uint8_t>(dspAddress_ & 0x7F);
        if (index == 0x4C) {
            // KON is edge-triggered; bits clear after the voice is armed.
            keyOn_ = static_cast<std::uint8_t>(keyOn_ | value);
            dsp_[0x4C] = value;
        } else if (index == 0x5C) {
            // KOFF is level-triggered and stays readable in $5C.
            keyOff_ = value;
            dsp_[0x5C] = value;
        } else if (index != 0x7C && (index & 0x0F) != 0x08 && (index & 0x0F) != 0x09) {
            dsp_[index] = value;
        }
        break;
    }
    case 0x00F4:
    case 0x00F5:
    case 0x00F6:
    case 0x00F7:
        cpuOut_[address - 0x00F4] = value;
        break;
    case 0x00FA:
    case 0x00FB:
    case 0x00FC:
        timerTarget_[address - 0x00FA] = value;
        break;
    case 0x00F0:
        break;
    default:
        ram_[address] = value;
        break;
    }
}

std::uint8_t Apu::fetch() {
    const std::uint8_t value = read8(pc_);
    pc_ = static_cast<std::uint16_t>(pc_ + 1);
    return value;
}

std::uint16_t Apu::fetchWord() {
    const auto low = fetch();
    const auto high = fetch();
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
}

std::uint16_t Apu::readWord(std::uint16_t address) {
    const auto low = read8(address);
    const auto high = read8(static_cast<std::uint16_t>(address + 1));
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
}

void Apu::setZN(std::uint8_t value) {
    psw_ = static_cast<std::uint8_t>((psw_ & ~(kNegative | kZero)) | (value == 0 ? kZero : 0) | (value & kNegative));
}

void Apu::add(std::uint8_t& target, std::uint8_t value) {
    const int carry = (psw_ & kCarry) != 0 ? 1 : 0;
    const int sum = target + value + carry;
    const int half = (target & 0x0F) + (value & 0x0F) + carry;
    const bool overflow = ((~(target ^ value) & (target ^ sum)) & 0x80) != 0;
    psw_ = static_cast<std::uint8_t>((psw_ & ~(kHalf | kOverflow | kCarry)) | (half > 0x0F ? kHalf : 0) |
                                     (overflow ? kOverflow : 0) | (sum > 0xFF ? kCarry : 0));
    target = static_cast<std::uint8_t>(sum);
    setZN(target);
}

void Apu::compare(std::uint8_t left, std::uint8_t right) {
    if (left >= right) {
        psw_ = static_cast<std::uint8_t>(psw_ | kCarry);
    } else {
        psw_ = static_cast<std::uint8_t>(psw_ & ~kCarry);
    }
    setZN(static_cast<std::uint8_t>(left - right));
}

void Apu::branch(bool taken) {
    const auto offset = static_cast<std::int8_t>(fetch());
    if (taken) {
        pc_ = static_cast<std::uint16_t>(pc_ + offset);
    }
}

void Apu::push(std::uint8_t value) {
    write8(static_cast<std::uint16_t>(0x0100 + sp_), value);
    --sp_;
}

std::uint8_t Apu::pull() {
    ++sp_;
    return read8(static_cast<std::uint16_t>(0x0100 + sp_));
}

void Apu::call(std::uint16_t address) {
    push(static_cast<std::uint8_t>(pc_ >> 8));
    push(static_cast<std::uint8_t>(pc_));
    pc_ = address;
}

void Apu::decodeBlock(Voice& voice) {
    const int header = ram_[static_cast<std::size_t>(voice.pointer)];
    int range = header >> 4;
    if (range > 12) {
        range = 12;
    }
    const int filter = (header >> 2) & 0x03;
    voice.blockEnds = (header & 0x02) != 0;
    voice.blockLoops = (header & 0x01) != 0;
    for (int pair = 0; pair < 8; ++pair) {
        const int byte = ram_[static_cast<std::size_t>((voice.pointer + 1 + pair) & 0xFFFF)];
        // The low nibble is the earlier sample. Swapping the pair makes a tone buzz.
        const int nibbles[2] = {byte & 0x0F, byte >> 4};
        for (int side = 0; side < 2; ++side) {
            int nibble = nibbles[side];
            if ((nibble & 0x08) != 0) {
                nibble |= ~0x0F;
            }
            int sample = (nibble << range) >> 1;
            if (filter == 1) {
                sample += voice.previous - (voice.previous >> 4);
            } else if (filter == 2) {
                sample += ((voice.previous * 61) >> 5) - ((voice.older * 15) >> 4);
            } else if (filter == 3) {
                sample += ((voice.previous * 115) >> 6) - ((voice.older * 13) >> 4);
            }
            sample = clamp16(sample);
            voice.decoded[pair * 2 + side] = sample;
            voice.older = voice.previous;
            voice.previous = sample;
        }
    }
    voice.pointer = (voice.pointer + 9) & 0xFFFF;
}

void Apu::keyOn(int voice) {
    Voice& current = voice_[voice];
    const int source = dsp_[(voice << 4) | 0x04];
    const int directory = (dsp_[0x5D] << 8) + source * 4;
    current.pointer = ram_[static_cast<std::size_t>(directory & 0xFFFF)] |
                      (ram_[static_cast<std::size_t>((directory + 1) & 0xFFFF)] << 8);
    current.loop = ram_[static_cast<std::size_t>((directory + 2) & 0xFFFF)] |
                   (ram_[static_cast<std::size_t>((directory + 3) & 0xFFFF)] << 8);
    current.phase = 0;
    current.older = 0;
    current.previous = 0;
    current.envelope = 0;
    current.output = 0;
    current.rateCounter = 0;
    current.brrPos = 0;
    current.konDelay = 5;
    current.envState = EnvState::Attack;
    current.playing = true;
    current.release = false;
    decodeBlock(current);
    // Gaussian playback starts centered on the second decoded sample.
    current.history[0] = 0;
    current.history[1] = 0;
    current.history[2] = current.decoded[0];
    current.history[3] = current.decoded[1];
    current.brrPos = 2;
    endx_ = static_cast<std::uint8_t>(endx_ & ~(1 << voice));
}

bool Apu::rateFires(int& counter, int rate) {
    if (rate <= 0 || rate > 31) {
        return false;
    }
    const int period = kEnvRate[rate];
    if (period <= 0) {
        return false;
    }
    ++counter;
    if (counter < period) {
        return false;
    }
    counter = 0;
    return true;
}

bool Apu::envelopeClock(Voice& voice, int rate) {
    return rateFires(voice.rateCounter, rate);
}

int Apu::nextBrrSample(Voice& voice, int index) {
    if (!voice.playing) {
        return 0;
    }
    if (voice.brrPos >= 16) {
        if (voice.blockEnds) {
            endx_ = static_cast<std::uint8_t>(endx_ | (1 << index));
            if (voice.blockLoops) {
                voice.pointer = voice.loop;
            } else {
                // End without loop: enter release at zero (keeps ENDX semantics).
                voice.release = true;
                voice.envState = EnvState::Release;
                voice.envelope = 0;
                voice.playing = false;
                return 0;
            }
        }
        if (voice.playing) {
            decodeBlock(voice);
            voice.brrPos = 0;
        }
    }
    if (!voice.playing) {
        return 0;
    }
    return voice.decoded[voice.brrPos++];
}

void Apu::tickEnvelope(Voice& voice, int index) {
    const int base = index << 4;
    if (voice.release || voice.envState == EnvState::Release) {
        voice.envState = EnvState::Release;
        if (envelopeClock(voice, 31)) {
            voice.envelope -= 8;
            if (voice.envelope <= 0) {
                voice.envelope = 0;
                voice.playing = false;
            }
        }
        return;
    }
    if ((dsp_[base | 0x05] & 0x80) == 0) {
        const int gain = dsp_[base | 0x07];
        if ((gain & 0x80) == 0) {
            voice.envelope = (gain & 0x7F) << 4;
        } else {
            const int rate = gain & 0x1F;
            const int mode = (gain >> 5) & 0x03;
            if (envelopeClock(voice, rate)) {
                if (mode == 0) {
                    voice.envelope -= 32;
                } else if (mode == 1) {
                    voice.envelope -= ((voice.envelope - 1) >> 8) + 1;
                } else if (mode == 2) {
                    voice.envelope += 32;
                } else {
                    voice.envelope += voice.envelope < 0x600 ? 32 : 8;
                }
            }
        }
    } else {
        const int ar = dsp_[base | 0x05] & 0x0F;
        const int dr = (dsp_[base | 0x05] >> 4) & 0x07;
        const int sl = (dsp_[base | 0x06] >> 5) & 0x07;
        const int sr = dsp_[base | 0x06] & 0x1F;
        const int sustain = (sl + 1) * 0x100;
        if (voice.envState == EnvState::Attack) {
            if (ar == 0x0F) {
                voice.envelope = 0x7FF;
                voice.envState = EnvState::Decay;
            } else if (envelopeClock(voice, (ar << 1) + 1)) {
                voice.envelope += voice.envelope < 0x600 ? 0x20 : 0x08;
                if (voice.envelope >= 0x7FF) {
                    voice.envelope = 0x7FF;
                    voice.envState = EnvState::Decay;
                }
            }
        } else if (voice.envState == EnvState::Decay) {
            if (envelopeClock(voice, 0x10 + (dr << 1))) {
                voice.envelope -= ((voice.envelope - 1) >> 8) + 1;
                if (voice.envelope <= sustain) {
                    voice.envelope = sustain;
                    voice.envState = EnvState::Sustain;
                }
            }
        } else if (sr != 0 && envelopeClock(voice, sr)) {
            voice.envelope -= ((voice.envelope - 1) >> 8) + 1;
        }
    }
    if (voice.envelope < 0) {
        voice.envelope = 0;
    }
    if (voice.envelope > 0x7FF) {
        voice.envelope = 0x7FF;
    }
}

int Apu::interpolate(const Voice& voice) const {
    const int index = (voice.phase >> 4) & 0xFF;
    int sample = (kGauss[0x0FF - index] * voice.history[0]) >> 11;
    sample = wrap15(sample);
    sample += (kGauss[0x1FF - index] * voice.history[1]) >> 11;
    sample = wrap15(sample);
    sample += (kGauss[0x100 + index] * voice.history[2]) >> 11;
    sample = wrap15(sample);
    sample += (kGauss[index] * voice.history[3]) >> 11;
    return clamp16(sample);
}

void Apu::renderSample(float& outLeft, float& outRight) {
    outLeft = 0.0f;
    outRight = 0.0f;
    const std::uint8_t flags = dsp_[0x6C];
    if (keyOn_ != 0) {
        for (int voice = 0; voice < 8; ++voice) {
            if ((keyOn_ & (1 << voice)) != 0) {
                keyOn(voice);
            }
        }
        keyOn_ = 0;
        dsp_[0x4C] = 0;
    }
    // KOFF is level-triggered from $5C.
    for (int voice = 0; voice < 8; ++voice) {
        if ((dsp_[0x5C] & (1 << voice)) != 0) {
            voice_[voice].release = true;
        }
    }
    if ((flags & 0x80) != 0) {
        // Soft reset clears ENDX and forces envelopes quiet, but echo still runs.
        endx_ = 0;
        for (Voice& voice : voice_) {
            voice.envelope = 0;
            voice.release = true;
            voice.envState = EnvState::Release;
            voice.output = 0;
            voice.konDelay = 0;
        }
    }
    // FLG bits 0–4 select the noise LFSR clock (same rate table as envelopes).
    if (rateFires(noiseCounter_, flags & 0x1F)) {
        const int noiseBit = (noise_ ^ (noise_ >> 1)) & 1;
        noise_ = (noise_ >> 1) | (noiseBit << 14);
    }
    int left = 0;
    int right = 0;
    int echoInLeft = 0;
    int echoInRight = 0;
    const std::uint8_t echoEnable = dsp_[0x4D];
    const std::uint8_t noiseEnable = dsp_[0x3D];
    const std::uint8_t pitchMod = static_cast<std::uint8_t>(dsp_[0x2D] & ~noiseEnable);
    for (int index = 0; index < 8; ++index) {
        Voice& voice = voice_[index];
        const int base = index << 4;
        if (!voice.playing) {
            dsp_[base | 0x08] = 0;
            dsp_[base | 0x09] = 0;
            continue;
        }
        if (voice.konDelay > 0) {
            --voice.konDelay;
            voice.output = 0;
            dsp_[base | 0x08] = 0;
            dsp_[base | 0x09] = 0;
            continue;
        }
        if (voice.release || (flags & 0x80) != 0) {
            voice.envState = EnvState::Release;
        }
        tickEnvelope(voice, index);
        int sample = 0;
        if ((noiseEnable & (1 << index)) != 0) {
            // Noise skips the Gaussian interpolator; the 15-bit LFSR is a signed sample.
            sample = static_cast<std::int16_t>(noise_ << 1);
        } else {
            sample = interpolate(voice);
        }
        int pitch = dsp_[base | 0x02] | ((dsp_[base | 0x03] & 0x3F) << 8);
        if (index > 0 && (pitchMod & (1 << index)) != 0) {
            pitch += (pitch * (voice_[index - 1].output >> 8)) >> 7;
        }
        if (pitch < 0) {
            pitch = 0;
        }
        if (pitch > 0x3FFF) {
            pitch = 0x3FFF;
        }
        // Advance phase after sampling. Step the four-tap window only on sample edges.
        voice.phase += pitch;
        while (voice.phase >= 0x1000 && voice.playing) {
            voice.phase -= 0x1000;
            voice.history[0] = voice.history[1];
            voice.history[1] = voice.history[2];
            voice.history[2] = voice.history[3];
            voice.history[3] = nextBrrSample(voice, index);
        }
        const int shaped = (sample * voice.envelope) >> 11;
        voice.output = shaped;
        dsp_[base | 0x08] = static_cast<std::uint8_t>((voice.envelope >> 4) & 0x7F);
        dsp_[base | 0x09] = static_cast<std::uint8_t>(shaped >> 8);
        const auto volumeLeft = static_cast<std::int8_t>(dsp_[base]);
        const auto volumeRight = static_cast<std::int8_t>(dsp_[base | 0x01]);
        const int voicedLeft = (shaped * volumeLeft) >> 7;
        const int voicedRight = (shaped * volumeRight) >> 7;
        left = clamp16(left + voicedLeft);
        right = clamp16(right + voicedRight);
        if ((echoEnable & (1 << index)) != 0) {
            echoInLeft = clamp16(echoInLeft + voicedLeft);
            echoInRight = clamp16(echoInRight + voicedRight);
        }
    }
    const auto mainLeft = static_cast<std::int8_t>(dsp_[0x0C]);
    const auto mainRight = static_cast<std::int8_t>(dsp_[0x1C]);
    left = (left * mainLeft) >> 7;
    right = (right * mainRight) >> 7;
    // Echo lives in APU RAM at ESA. EDL latches when the ring wraps.
    if (echoDelay_ < 1) {
        echoDelay_ = 1;
    }
    if (static_cast<int>(echoLeft_.size()) != echoDelay_) {
        echoLeft_.assign(static_cast<std::size_t>(echoDelay_), 0);
        echoRight_.assign(static_cast<std::size_t>(echoDelay_), 0);
        echoPos_ = 0;
    }
    const int delay = echoDelay_;
    const int echoBase = dsp_[0x6D] << 8;
    int filteredLeft = 0;
    int filteredRight = 0;
    for (int tap = 0; tap < 8; ++tap) {
        // FIR0 is the oldest sample, FIR7 the newest.
        int position = echoPos_ - 8 + tap;
        while (position < 0) {
            position += delay;
        }
        const int ramIndex = (echoBase + position * 4) & 0xFFFF;
        const int sampleLeft =
            signed16(ram_[static_cast<std::size_t>(ramIndex)], ram_[static_cast<std::size_t>((ramIndex + 1) & 0xFFFF)]);
        const int sampleRight = signed16(ram_[static_cast<std::size_t>((ramIndex + 2) & 0xFFFF)],
                                         ram_[static_cast<std::size_t>((ramIndex + 3) & 0xFFFF)]);
        echoLeft_[static_cast<std::size_t>(position)] = sampleLeft;
        echoRight_[static_cast<std::size_t>(position)] = sampleRight;
        const auto coefficient = static_cast<std::int8_t>(dsp_[0x0F + tap * 0x10]);
        filteredLeft += (sampleLeft * coefficient) >> 6;
        filteredRight += (sampleRight * coefficient) >> 6;
        if (tap < 7) {
            filteredLeft = clip15(filteredLeft);
            filteredRight = clip15(filteredRight);
        } else {
            filteredLeft = clamp16(filteredLeft) & ~1;
            filteredRight = clamp16(filteredRight) & ~1;
        }
    }
    const auto echoVolLeft = static_cast<std::int8_t>(dsp_[0x2C]);
    const auto echoVolRight = static_cast<std::int8_t>(dsp_[0x3C]);
    left += (filteredLeft * echoVolLeft) >> 7;
    right += (filteredRight * echoVolRight) >> 7;
    if ((flags & 0x20) == 0) {
        const auto feedback = static_cast<std::int8_t>(dsp_[0x0D]);
        const int storedLeft = clamp16(echoInLeft + ((filteredLeft * feedback) >> 7)) & ~1;
        const int storedRight = clamp16(echoInRight + ((filteredRight * feedback) >> 7)) & ~1;
        echoLeft_[static_cast<std::size_t>(echoPos_)] = storedLeft;
        echoRight_[static_cast<std::size_t>(echoPos_)] = storedRight;
        const int ramIndex = (echoBase + echoPos_ * 4) & 0xFFFF;
        ram_[static_cast<std::size_t>(ramIndex)] = static_cast<std::uint8_t>(storedLeft);
        ram_[static_cast<std::size_t>((ramIndex + 1) & 0xFFFF)] = static_cast<std::uint8_t>(storedLeft >> 8);
        ram_[static_cast<std::size_t>((ramIndex + 2) & 0xFFFF)] = static_cast<std::uint8_t>(storedRight);
        ram_[static_cast<std::size_t>((ramIndex + 3) & 0xFFFF)] = static_cast<std::uint8_t>(storedRight >> 8);
    }
    // Even with echo write protect, the ring pointer keeps advancing.
    echoPos_ = (echoPos_ + 1) % delay;
    if (echoPos_ == 0) {
        const int edl = dsp_[0x7D] & 0x0F;
        echoDelay_ = edl == 0 ? 1 : edl * 512;
    }
    if ((flags & 0x40) != 0 || (flags & 0x80) != 0) {
        return;
    }
    outLeft = static_cast<float>(clamp16(left)) / 32768.0f;
    outRight = static_cast<float>(clamp16(right)) / 32768.0f;
}

int Apu::instruction() {
    if (!hasIpl_ && pc_ == 0xFFC0) {
        enterBoot();
        return 2;
    }
    const std::uint8_t op = fetch();
    if ((op & 0x0F) == 0x01) {
        const auto vector = static_cast<std::uint16_t>(0xFFDE - (op >> 4) * 2);
        call(readWord(vector));
        return 8;
    }
    if ((op & 0x1F) == 0x02 || (op & 0x1F) == 0x12) {
        const auto address = direct(fetch());
        const auto mask = static_cast<std::uint8_t>(1 << (op >> 5));
        const auto value = read8(address);
        write8(address, (op & 0x1F) == 0x02 ? static_cast<std::uint8_t>(value | mask)
                                            : static_cast<std::uint8_t>(value & ~mask));
        return 4;
    }
    if ((op & 0x1F) == 0x03 || (op & 0x1F) == 0x13) {
        const auto address = direct(fetch());
        const bool set = (read8(address) & (1 << (op >> 5))) != 0;
        branch((op & 0x1F) == 0x03 ? set : !set);
        return 6;
    }
    const int kind = op >> 5;
    const int mode = [op]() {
        switch (op & 0x1F) {
        case 0x04:
            return 2;
        case 0x05:
            return 4;
        case 0x06:
            return 1;
        case 0x07:
            return 7;
        case 0x08:
            return 0;
        case 0x09:
            return 10;
        case 0x14:
            return 3;
        case 0x15:
            return 5;
        case 0x16:
            return 6;
        case 0x17:
            return 8;
        case 0x18:
            return 11;
        case 0x19:
            return 9;
        default:
            return -1;
        }
    }();
    if (kind <= 5 && mode >= 0) {
        const auto apply = [&](std::uint8_t& target, std::uint8_t value) {
            if (kind == 3) {
                compare(target, value);
                return;
            }
            if (kind == 4) {
                add(target, value);
                return;
            }
            if (kind == 5) {
                add(target, static_cast<std::uint8_t>(~value));
                return;
            }
            if (kind == 0) {
                target = static_cast<std::uint8_t>(target | value);
            } else if (kind == 1) {
                target = static_cast<std::uint8_t>(target & value);
            } else {
                target = static_cast<std::uint8_t>(target ^ value);
            }
            setZN(target);
        };
        if (mode == 9) {
            std::uint8_t left = read8(direct(x_));
            apply(left, read8(direct(y_)));
            if (kind != 3) {
                write8(direct(x_), left);
            }
            return 5;
        }
        if (mode == 10 || mode == 11) {
            const auto first = fetch();
            const auto second = fetch();
            const auto destination = direct(second);
            const auto source = mode == 11 ? first : read8(direct(first));
            std::uint8_t left = read8(destination);
            apply(left, mode == 11 ? first : source);
            if (kind != 3) {
                write8(destination, left);
            }
            return mode == 11 ? 5 : 6;
        }
        std::uint8_t value = 0;
        int cycles = 2;
        if (mode == 0) {
            value = fetch();
            cycles = 2;
        } else if (mode == 1) {
            value = read8(direct(x_));
            cycles = 3;
        } else if (mode == 2) {
            value = read8(direct(fetch()));
            cycles = 3;
        } else if (mode == 3) {
            value = read8(direct(static_cast<std::uint8_t>(fetch() + x_)));
            cycles = 4;
        } else if (mode == 4) {
            value = read8(fetchWord());
            cycles = 4;
        } else if (mode == 5) {
            value = read8(static_cast<std::uint16_t>(fetchWord() + x_));
            cycles = 5;
        } else if (mode == 6) {
            value = read8(static_cast<std::uint16_t>(fetchWord() + y_));
            cycles = 5;
        } else if (mode == 7) {
            value = read8(readWord(direct(static_cast<std::uint8_t>(fetch() + x_))));
            cycles = 6;
        } else {
            value = read8(static_cast<std::uint16_t>(readWord(direct(fetch())) + y_));
            cycles = 6;
        }
        apply(a_, value);
        return cycles;
    }

    switch (op) {
    case 0x00:
        return 2;
    case 0x20:
        psw_ = static_cast<std::uint8_t>(psw_ & ~kPage);
        return 2;
    case 0x40:
        psw_ = static_cast<std::uint8_t>(psw_ | kPage);
        return 2;
    case 0x60:
        psw_ = static_cast<std::uint8_t>(psw_ & ~kCarry);
        return 2;
    case 0x80:
        psw_ = static_cast<std::uint8_t>(psw_ | kCarry);
        return 2;
    case 0xED:
        psw_ = static_cast<std::uint8_t>(psw_ ^ kCarry);
        return 3;
    case 0xE0:
        psw_ = static_cast<std::uint8_t>(psw_ & ~(kOverflow | kHalf));
        return 2;
    case 0xA0:
        psw_ = static_cast<std::uint8_t>(psw_ | 0x04);
        return 3;
    case 0xC0:
        psw_ = static_cast<std::uint8_t>(psw_ & ~0x04);
        return 3;
    case 0x2F:
        branch(true);
        return 4;
    case 0xF0:
        branch((psw_ & kZero) != 0);
        return 4;
    case 0xD0:
        branch((psw_ & kZero) == 0);
        return 4;
    case 0xB0:
        branch((psw_ & kCarry) != 0);
        return 4;
    case 0x90:
        branch((psw_ & kCarry) == 0);
        return 4;
    case 0x70:
        branch((psw_ & kOverflow) != 0);
        return 4;
    case 0x50:
        branch((psw_ & kOverflow) == 0);
        return 4;
    case 0x30:
        branch((psw_ & kNegative) != 0);
        return 4;
    case 0x10:
        branch((psw_ & kNegative) == 0);
        return 4;
    case 0x2E:
        branch(a_ != read8(direct(fetch())));
        return 6;
    case 0xDE:
        branch(a_ != read8(direct(static_cast<std::uint8_t>(fetch() + x_))));
        return 6;
    case 0x6E: {
        const auto address = direct(fetch());
        const auto value = static_cast<std::uint8_t>(read8(address) - 1);
        write8(address, value);
        branch(value != 0);
        return 6;
    }
    case 0xFE:
        --y_;
        branch(y_ != 0);
        return 6;
    case 0x5F:
        pc_ = fetchWord();
        return 3;
    case 0x1F:
        pc_ = readWord(static_cast<std::uint16_t>(fetchWord() + x_));
        return 6;
    case 0x3F:
        call(fetchWord());
        return 8;
    case 0x4F:
        call(static_cast<std::uint16_t>(0xFF00 | fetch()));
        return 6;
    case 0x0F:
        push(static_cast<std::uint8_t>(pc_ >> 8));
        push(static_cast<std::uint8_t>(pc_));
        push(psw_);
        psw_ = static_cast<std::uint8_t>((psw_ | 0x10) & ~0x04);
        pc_ = readWord(0xFFDE);
        return 8;
    case 0x6F: {
        const auto low = pull();
        const auto high = pull();
        pc_ = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
        return 5;
    }
    case 0x7F: {
        psw_ = pull();
        const auto low = pull();
        const auto high = pull();
        pc_ = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
        return 6;
    }
    case 0x2D:
        push(a_);
        return 4;
    case 0x4D:
        push(x_);
        return 4;
    case 0x6D:
        push(y_);
        return 4;
    case 0x0D:
        push(psw_);
        return 4;
    case 0xAE:
        a_ = pull();
        return 4;
    case 0xCE:
        x_ = pull();
        return 4;
    case 0xEE:
        y_ = pull();
        return 4;
    case 0x8E:
        psw_ = pull();
        return 4;
    case 0xBC:
        ++a_;
        setZN(a_);
        return 2;
    case 0x3D:
        ++x_;
        setZN(x_);
        return 2;
    case 0xFC:
        ++y_;
        setZN(y_);
        return 2;
    case 0x9C:
        --a_;
        setZN(a_);
        return 2;
    case 0x1D:
        --x_;
        setZN(x_);
        return 2;
    case 0xDC:
        --y_;
        setZN(y_);
        return 2;
    case 0xAB:
    case 0xBB:
    case 0xAC:
    case 0x8B:
    case 0x9B:
    case 0x8C: {
        std::uint16_t address = 0;
        if (op == 0xAB || op == 0x8B) {
            address = direct(fetch());
        } else if (op == 0xBB || op == 0x9B) {
            address = direct(static_cast<std::uint8_t>(fetch() + x_));
        } else {
            address = fetchWord();
        }
        auto value = read8(address);
        value = static_cast<std::uint8_t>(op == 0x8B || op == 0x9B || op == 0x8C ? value - 1 : value + 1);
        write8(address, value);
        setZN(value);
        return op == 0xAB || op == 0x8B ? 4 : 5;
    }
    case 0x1C:
    case 0x0B:
    case 0x1B:
    case 0x0C:
    case 0x5C:
    case 0x4B:
    case 0x5B:
    case 0x4C:
    case 0x3C:
    case 0x2B:
    case 0x3B:
    case 0x2C:
    case 0x7C:
    case 0x6B:
    case 0x7B:
    case 0x6C: {
        const bool accumulator = op == 0x1C || op == 0x5C || op == 0x3C || op == 0x7C;
        const bool indexed = op == 0x1B || op == 0x5B || op == 0x3B || op == 0x7B;
        const bool directPage = op == 0x0B || op == 0x4B || op == 0x2B || op == 0x6B;
        const bool right = op == 0x5C || op == 0x4B || op == 0x5B || op == 0x4C || op == 0x7C || op == 0x6B ||
                           op == 0x7B || op == 0x6C;
        const bool rotate = op == 0x3C || op == 0x2B || op == 0x3B || op == 0x2C || op == 0x7C || op == 0x6B ||
                            op == 0x7B || op == 0x6C;
        std::uint16_t address = 0;
        std::uint8_t value = a_;
        if (!accumulator) {
            if (directPage) {
                address = direct(fetch());
            } else if (indexed) {
                address = direct(static_cast<std::uint8_t>(fetch() + x_));
            } else {
                address = fetchWord();
            }
            value = read8(address);
        }
        const int carry = (psw_ & kCarry) != 0 ? 1 : 0;
        int out = 0;
        if (!right && !rotate) {
            out = (value >> 7) & 1;
            value = static_cast<std::uint8_t>(value << 1);
        } else if (!right) {
            out = (value >> 7) & 1;
            value = static_cast<std::uint8_t>((value << 1) | carry);
        } else if (!rotate) {
            out = value & 1;
            value = static_cast<std::uint8_t>(value >> 1);
        } else {
            out = value & 1;
            value = static_cast<std::uint8_t>((value >> 1) | (carry << 7));
        }
        if (out != 0) {
            psw_ = static_cast<std::uint8_t>(psw_ | kCarry);
        } else {
            psw_ = static_cast<std::uint8_t>(psw_ & ~kCarry);
        }
        setZN(value);
        if (accumulator) {
            a_ = value;
        } else {
            write8(address, value);
        }
        return accumulator ? 2 : 4;
    }
    case 0x9F:
        a_ = static_cast<std::uint8_t>((a_ >> 4) | (a_ << 4));
        setZN(a_);
        return 5;
    case 0xE8:
        a_ = fetch();
        setZN(a_);
        return 2;
    case 0xE6:
        a_ = read8(direct(x_));
        setZN(a_);
        return 3;
    case 0xBF:
        a_ = read8(direct(x_));
        ++x_;
        setZN(a_);
        return 4;
    case 0xE4:
        a_ = read8(direct(fetch()));
        setZN(a_);
        return 3;
    case 0xF4:
        a_ = read8(direct(static_cast<std::uint8_t>(fetch() + x_)));
        setZN(a_);
        return 4;
    case 0xE5:
        a_ = read8(fetchWord());
        setZN(a_);
        return 4;
    case 0xF5:
        a_ = read8(static_cast<std::uint16_t>(fetchWord() + x_));
        setZN(a_);
        return 5;
    case 0xF6:
        a_ = read8(static_cast<std::uint16_t>(fetchWord() + y_));
        setZN(a_);
        return 5;
    case 0xE7:
        a_ = read8(readWord(direct(static_cast<std::uint8_t>(fetch() + x_))));
        setZN(a_);
        return 6;
    case 0xF7:
        a_ = read8(static_cast<std::uint16_t>(readWord(direct(fetch())) + y_));
        setZN(a_);
        return 6;
    case 0xCD:
        x_ = fetch();
        setZN(x_);
        return 2;
    case 0xF8:
        x_ = read8(direct(fetch()));
        setZN(x_);
        return 3;
    case 0xF9:
        x_ = read8(direct(static_cast<std::uint8_t>(fetch() + y_)));
        setZN(x_);
        return 4;
    case 0xE9:
        x_ = read8(fetchWord());
        setZN(x_);
        return 4;
    case 0x8D:
        y_ = fetch();
        setZN(y_);
        return 2;
    case 0xEB:
        y_ = read8(direct(fetch()));
        setZN(y_);
        return 3;
    case 0xFB:
        y_ = read8(direct(static_cast<std::uint8_t>(fetch() + x_)));
        setZN(y_);
        return 4;
    case 0xEC:
        y_ = read8(fetchWord());
        setZN(y_);
        return 4;
    case 0xC6:
        write8(direct(x_), a_);
        return 4;
    case 0xAF:
        write8(direct(x_), a_);
        ++x_;
        return 4;
    case 0xC4:
        write8(direct(fetch()), a_);
        return 4;
    case 0xD4:
        write8(direct(static_cast<std::uint8_t>(fetch() + x_)), a_);
        return 5;
    case 0xC5:
        write8(fetchWord(), a_);
        return 5;
    case 0xD5:
        write8(static_cast<std::uint16_t>(fetchWord() + x_), a_);
        return 6;
    case 0xD6:
        write8(static_cast<std::uint16_t>(fetchWord() + y_), a_);
        return 6;
    case 0xC7:
        write8(readWord(direct(static_cast<std::uint8_t>(fetch() + x_))), a_);
        return 7;
    case 0xD7:
        write8(static_cast<std::uint16_t>(readWord(direct(fetch())) + y_), a_);
        return 7;
    case 0xD8:
        write8(direct(fetch()), x_);
        return 4;
    case 0xD9:
        write8(direct(static_cast<std::uint8_t>(fetch() + y_)), x_);
        return 5;
    case 0xC9:
        write8(fetchWord(), x_);
        return 5;
    case 0xCB:
        write8(direct(fetch()), y_);
        return 4;
    case 0xDB:
        write8(direct(static_cast<std::uint8_t>(fetch() + x_)), y_);
        return 5;
    case 0xCC:
        write8(fetchWord(), y_);
        return 5;
    case 0x7D:
        a_ = x_;
        setZN(a_);
        return 2;
    case 0xDD:
        a_ = y_;
        setZN(a_);
        return 2;
    case 0x5D:
        x_ = a_;
        setZN(x_);
        return 2;
    case 0xFD:
        y_ = a_;
        setZN(y_);
        return 2;
    case 0x9D:
        x_ = sp_;
        setZN(x_);
        return 2;
    case 0xBD:
        sp_ = x_;
        return 2;
    case 0xFA: {
        const auto source = fetch();
        const auto destination = fetch();
        write8(direct(destination), read8(direct(source)));
        return 5;
    }
    case 0x8F: {
        const auto value = fetch();
        write8(direct(fetch()), value);
        return 5;
    }
    case 0xBA: {
        const auto word = readWord(direct(fetch()));
        a_ = static_cast<std::uint8_t>(word);
        y_ = static_cast<std::uint8_t>(word >> 8);
        psw_ = static_cast<std::uint8_t>((psw_ & ~(kNegative | kZero)) | (y_ & kNegative) | (word == 0 ? kZero : 0));
        return 5;
    }
    case 0xDA: {
        const auto address = direct(fetch());
        write8(address, a_);
        write8(static_cast<std::uint16_t>(address + 1), y_);
        return 4;
    }
    case 0x3A:
    case 0x1A: {
        const auto address = direct(fetch());
        auto word = readWord(address);
        word = static_cast<std::uint16_t>(op == 0x3A ? word + 1 : word - 1);
        write8(address, static_cast<std::uint8_t>(word));
        write8(static_cast<std::uint16_t>(address + 1), static_cast<std::uint8_t>(word >> 8));
        psw_ = static_cast<std::uint8_t>((psw_ & ~(kNegative | kZero)) | ((word & 0x8000) != 0 ? kNegative : 0) |
                                         (word == 0 ? kZero : 0));
        return 6;
    }
    case 0x7A:
    case 0x9A:
    case 0x5A: {
        const auto word = readWord(direct(fetch()));
        const int ya = a_ | (y_ << 8);
        const bool subtract = op != 0x7A;
        const int result = subtract ? ya - word : ya + word;
        if (op != 0x5A) {
            a_ = static_cast<std::uint8_t>(result);
            y_ = static_cast<std::uint8_t>(result >> 8);
        }
        const bool carry = subtract ? ya >= word : result > 0xFFFF;
        if (carry) {
            psw_ = static_cast<std::uint8_t>(psw_ | kCarry);
        } else {
            psw_ = static_cast<std::uint8_t>(psw_ & ~kCarry);
        }
        psw_ = static_cast<std::uint8_t>((psw_ & ~(kNegative | kZero)) | ((result & 0x8000) != 0 ? kNegative : 0) |
                                         ((result & 0xFFFF) == 0 ? kZero : 0));
        return 5;
    }
    case 0xCF: {
        const int product = y_ * a_;
        a_ = static_cast<std::uint8_t>(product);
        y_ = static_cast<std::uint8_t>(product >> 8);
        setZN(y_);
        return 9;
    }
    case 0x9E: {
        const int ya = a_ | (y_ << 8);
        if (x_ == 0) {
            psw_ = static_cast<std::uint8_t>(psw_ | kOverflow);
            a_ = 0xFF;
            y_ = 0xFF;
        } else {
            const int quotient = ya / x_;
            y_ = static_cast<std::uint8_t>(ya % x_);
            a_ = static_cast<std::uint8_t>(quotient);
            if (quotient > 0xFF) {
                psw_ = static_cast<std::uint8_t>(psw_ | kOverflow);
            } else {
                psw_ = static_cast<std::uint8_t>(psw_ & ~kOverflow);
            }
        }
        setZN(a_);
        return 12;
    }
    case 0xDF: {
        int value = a_;
        if ((psw_ & kHalf) != 0 || (value & 0x0F) > 9) {
            value += 6;
        }
        if ((psw_ & kCarry) != 0 || value > 0x9F) {
            value += 0x60;
            psw_ = static_cast<std::uint8_t>(psw_ | kCarry);
        }
        a_ = static_cast<std::uint8_t>(value);
        setZN(a_);
        return 3;
    }
    case 0xBE: {
        int value = a_;
        if ((psw_ & kHalf) != 0) {
            value -= 6;
        }
        if ((psw_ & kCarry) != 0) {
            value -= 0x60;
        }
        a_ = static_cast<std::uint8_t>(value);
        setZN(a_);
        return 3;
    }
    case 0x0E:
    case 0x4E: {
        const auto address = fetchWord();
        const auto value = read8(address);
        setZN(static_cast<std::uint8_t>(a_ - value));
        write8(address, op == 0x0E ? static_cast<std::uint8_t>(value | a_) : static_cast<std::uint8_t>(value & ~a_));
        return 6;
    }
    case 0xAA:
    case 0xCA:
    case 0x4A:
    case 0x6A:
    case 0x0A:
    case 0x2A:
    case 0x8A:
    case 0xEA: {
        const auto spec = fetchWord();
        const int bit = spec >> 13;
        const auto address = static_cast<std::uint16_t>(spec & 0x1FFF);
        const auto mask = static_cast<std::uint8_t>(1 << bit);
        const bool on = (read8(address) & mask) != 0;
        if (op == 0xEA) {
            write8(address, static_cast<std::uint8_t>(read8(address) ^ mask));
        } else if (op == 0xCA) {
            const auto value = read8(address);
            write8(address, (psw_ & kCarry) != 0 ? static_cast<std::uint8_t>(value | mask)
                                                 : static_cast<std::uint8_t>(value & ~mask));
        } else {
            bool carry = (psw_ & kCarry) != 0;
            if (op == 0xAA) {
                carry = on;
            } else if (op == 0x4A) {
                carry = carry && on;
            } else if (op == 0x6A) {
                carry = carry && !on;
            } else if (op == 0x0A) {
                carry = carry || on;
            } else if (op == 0x2A) {
                carry = carry || !on;
            } else {
                carry = carry != on;
            }
            if (carry) {
                psw_ = static_cast<std::uint8_t>(psw_ | kCarry);
            } else {
                psw_ = static_cast<std::uint8_t>(psw_ & ~kCarry);
            }
        }
        return 5;
    }
    case 0xC8:
        compare(x_, fetch());
        return 2;
    case 0x3E:
        compare(x_, read8(direct(fetch())));
        return 3;
    case 0x1E:
        compare(x_, read8(fetchWord()));
        return 4;
    case 0xAD:
        compare(y_, fetch());
        return 2;
    case 0x7E:
        compare(y_, read8(direct(fetch())));
        return 3;
    case 0x5E:
        compare(y_, read8(fetchWord()));
        return 4;
    case 0xEF:
        // SLEEP waits for a timer IRQ (or other IRQ). STOP is permanent until reset.
        sleeping_ = true;
        return 3;
    case 0xFF:
        halted_ = true;
        sleeping_ = false;
        return 3;
    default:
        // Every SPC700 opcode is handled above (TCALL / SET1 / CLR1 / BBS / BBC /
        // OR–SBC addressing groups, then this switch). Do not halt on a stray.
        return 2;
    }
}

void Apu::saveState(iris::ByteWriter& out) const {
    out.bytes(ram_.data(), ram_.size());
    for (const Voice& voice : voice_) {
        out.u32(static_cast<std::uint32_t>(voice.pointer));
        out.u32(static_cast<std::uint32_t>(voice.loop));
        out.u32(static_cast<std::uint32_t>(voice.phase));
        out.u32(static_cast<std::uint32_t>(voice.older));
        out.u32(static_cast<std::uint32_t>(voice.previous));
        out.u32(static_cast<std::uint32_t>(voice.envelope));
        out.u32(static_cast<std::uint32_t>(voice.output));
        out.boolean(voice.playing);
        out.boolean(voice.release);
        out.boolean(voice.blockEnds);
        out.boolean(voice.blockLoops);
        for (const int sample : voice.decoded) {
            out.u32(static_cast<std::uint32_t>(sample));
        }
    }
    out.bytes(dsp_, sizeof(dsp_));
    out.bytes(cpuIn_, sizeof(cpuIn_));
    out.bytes(cpuOut_, sizeof(cpuOut_));
    out.u8(control_);
    out.u8(dspAddress_);
    out.bytes(timerTarget_, sizeof(timerTarget_));
    out.bytes(timerCount_, sizeof(timerCount_));
    out.u8(endx_);
    out.u8(keyOn_);
    out.u8(keyOff_);
    out.u16(pc_);
    out.u16(dest_);
    out.u8(a_);
    out.u8(x_);
    out.u8(y_);
    out.u8(sp_);
    out.u8(psw_);
    out.u8(cursor_);
    out.u8(seen_);
    out.u32(static_cast<std::uint32_t>(clocked_));
    out.u32(static_cast<std::uint32_t>(samplePhase_));
    for (const int value : timerDivider_) {
        out.u32(static_cast<std::uint32_t>(value));
    }
    for (const int value : timerStage_) {
        out.u32(static_cast<std::uint32_t>(value));
    }
    out.u32(static_cast<std::uint32_t>(echoPos_));
    out.u32(static_cast<std::uint32_t>(noise_));
    out.u64(static_cast<std::uint64_t>(cycleAccum_));
    out.u64(static_cast<std::uint64_t>(outputAccum_));
    out.boolean(booting_);
    out.boolean(transferring_);
    out.boolean(halted_);
    out.boolean(hasIpl_);
    out.bytes(ipl_, sizeof(ipl_));
    out.u32(static_cast<std::uint32_t>(echoLeft_.size()));
    for (const int sample : echoLeft_) {
        out.u32(static_cast<std::uint32_t>(sample));
    }
    for (const int sample : echoRight_) {
        out.u32(static_cast<std::uint32_t>(sample));
    }
    out.boolean(sleeping_);
    out.boolean(irqPending_);
}

bool Apu::loadState(iris::ByteReader& in) {
    std::vector<std::uint8_t> ram(ram_.size());
    in.bytes(ram.data(), ram.size());
    Voice voice[8]{};
    for (Voice& entry : voice) {
        entry.pointer = static_cast<int>(in.u32());
        entry.loop = static_cast<int>(in.u32());
        entry.phase = static_cast<int>(in.u32());
        entry.older = static_cast<int>(in.u32());
        entry.previous = static_cast<int>(in.u32());
        entry.envelope = static_cast<int>(in.u32());
        entry.output = static_cast<int>(in.u32());
        entry.playing = in.boolean();
        entry.release = in.boolean();
        entry.blockEnds = in.boolean();
        entry.blockLoops = in.boolean();
        for (int& sample : entry.decoded) {
            sample = static_cast<int>(in.u32());
        }
    }
    std::uint8_t dsp[128] = {};
    std::uint8_t cpuIn[4] = {};
    std::uint8_t cpuOut[4] = {};
    in.bytes(dsp, sizeof(dsp));
    in.bytes(cpuIn, sizeof(cpuIn));
    in.bytes(cpuOut, sizeof(cpuOut));
    const auto control = in.u8();
    const auto dspAddress = in.u8();
    std::uint8_t timerTarget[3] = {};
    std::uint8_t timerCount[3] = {};
    in.bytes(timerTarget, sizeof(timerTarget));
    in.bytes(timerCount, sizeof(timerCount));
    const auto endx = in.u8();
    const auto keyOn = in.u8();
    const auto keyOff = in.u8();
    const auto pc = in.u16();
    const auto dest = in.u16();
    const auto a = in.u8();
    const auto x = in.u8();
    const auto y = in.u8();
    const auto sp = in.u8();
    const auto psw = in.u8();
    const auto cursor = in.u8();
    const auto seen = in.u8();
    const auto clocked = static_cast<int>(in.u32());
    const auto samplePhase = static_cast<int>(in.u32());
    int timerDivider[3] = {};
    int timerStage[3] = {};
    for (int& value : timerDivider) {
        value = static_cast<int>(in.u32());
    }
    for (int& value : timerStage) {
        value = static_cast<int>(in.u32());
    }
    const auto echoPos = static_cast<int>(in.u32());
    const auto noise = static_cast<int>(in.u32());
    const auto cycleAccum = static_cast<std::int64_t>(in.u64());
    const auto outputAccum = static_cast<std::int64_t>(in.u64());
    const auto booting = in.boolean();
    const auto transferring = in.boolean();
    const auto halted = in.boolean();
    const auto hasIpl = in.boolean();
    std::uint8_t ipl[kIplSize] = {};
    in.bytes(ipl, sizeof(ipl));
    const auto echoCount = in.u32();
    if (!in.ok() || echoCount > 64 * 1024) {
        return false;
    }
    std::vector<int> echoLeft(echoCount);
    std::vector<int> echoRight(echoCount);
    for (int& sample : echoLeft) {
        sample = static_cast<int>(in.u32());
    }
    for (int& sample : echoRight) {
        sample = static_cast<int>(in.u32());
    }
    const auto sleeping = in.boolean();
    const auto irqPending = in.boolean();
    if (!in.ok()) {
        return false;
    }
    ram_ = std::move(ram);
    for (int index = 0; index < 8; ++index) {
        voice_[index] = voice[index];
    }
    std::copy(std::begin(dsp), std::end(dsp), std::begin(dsp_));
    std::copy(std::begin(cpuIn), std::end(cpuIn), std::begin(cpuIn_));
    std::copy(std::begin(cpuOut), std::end(cpuOut), std::begin(cpuOut_));
    control_ = control;
    dspAddress_ = dspAddress;
    std::copy(std::begin(timerTarget), std::end(timerTarget), std::begin(timerTarget_));
    std::copy(std::begin(timerCount), std::end(timerCount), std::begin(timerCount_));
    endx_ = endx;
    keyOn_ = keyOn;
    keyOff_ = keyOff;
    pc_ = pc;
    dest_ = dest;
    a_ = a;
    x_ = x;
    y_ = y;
    sp_ = sp;
    psw_ = psw;
    cursor_ = cursor;
    seen_ = seen;
    clocked_ = clocked;
    samplePhase_ = samplePhase;
    std::copy(std::begin(timerDivider), std::end(timerDivider), std::begin(timerDivider_));
    std::copy(std::begin(timerStage), std::end(timerStage), std::begin(timerStage_));
    echoPos_ = echoPos;
    echoDelay_ = echoLeft.empty() ? 1 : static_cast<int>(echoLeft.size());
    noise_ = noise;
    cycleAccum_ = cycleAccum;
    outputAccum_ = outputAccum;
    booting_ = booting;
    transferring_ = transferring;
    halted_ = halted;
    hasIpl_ = hasIpl;
    std::copy(std::begin(ipl), std::end(ipl), std::begin(ipl_));
    echoLeft_ = std::move(echoLeft);
    echoRight_ = std::move(echoRight);
    sleeping_ = sleeping;
    irqPending_ = irqPending;
    return true;
}

}  // namespace iris::systems::snes
