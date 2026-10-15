#include "iris/snes/dsp1.hpp"

#include <cmath>

namespace iris::systems::snes {
namespace {

std::int16_t clamp16(int value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return static_cast<std::int16_t>(value);
}

void anglePair(std::uint16_t angle, int radius, std::int16_t& sine, std::int16_t& cosine) {
    const double turns = static_cast<double>(angle) * 6.283185307179586 / 65536.0;
    sine = clamp16(static_cast<int>(std::lround(static_cast<double>(radius) * std::sin(turns))));
    cosine = clamp16(static_cast<int>(std::lround(static_cast<double>(radius) * std::cos(turns))));
}

}  // namespace

void Dsp1::power() {
    phase_ = Phase::Command;
    byteIndex_ = 0;
    inputCount_ = 0;
    inputNeed_ = 0;
    outputCount_ = 0;
    outputIndex_ = 0;
    word_ = 0;
    command_ = 0;
}

bool Dsp1::read(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t& value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    const bool data = hiRom ? mapped <= 0x0F && address >= 0x6000 && address < 0x7000
                            : mapped >= 0x30 && mapped <= 0x3F && address >= 0x8000 && address < 0xC000;
    const bool status = hiRom ? mapped <= 0x0F && address >= 0x7000 && address < 0x8000
                              : mapped >= 0x30 && mapped <= 0x3F && address >= 0xC000;
    if (status) {
        value = 0x80;
        return true;
    }
    if (!data) {
        return false;
    }
    if (phase_ != Phase::Output || outputIndex_ >= outputCount_) {
        value = 0;
        return true;
    }
    const auto word = static_cast<std::uint16_t>(output_[outputIndex_]);
    value = byteIndex_ == 0 ? static_cast<std::uint8_t>(word) : static_cast<std::uint8_t>(word >> 8);
    if (++byteIndex_ >= 2) {
        byteIndex_ = 0;
        ++outputIndex_;
        if (outputIndex_ >= outputCount_) {
            phase_ = Phase::Command;
        }
    }
    return true;
}

bool Dsp1::write(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    const bool data = hiRom ? mapped <= 0x0F && address >= 0x6000 && address < 0x7000
                            : mapped >= 0x30 && mapped <= 0x3F && address >= 0x8000 && address < 0xC000;
    if (!data || phase_ == Phase::Output) {
        return data;
    }
    if (byteIndex_ == 0) {
        word_ = value;
        byteIndex_ = 1;
        return true;
    }
    word_ = static_cast<std::uint16_t>(word_ | (static_cast<std::uint16_t>(value) << 8));
    byteIndex_ = 0;
    acceptWord(word_);
    return true;
}

void Dsp1::acceptWord(std::uint16_t word) {
    if (phase_ == Phase::Command) {
        command_ = static_cast<std::uint8_t>(word);
        inputNeed_ = inputsFor(command_);
        inputCount_ = 0;
        outputCount_ = 0;
        outputIndex_ = 0;
        phase_ = inputNeed_ == 0 ? Phase::Command : Phase::Input;
        if (inputNeed_ == 0) {
            execute();
        }
        return;
    }
    if (inputCount_ < 8) {
        input_[inputCount_] = static_cast<std::int16_t>(word);
    }
    ++inputCount_;
    if (inputCount_ >= inputNeed_) {
        execute();
    }
}

void Dsp1::execute() {
    outputCount_ = 0;
    outputIndex_ = 0;
    byteIndex_ = 0;
    if (command_ == 0x00 && inputCount_ >= 2) {
        output_[0] = clamp16((static_cast<int>(input_[0]) * static_cast<int>(input_[1])) >> 15);
        outputCount_ = 1;
    } else if (command_ == 0x10 && inputCount_ >= 1) {
        if (input_[0] == 0) {
            output_[0] = 32767;
        } else {
            const int magnitude = input_[0] < 0 ? -input_[0] : input_[0];
            const int inverse = 32768 / magnitude;
            output_[0] = clamp16(input_[0] < 0 ? -inverse : inverse);
        }
        outputCount_ = 1;
    } else if (command_ == 0x04 && inputCount_ >= 2) {
        anglePair(static_cast<std::uint16_t>(input_[0]), input_[1], output_[0], output_[1]);
        outputCount_ = 2;
    } else if (command_ == 0x0C && inputCount_ >= 3) {
        std::int16_t sine = 0;
        std::int16_t cosine = 0;
        anglePair(static_cast<std::uint16_t>(input_[0]), 32767, sine, cosine);
        const int x = input_[1];
        const int y = input_[2];
        output_[0] = clamp16((x * cosine - y * sine) / 32767);
        output_[1] = clamp16((x * sine + y * cosine) / 32767);
        outputCount_ = 2;
    }
    phase_ = outputCount_ == 0 ? Phase::Command : Phase::Output;
}

int Dsp1::inputsFor(std::uint8_t command) {
    switch (command) {
    case 0x00:
        return 2;
    case 0x04:
        return 2;
    case 0x0C:
        return 3;
    case 0x10:
        return 1;
    default:
        return 0;
    }
}

}  // namespace iris::systems::snes
