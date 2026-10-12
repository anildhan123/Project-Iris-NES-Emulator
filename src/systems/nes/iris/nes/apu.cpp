#include "iris/nes/apu.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::uint8_t kLength[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

constexpr std::uint8_t kDuty[4] = {0x40, 0x60, 0x78, 0x9F};

constexpr std::uint16_t kNoiseNtsc[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

constexpr std::uint16_t kNoisePal[16] = {
    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778,
};

constexpr std::uint16_t kDmcNtsc[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54,
};

constexpr std::uint16_t kDmcPal[16] = {
    398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50,
};

constexpr int kQuarter = 1;
constexpr int kHalf = 2;
constexpr int kIrq = 4;
constexpr int kReset = 8;

constexpr int kFrame4Ntsc[] = {
    7457, kQuarter, 14913, kQuarter | kHalf, 22371, kQuarter, 29828, kIrq, 29829, kQuarter | kHalf | kIrq, 29830, kReset,
};

constexpr int kFrame5Ntsc[] = {
    7457, kQuarter, 14913, kQuarter | kHalf, 22371, kQuarter, 37281, kQuarter | kHalf, 37282, kReset,
};

constexpr int kFrame4Pal[] = {
    8313, kQuarter, 16627, kQuarter | kHalf, 24939, kQuarter, 33252, kIrq, 33253, kQuarter | kHalf | kIrq, 33254, kReset,
};

constexpr int kFrame5Pal[] = {
    8313, kQuarter, 16627, kQuarter | kHalf, 24939, kQuarter, 41565, kQuarter | kHalf, 41566, kReset,
};

constexpr std::uint8_t kTriangle[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

}  // namespace

void Apu::Envelope::clock() {
    if (start) {
        start = false;
        decay = 15;
        divider = volume;
        return;
    }
    if (divider == 0) {
        divider = volume;
        if (decay > 0) {
            --decay;
        } else if (loop) {
            decay = 15;
        }
    } else {
        --divider;
    }
}

std::uint8_t Apu::Envelope::output() const { return constant ? volume : decay; }

void Apu::Pulse::writeControl(std::uint8_t value) {
    duty = static_cast<std::uint8_t>(value >> 6);
    halt = (value & 0x20) != 0;
    envelope.loop = halt;
    envelope.constant = (value & 0x10) != 0;
    envelope.volume = static_cast<std::uint8_t>(value & 0x0F);
}

void Apu::Pulse::writeSweep(std::uint8_t value) {
    sweepEnabled = (value & 0x80) != 0;
    sweepPeriod = static_cast<std::uint8_t>((value >> 4) & 0x07);
    sweepNegate = (value & 0x08) != 0;
    sweepShift = static_cast<std::uint8_t>(value & 0x07);
    sweepReload = true;
}

void Apu::Pulse::writeLow(std::uint8_t value) { period = static_cast<std::uint16_t>((period & 0xFF00) | value); }

void Apu::Pulse::writeHigh(std::uint8_t value, bool /*onesComplement*/) {
    period = static_cast<std::uint16_t>((period & 0x00FF) | ((value & 0x07) << 8));
    if (enabled) {
        length = kLength[value >> 3];
    }
    envelope.start = true;
    sequence = 0;
}

void Apu::Pulse::clockTimer() {
    if (timer == 0) {
        timer = period;
        sequence = static_cast<std::uint8_t>((sequence + 1) & 0x07);
    } else {
        --timer;
    }
}

void Apu::Pulse::clockLength() {
    if (!halt && length > 0) {
        --length;
    }
}

int Apu::Pulse::targetPeriod(bool onesComplement) const {
    const int change = period >> sweepShift;
    if (sweepNegate) {
        return static_cast<int>(period) - change - (onesComplement ? 1 : 0);
    }
    return static_cast<int>(period) + change;
}

void Apu::Pulse::clockSweep(bool onesComplement) {
    const int target = targetPeriod(onesComplement);
    if (sweepDivider == 0 && sweepEnabled && sweepShift != 0 && period >= 8 && target >= 0 && target <= 0x7FF) {
        period = static_cast<std::uint16_t>(target);
    }
    if (sweepDivider == 0 || sweepReload) {
        sweepDivider = sweepPeriod;
        sweepReload = false;
    } else {
        --sweepDivider;
    }
}

std::uint8_t Apu::Pulse::output(bool onesComplement) const {
    if (!enabled || length == 0 || period < 8 || targetPeriod(onesComplement) > 0x7FF) {
        return 0;
    }
    if (((kDuty[duty] >> (7 - sequence)) & 0x01) == 0) {
        return 0;
    }
    return envelope.output();
}

void Apu::Triangle::clockTimer() {
    if (timer == 0) {
        timer = period;
        if (length > 0 && linear > 0) {
            sequence = static_cast<std::uint8_t>((sequence + 1) & 0x1F);
        }
    } else {
        --timer;
    }
}

void Apu::Triangle::clockLinear() {
    if (linearReload) {
        linear = linearPeriod;
    } else if (linear > 0) {
        --linear;
    }
    if (!halt) {
        linearReload = false;
    }
}

void Apu::Triangle::clockLength() {
    if (!halt && length > 0) {
        --length;
    }
}

std::uint8_t Apu::Triangle::output() const {
    if (!enabled || length == 0 || linear == 0) {
        return 0;
    }
    return kTriangle[sequence];
}

void Apu::Noise::clockTimer() {
    if (timer == 0) {
        timer = period;
        const int tap = mode ? 6 : 1;
        const std::uint16_t feedback = static_cast<std::uint16_t>((shift ^ (shift >> tap)) & 0x01);
        shift = static_cast<std::uint16_t>((shift >> 1) | (feedback << 14));
    } else {
        --timer;
    }
}

void Apu::Noise::clockLength() {
    if (!halt && length > 0) {
        --length;
    }
}

std::uint8_t Apu::Noise::output() const {
    if (!enabled || length == 0 || (shift & 0x01) != 0) {
        return 0;
    }
    return envelope.output();
}

void Apu::reset() {
    pulse1_ = {};
    pulse2_ = {};
    triangle_ = {};
    noise_ = {};
    noise_.shift = 1;
    frameCycle_ = 0;
    samplePhase_ = 0;
    evenCycle_ = true;
    fiveStep_ = false;
    irqInhibit_ = false;
    frameIrq_ = false;
    dmcIrqEnable_ = false;
    dmcLoop_ = false;
    dmcIrq_ = false;
    dmcSilence_ = true;
    dmcSampleEmpty_ = true;
    dmcFetch_ = false;
    dmcOutput_ = 0;
    dmcShift_ = 0;
    dmcSample_ = 0;
    dmcBits_ = 8;
    dmcRate_ = 0;
    dmcAddress_ = 0xC000;
    dmcSampleAddress_ = 0xC000;
    dmcBytesRemaining_ = 0;
    dmcSampleLength_ = 1;
    if (noisePeriods_ == nullptr) {
        setRegion(Region::Ntsc);
    }
    dmcPeriod_ = dmcPeriods_[0];
    dmcTimer_ = dmcPeriod_;
}

void Apu::setRegion(Region region) {
    const bool pal = region == Region::Pal;
    cpuClock_ = regionTiming(region).cpuClock;
    noisePeriods_ = pal ? kNoisePal : kNoiseNtsc;
    dmcPeriods_ = pal ? kDmcPal : kDmcNtsc;
    frame4_ = pal ? kFrame4Pal : kFrame4Ntsc;
    frame5_ = pal ? kFrame5Pal : kFrame5Ntsc;
    dmcPeriod_ = dmcPeriods_[dmcRate_ & 0x0F];
    if (dmcTimer_ == 0) {
        dmcTimer_ = dmcPeriod_;
    }
}

void Apu::write(std::uint16_t address, std::uint8_t value) {
    switch (address) {
    case 0x4000: pulse1_.writeControl(value); break;
    case 0x4001: pulse1_.writeSweep(value); break;
    case 0x4002: pulse1_.writeLow(value); break;
    case 0x4003: pulse1_.writeHigh(value, true); break;
    case 0x4004: pulse2_.writeControl(value); break;
    case 0x4005: pulse2_.writeSweep(value); break;
    case 0x4006: pulse2_.writeLow(value); break;
    case 0x4007: pulse2_.writeHigh(value, false); break;
    case 0x4008:
        triangle_.halt = (value & 0x80) != 0;
        triangle_.linearPeriod = static_cast<std::uint8_t>(value & 0x7F);
        break;
    case 0x400A:
        triangle_.period = static_cast<std::uint16_t>((triangle_.period & 0xFF00) | value);
        break;
    case 0x400B:
        triangle_.period = static_cast<std::uint16_t>((triangle_.period & 0x00FF) | ((value & 0x07) << 8));
        if (triangle_.enabled) {
            triangle_.length = kLength[value >> 3];
        }
        triangle_.linearReload = true;
        break;
    case 0x400C:
        noise_.halt = (value & 0x20) != 0;
        noise_.envelope.loop = noise_.halt;
        noise_.envelope.constant = (value & 0x10) != 0;
        noise_.envelope.volume = static_cast<std::uint8_t>(value & 0x0F);
        break;
    case 0x400E:
        noise_.mode = (value & 0x80) != 0;
        noise_.period = noisePeriods_[value & 0x0F];
        break;
    case 0x400F:
        if (noise_.enabled) {
            noise_.length = kLength[value >> 3];
        }
        noise_.envelope.start = true;
        break;
    case 0x4010:
        dmcIrqEnable_ = (value & 0x80) != 0;
        dmcLoop_ = (value & 0x40) != 0;
        dmcRate_ = static_cast<std::uint8_t>(value & 0x0F);
        dmcPeriod_ = dmcPeriods_[dmcRate_];
        if (!dmcIrqEnable_) {
            dmcIrq_ = false;
        }
        break;
    case 0x4011:
        dmcOutput_ = static_cast<std::uint8_t>(value & 0x7F);
        break;
    case 0x4012:
        dmcSampleAddress_ = static_cast<std::uint16_t>(0xC000 + static_cast<unsigned>(value) * 64);
        break;
    case 0x4013:
        dmcSampleLength_ = static_cast<int>(value) * 16 + 1;
        break;
    case 0x4015:
        pulse1_.enabled = (value & 0x01) != 0;
        pulse2_.enabled = (value & 0x02) != 0;
        triangle_.enabled = (value & 0x04) != 0;
        noise_.enabled = (value & 0x08) != 0;
        if (!pulse1_.enabled) pulse1_.length = 0;
        if (!pulse2_.enabled) pulse2_.length = 0;
        if (!triangle_.enabled) triangle_.length = 0;
        if (!noise_.enabled) noise_.length = 0;
        dmcIrq_ = false;
        if ((value & 0x10) != 0) {
            if (dmcBytesRemaining_ == 0) {
                dmcAddress_ = dmcSampleAddress_;
                dmcBytesRemaining_ = dmcSampleLength_;
            }
        } else {
            dmcBytesRemaining_ = 0;
        }
        break;
    case 0x4017:
        fiveStep_ = (value & 0x80) != 0;
        irqInhibit_ = (value & 0x40) != 0;
        frameIrq_ = false;
        frameCycle_ = 0;
        if (fiveStep_) {
            clockQuarter();
            clockHalf();
        }
        break;
    default:
        break;
    }
}

std::uint8_t Apu::readStatus() {
    std::uint8_t value = 0;
    if (pulse1_.length > 0) value = static_cast<std::uint8_t>(value | 0x01);
    if (pulse2_.length > 0) value = static_cast<std::uint8_t>(value | 0x02);
    if (triangle_.length > 0) value = static_cast<std::uint8_t>(value | 0x04);
    if (noise_.length > 0) value = static_cast<std::uint8_t>(value | 0x08);
    if (dmcBytesRemaining_ > 0) value = static_cast<std::uint8_t>(value | 0x10);
    if (frameIrq_) value = static_cast<std::uint8_t>(value | 0x40);
    if (dmcIrq_) value = static_cast<std::uint8_t>(value | 0x80);
    frameIrq_ = false;
    return value;
}

bool Apu::pollDmc(std::uint16_t& address) {
    if (!dmcFetch_) {
        return false;
    }
    dmcFetch_ = false;
    address = dmcAddress_;
    return true;
}

void Apu::supplyDmc(std::uint8_t value) {
    dmcSample_ = value;
    dmcSampleEmpty_ = false;
    if (dmcAddress_ == 0xFFFF) {
        dmcAddress_ = 0x8000;
    } else {
        ++dmcAddress_;
    }
    if (dmcBytesRemaining_ > 0) {
        --dmcBytesRemaining_;
    }
    if (dmcBytesRemaining_ == 0) {
        if (dmcLoop_) {
            dmcAddress_ = dmcSampleAddress_;
            dmcBytesRemaining_ = dmcSampleLength_;
        } else if (dmcIrqEnable_) {
            dmcIrq_ = true;
        }
    }
}

void Apu::clockQuarter() {
    pulse1_.envelope.clock();
    pulse2_.envelope.clock();
    triangle_.clockLinear();
    noise_.envelope.clock();
}

void Apu::clockHalf() {
    pulse1_.clockLength();
    pulse1_.clockSweep(true);
    pulse2_.clockLength();
    pulse2_.clockSweep(false);
    triangle_.clockLength();
    noise_.clockLength();
}

float Apu::mix() const {
    const float pulse = static_cast<float>(pulse1_.output(true) + pulse2_.output(false));
    const float pulseOut = pulse == 0.0f ? 0.0f : 95.88f / ((8128.0f / pulse) + 100.0f);
    const float tndIn = static_cast<float>(triangle_.output()) / 8227.0f + static_cast<float>(noise_.output()) / 12241.0f +
                        static_cast<float>(dmcOutput_) / 22638.0f;
    const float tndOut = tndIn == 0.0f ? 0.0f : 159.79f / ((1.0f / tndIn) + 100.0f);
    return pulseOut + tndOut;
}

void Apu::clockFrame() {
    if (frame4_ == nullptr) {
        setRegion(Region::Ntsc);
    }
    ++frameCycle_;
    const int* steps = fiveStep_ ? frame5_ : frame4_;
    const int count = fiveStep_ ? 5 : 6;
    bool reset = false;
    for (int index = 0; index < count; ++index) {
        if (frameCycle_ != steps[index * 2]) {
            continue;
        }
        const int events = steps[index * 2 + 1];
        if ((events & kQuarter) != 0) {
            clockQuarter();
        }
        if ((events & kHalf) != 0) {
            clockHalf();
        }
        if ((events & kIrq) != 0 && !irqInhibit_) {
            frameIrq_ = true;
        }
        if ((events & kReset) != 0) {
            reset = true;
        }
    }
    if (reset) {
        frameCycle_ = 0;
    }
}

void Apu::clockDmc() {
    if (dmcTimer_ > 0) {
        --dmcTimer_;
    }
    if (dmcTimer_ == 0) {
        dmcTimer_ = dmcPeriod_ == 0 ? 428 : dmcPeriod_;
        if (!dmcSilence_) {
            if ((dmcShift_ & 0x01) != 0) {
                if (dmcOutput_ <= 125) {
                    dmcOutput_ = static_cast<std::uint8_t>(dmcOutput_ + 2);
                }
            } else if (dmcOutput_ >= 2) {
                dmcOutput_ = static_cast<std::uint8_t>(dmcOutput_ - 2);
            }
            dmcShift_ = static_cast<std::uint8_t>(dmcShift_ >> 1);
        }
        --dmcBits_;
        if (dmcBits_ == 0) {
            dmcBits_ = 8;
            if (dmcSampleEmpty_) {
                dmcSilence_ = true;
            } else {
                dmcSilence_ = false;
                dmcShift_ = dmcSample_;
                dmcSampleEmpty_ = true;
            }
        }
    }
    if (dmcSampleEmpty_ && dmcBytesRemaining_ > 0 && !dmcFetch_) {
        dmcFetch_ = true;
    }
}

void Apu::clock(std::vector<float>& samples) {
    if (evenCycle_) {
        pulse1_.clockTimer();
        pulse2_.clockTimer();
        noise_.clockTimer();
    }
    evenCycle_ = !evenCycle_;
    triangle_.clockTimer();
    clockDmc();
    clockFrame();

    samplePhase_ += kSampleRate;
    if (samplePhase_ >= cpuClock_) {
        samplePhase_ -= cpuClock_;
        samples.push_back(mix());
    }
}

void Apu::captureEnvelope(ByteWriter& writer, const Envelope& envelope) {
    writer.boolean(envelope.start);
    writer.boolean(envelope.loop);
    writer.boolean(envelope.constant);
    writer.u8(envelope.volume);
    writer.u8(envelope.divider);
    writer.u8(envelope.decay);
}

bool Apu::readEnvelope(ByteReader& reader, Envelope& envelope) {
    envelope.start = reader.boolean();
    envelope.loop = reader.boolean();
    envelope.constant = reader.boolean();
    envelope.volume = reader.u8();
    envelope.divider = reader.u8();
    envelope.decay = reader.u8();
    return reader.ok();
}

void Apu::captureState(ByteWriter& writer) const {
    auto capturePulse = [&](const Pulse& pulse) {
        writer.boolean(pulse.enabled);
        writer.boolean(pulse.halt);
        writer.u8(pulse.duty);
        writer.u8(pulse.sequence);
        writer.u16(pulse.timer);
        writer.u16(pulse.period);
        writer.u8(pulse.length);
        writer.boolean(pulse.sweepEnabled);
        writer.u8(pulse.sweepPeriod);
        writer.u8(pulse.sweepDivider);
        writer.boolean(pulse.sweepNegate);
        writer.u8(pulse.sweepShift);
        writer.boolean(pulse.sweepReload);
        captureEnvelope(writer, pulse.envelope);
    };
    capturePulse(pulse1_);
    capturePulse(pulse2_);
    writer.boolean(triangle_.enabled);
    writer.boolean(triangle_.halt);
    writer.boolean(triangle_.linearReload);
    writer.u8(triangle_.linearPeriod);
    writer.u8(triangle_.linear);
    writer.u8(triangle_.sequence);
    writer.u16(triangle_.timer);
    writer.u16(triangle_.period);
    writer.u8(triangle_.length);
    writer.boolean(noise_.enabled);
    writer.boolean(noise_.mode);
    writer.u16(noise_.timer);
    writer.u16(noise_.period);
    writer.u16(noise_.shift);
    writer.u8(noise_.length);
    writer.boolean(noise_.halt);
    captureEnvelope(writer, noise_.envelope);
    writer.u32(static_cast<std::uint32_t>(frameCycle_));
    writer.u32(static_cast<std::uint32_t>(samplePhase_));
    writer.boolean(evenCycle_);
    writer.boolean(fiveStep_);
    writer.boolean(irqInhibit_);
    writer.boolean(frameIrq_);
    writer.boolean(dmcIrqEnable_);
    writer.boolean(dmcLoop_);
    writer.boolean(dmcIrq_);
    writer.boolean(dmcSilence_);
    writer.boolean(dmcSampleEmpty_);
    writer.boolean(dmcFetch_);
    writer.u8(dmcOutput_);
    writer.u8(dmcShift_);
    writer.u8(dmcSample_);
    writer.u8(dmcBits_);
    writer.u8(dmcRate_);
    writer.u16(dmcTimer_);
    writer.u16(dmcPeriod_);
    writer.u16(dmcAddress_);
    writer.u16(dmcSampleAddress_);
    writer.u32(static_cast<std::uint32_t>(dmcBytesRemaining_));
    writer.u32(static_cast<std::uint32_t>(dmcSampleLength_));
}

bool Apu::applyState(ByteReader& reader) {
    auto readPulse = [&](Pulse& pulse) {
        pulse.enabled = reader.boolean();
        pulse.halt = reader.boolean();
        pulse.duty = reader.u8();
        pulse.sequence = reader.u8();
        pulse.timer = reader.u16();
        pulse.period = reader.u16();
        pulse.length = reader.u8();
        pulse.sweepEnabled = reader.boolean();
        pulse.sweepPeriod = reader.u8();
        pulse.sweepDivider = reader.u8();
        pulse.sweepNegate = reader.boolean();
        pulse.sweepShift = reader.u8();
        pulse.sweepReload = reader.boolean();
        return readEnvelope(reader, pulse.envelope);
    };
    Pulse pulse1;
    Pulse pulse2;
    Triangle triangle;
    Noise noise;
    if (!readPulse(pulse1) || !readPulse(pulse2)) {
        return false;
    }
    triangle.enabled = reader.boolean();
    triangle.halt = reader.boolean();
    triangle.linearReload = reader.boolean();
    triangle.linearPeriod = reader.u8();
    triangle.linear = reader.u8();
    triangle.sequence = reader.u8();
    triangle.timer = reader.u16();
    triangle.period = reader.u16();
    triangle.length = reader.u8();
    noise.enabled = reader.boolean();
    noise.mode = reader.boolean();
    noise.timer = reader.u16();
    noise.period = reader.u16();
    noise.shift = reader.u16();
    noise.length = reader.u8();
    noise.halt = reader.boolean();
    if (!readEnvelope(reader, noise.envelope)) {
        return false;
    }
    const int frameCycle = static_cast<int>(reader.u32());
    const int samplePhase = static_cast<int>(reader.u32());
    const bool evenCycle = reader.boolean();
    const bool fiveStep = reader.boolean();
    const bool irqInhibit = reader.boolean();
    const bool frameIrq = reader.boolean();
    const bool dmcIrqEnable = reader.boolean();
    const bool dmcLoop = reader.boolean();
    const bool dmcIrq = reader.boolean();
    const bool dmcSilence = reader.boolean();
    const bool dmcSampleEmpty = reader.boolean();
    const bool dmcFetch = reader.boolean();
    const std::uint8_t dmcOutput = reader.u8();
    const std::uint8_t dmcShift = reader.u8();
    const std::uint8_t dmcSample = reader.u8();
    const std::uint8_t dmcBits = reader.u8();
    const std::uint8_t dmcRate = reader.u8();
    const std::uint16_t dmcTimer = reader.u16();
    const std::uint16_t dmcPeriod = reader.u16();
    const std::uint16_t dmcAddress = reader.u16();
    const std::uint16_t dmcSampleAddress = reader.u16();
    const int dmcBytesRemaining = static_cast<int>(reader.u32());
    const int dmcSampleLength = static_cast<int>(reader.u32());
    if (!reader.ok() || !reader.exhausted() || pulse1.duty > 3 || pulse2.duty > 3 || dmcRate > 15 || dmcOutput > 127 ||
        dmcBits == 0 || dmcBits > 8 || dmcPeriod == 0 || samplePhase < 0 || samplePhase > 2000000 || frameCycle < 0 ||
        frameCycle > 100000 || dmcBytesRemaining < 0 || dmcSampleLength < 1 || dmcSampleLength > 100000) {
        return false;
    }
    pulse1_ = pulse1;
    pulse2_ = pulse2;
    triangle_ = triangle;
    noise_ = noise;
    frameCycle_ = frameCycle;
    samplePhase_ = samplePhase;
    evenCycle_ = evenCycle;
    fiveStep_ = fiveStep;
    irqInhibit_ = irqInhibit;
    frameIrq_ = frameIrq;
    dmcIrqEnable_ = dmcIrqEnable;
    dmcLoop_ = dmcLoop;
    dmcIrq_ = dmcIrq;
    dmcSilence_ = dmcSilence;
    dmcSampleEmpty_ = dmcSampleEmpty;
    dmcFetch_ = dmcFetch;
    dmcOutput_ = dmcOutput;
    dmcShift_ = dmcShift;
    dmcSample_ = dmcSample;
    dmcBits_ = dmcBits;
    dmcRate_ = dmcRate;
    dmcTimer_ = dmcTimer;
    dmcPeriod_ = dmcPeriod;
    dmcAddress_ = dmcAddress;
    dmcSampleAddress_ = dmcSampleAddress;
    dmcBytesRemaining_ = dmcBytesRemaining;
    dmcSampleLength_ = dmcSampleLength;
    return true;
}

}  // namespace iris::systems::nes
