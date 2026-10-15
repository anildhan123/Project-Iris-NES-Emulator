#include "iris/snes/apu.hpp"

#include <algorithm>

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

}  // namespace

void Apu::power() {
    ram_.assign(65536, 0);
    raw_.clear();
    samples_.clear();
    echoLeft_.assign(8, 0);
    echoRight_.assign(8, 0);
    for (Voice& voice : voice_) {
        voice = Voice{};
    }
    std::fill(std::begin(dsp_), std::end(dsp_), static_cast<std::uint8_t>(0));
    dsp_[0x6C] = 0x80;
    std::fill(std::begin(cpuIn_), std::end(cpuIn_), static_cast<std::uint8_t>(0));
    control_ = 0;
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
    noise_ = 0x4000;
    cycleAccum_ = 0;
    outputAccum_ = 0;
    halted_ = false;
    enterBoot();
}

void Apu::enterBoot() {
    booting_ = true;
    transferring_ = false;
    halted_ = false;
    seen_ = cpuIn_[0];
    cpuOut_[0] = 0xAA;
    cpuOut_[1] = 0xBB;
    cpuOut_[2] = 0;
    cpuOut_[3] = 0;
}

void Apu::beginFrame() {
    clocked_ = 0;
    raw_.clear();
}

void Apu::catchUp(int masterCycle, int masterClock) {
    if (booting_) {
        bootPoll();
    }
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
            raw_.push_back(renderSample());
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
    samples_.assign(static_cast<std::size_t>(count), 0.0f);
    if (raw_.empty()) {
        return;
    }
    const int last = static_cast<int>(raw_.size()) - 1;
    for (int index = 0; index < count; ++index) {
        const double position = static_cast<double>(index) * static_cast<double>(raw_.size()) / static_cast<double>(count);
        const int base = static_cast<int>(position);
        const float fraction = static_cast<float>(position - base);
        const float first = raw_[static_cast<std::size_t>(base)];
        const int next = base < last ? base + 1 : last;
        const float second = raw_[static_cast<std::size_t>(next)];
        float mixed = first + (second - first) * fraction;
        if (mixed > 1.0f) {
            mixed = 1.0f;
        }
        if (mixed < -1.0f) {
            mixed = -1.0f;
        }
        samples_[static_cast<std::size_t>(index)] = mixed;
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
        halted_ = false;
        pc_ = entry;
        sp_ = 0xEF;
        psw_ = 0;
        return;
    }
    dest_ = static_cast<std::uint16_t>(cpuIn_[2] | (static_cast<std::uint16_t>(cpuIn_[3]) << 8));
    cursor_ = 0xFF;
}

void Apu::run(int cycles) {
    int left = cycles;
    while (left > 0 && !booting_ && !halted_) {
        int used = instruction();
        if (used < 1) {
            used = 2;
        }
        tickTimers(used);
        left -= used;
    }
    if (left > 0) {
        tickTimers(left);
    }
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
        return dspAddress_ == 0x7C ? endx_ : dsp_[dspAddress_];
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
    case 0x00F1:
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
            if ((value & (1 << index)) == 0) {
                timerDivider_[index] = 0;
                timerStage_[index] = 0;
            }
        }
        break;
    case 0x00F2:
        dspAddress_ = static_cast<std::uint8_t>(value & 0x7F);
        break;
    case 0x00F3:
        if (dspAddress_ == 0x4C) {
            keyOn_ = static_cast<std::uint8_t>(keyOn_ | value);
        } else if (dspAddress_ == 0x5C) {
            keyOff_ = static_cast<std::uint8_t>(keyOff_ | value);
        } else if (dspAddress_ != 0x7C && (dspAddress_ & 0x0F) != 0x08 && (dspAddress_ & 0x0F) != 0x09) {
            dsp_[dspAddress_] = value;
        }
        break;
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
        const int nibbles[2] = {byte >> 4, byte & 0x0F};
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
    current.playing = true;
    current.release = false;
    decodeBlock(current);
    endx_ = static_cast<std::uint8_t>(endx_ & ~(1 << voice));
}

float Apu::renderSample() {
    const std::uint8_t flags = dsp_[0x6C];
    if (keyOn_ != 0) {
        for (int voice = 0; voice < 8; ++voice) {
            if ((keyOn_ & (1 << voice)) != 0) {
                keyOn(voice);
            }
        }
        keyOn_ = 0;
    }
    if (keyOff_ != 0) {
        for (int voice = 0; voice < 8; ++voice) {
            if ((keyOff_ & (1 << voice)) != 0) {
                voice_[voice].release = true;
            }
        }
        keyOff_ = 0;
    }
    if ((flags & 0x80) != 0) {
        endx_ = 0;
        return 0.0f;
    }
    const int noiseBit = (noise_ ^ (noise_ >> 1)) & 1;
    noise_ = (noise_ >> 1) | (noiseBit << 14);
    int left = 0;
    int right = 0;
    for (int index = 0; index < 8; ++index) {
        Voice& voice = voice_[index];
        const int base = index << 4;
        if (!voice.playing) {
            dsp_[base | 0x08] = 0;
            dsp_[base | 0x09] = 0;
            continue;
        }
        if (voice.release) {
            voice.envelope -= 8;
            if (voice.envelope <= 0) {
                voice.envelope = 0;
                voice.playing = false;
            }
        } else if ((dsp_[base | 0x05] & 0x80) == 0) {
            const int gain = dsp_[base | 0x07];
            if ((gain & 0x80) == 0) {
                voice.envelope = (gain & 0x7F) << 4;
            } else {
                const int step = 1 << ((gain & 0x1F) / 4);
                const int mode = (gain >> 5) & 0x03;
                voice.envelope += (mode >= 2 ? step : -step);
            }
        } else {
            const int sustain = (((dsp_[base | 0x06] >> 5) & 0x07) + 1) * 0x100;
            if (voice.envelope < 0x7FF) {
                voice.envelope += 16 + (dsp_[base | 0x05] & 0x0F);
            } else if (voice.envelope > sustain) {
                voice.envelope -= 8;
            } else if ((dsp_[base | 0x06] & 0x1F) != 0) {
                voice.envelope -= 1;
            }
        }
        if (voice.envelope < 0) {
            voice.envelope = 0;
        }
        if (voice.envelope > 0x7FF) {
            voice.envelope = 0x7FF;
        }
        int cursor = voice.phase >> 12;
        while (cursor >= 16 && voice.playing) {
            cursor -= 16;
            voice.phase -= 16 << 12;
            if (voice.blockEnds) {
                endx_ = static_cast<std::uint8_t>(endx_ | (1 << index));
                if (voice.blockLoops) {
                    voice.pointer = voice.loop;
                } else {
                    voice.playing = false;
                    break;
                }
            }
            if (voice.playing) {
                decodeBlock(voice);
            }
        }
        int sample = 0;
        if (voice.playing && (dsp_[0x3D] & (1 << index)) != 0) {
            sample = (noise_ & 1) != 0 ? 12000 : -12000;
        } else if (voice.playing) {
            sample = voice.decoded[cursor];
        }
        int pitch = dsp_[base | 0x02] | ((dsp_[base | 0x03] & 0x3F) << 8);
        if (index > 0 && (dsp_[0x2D] & (1 << index)) != 0) {
            pitch += (pitch * (voice_[index - 1].output >> 8)) >> 7;
        }
        if (pitch < 0) {
            pitch = 0;
        }
        if (pitch > 0x3FFF) {
            pitch = 0x3FFF;
        }
        voice.phase += pitch;
        const int shaped = (sample * voice.envelope) >> 11;
        voice.output = shaped;
        dsp_[base | 0x08] = static_cast<std::uint8_t>((voice.envelope >> 4) & 0x7F);
        dsp_[base | 0x09] = static_cast<std::uint8_t>(shaped >> 8);
        const auto volumeLeft = static_cast<std::int8_t>(dsp_[base]);
        const auto volumeRight = static_cast<std::int8_t>(dsp_[base | 0x01]);
        left += (shaped * volumeLeft) >> 7;
        right += (shaped * volumeRight) >> 7;
    }
    const auto mainLeft = static_cast<std::int8_t>(dsp_[0x0C]);
    const auto mainRight = static_cast<std::int8_t>(dsp_[0x1C]);
    left = (left * mainLeft) >> 7;
    right = (right * mainRight) >> 7;
    const int delay = (dsp_[0x7D] & 0x0F) * 512;
    if (delay > 1 && (flags & 0x20) == 0) {
        if (static_cast<int>(echoLeft_.size()) != delay) {
            echoLeft_.assign(static_cast<std::size_t>(delay), 0);
            echoRight_.assign(static_cast<std::size_t>(delay), 0);
            echoPos_ = 0;
        }
        int filteredLeft = 0;
        int filteredRight = 0;
        for (int tap = 0; tap < 8; ++tap) {
            int position = echoPos_ - tap;
            if (position < 0) {
                position += delay;
            }
            const auto coefficient = static_cast<std::int8_t>(dsp_[0x0F + tap * 0x10]);
            filteredLeft += (echoLeft_[static_cast<std::size_t>(position)] * coefficient) >> 7;
            filteredRight += (echoRight_[static_cast<std::size_t>(position)] * coefficient) >> 7;
        }
        const auto echoLeft = static_cast<std::int8_t>(dsp_[0x2C]);
        const auto echoRight = static_cast<std::int8_t>(dsp_[0x3C]);
        left += (filteredLeft * echoLeft) >> 7;
        right += (filteredRight * echoRight) >> 7;
        const auto feedback = static_cast<std::int8_t>(dsp_[0x0D]);
        echoLeft_[static_cast<std::size_t>(echoPos_)] = clamp16(left + ((left * feedback) >> 7));
        echoRight_[static_cast<std::size_t>(echoPos_)] = clamp16(right + ((right * feedback) >> 7));
        echoPos_ = (echoPos_ + 1) % delay;
    }
    if ((flags & 0x40) != 0) {
        return 0.0f;
    }
    const int mixed = clamp16((left + right) / 2);
    return static_cast<float>(mixed) / 32768.0f;
}

int Apu::instruction() {
    if (pc_ == 0xFFC0) {
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
    case 0xFF:
        halted_ = true;
        return 3;
    default:
        return 2;
    }
}

}  // namespace iris::systems::snes
