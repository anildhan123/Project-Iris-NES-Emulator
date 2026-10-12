#include "iris/nes/apu.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::uint8_t kLength[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

constexpr std::uint8_t kDuty[4] = {0x40, 0x60, 0x78, 0x9F};

constexpr std::uint16_t kNoisePeriod[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
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
    frameDivider_ = 0;
    samplePhase_ = 0;
    fiveStep_ = false;
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
        noise_.period = kNoisePeriod[value & 0x0F];
        break;
    case 0x400F:
        if (noise_.enabled) {
            noise_.length = kLength[value >> 3];
        }
        noise_.envelope.start = true;
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
        break;
    case 0x4017:
        fiveStep_ = (value & 0x80) != 0;
        frameDivider_ = 0;
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
    return value;
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
    const float tndIn = static_cast<float>(triangle_.output()) / 8227.0f + static_cast<float>(noise_.output()) / 12241.0f;
    const float tndOut = tndIn == 0.0f ? 0.0f : 159.79f / ((1.0f / tndIn) + 100.0f);
    return pulseOut + tndOut;
}

void Apu::clock(std::vector<float>& samples) {
    if ((frameDivider_ & 1) == 0) {
        pulse1_.clockTimer();
        pulse2_.clockTimer();
        noise_.clockTimer();
    }
    triangle_.clockTimer();
    ++frameDivider_;
    if (frameDivider_ == 7457) {
        clockQuarter();
    } else if (frameDivider_ >= 14914) {
        frameDivider_ = 0;
        clockQuarter();
        clockHalf();
    }

    samplePhase_ += kSampleRate;
    if (samplePhase_ >= kCpuClock) {
        samplePhase_ -= kCpuClock;
        samples.push_back(mix());
    }
}

}  // namespace iris::systems::nes
