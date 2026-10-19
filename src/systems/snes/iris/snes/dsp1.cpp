#include "iris/snes/dsp1.hpp"

#include "iris/byte_io.hpp"

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
    for (std::int16_t& value : input_) {
        value = 0;
    }
    for (std::int16_t& value : output_) {
        value = 0;
    }
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
    if (chip_ == Chip::Dsp2) {
        // Dungeon Master bitmap convert and scale helpers (host arithmetic, no ROM dump).
        if (command_ == 0x01 && inputCount_ >= 1) {
            const int bits = input_[0] & 0xFF;
            outputCount_ = bits > 0 && bits <= 8 ? bits : 1;
            for (int index = 0; index < outputCount_; ++index) {
                output_[index] = static_cast<std::int16_t>(0xFFFF);
            }
        } else if (command_ == 0x03 && inputCount_ >= 1) {
            output_[0] = input_[0];
            outputCount_ = 1;
        } else if (command_ == 0x05 && inputCount_ >= 2) {
            output_[0] = clamp16((static_cast<int>(input_[0]) * static_cast<int>(input_[1])) >> 8);
            outputCount_ = 1;
        } else if (command_ == 0x09 && inputCount_ >= 1) {
            const int bits = input_[0] & 0xFF;
            outputCount_ = bits > 0 && bits <= 8 ? bits : 1;
            for (int index = 0; index < outputCount_; ++index) {
                output_[index] = static_cast<std::int16_t>(1 << (7 - index));
            }
        } else {
            const int words = inputCount_ > 0 ? (input_[0] & 0xFF) : 0;
            outputCount_ = words > 0 && words <= 8 ? words : 1;
            for (int index = 0; index < outputCount_; ++index) {
                output_[index] = 0;
            }
        }
    } else if (chip_ == Chip::Dsp3) {
        if (command_ == 0x2E || command_ == 0x2F) {
            output_[0] = 0;
            output_[1] = 0;
            output_[2] = inputCount_ > 0 ? input_[0] : 0;
            outputCount_ = 3;
        } else if (command_ == 0x03) {
            output_[0] = inputCount_ > 0 ? input_[0] : 0;
            output_[1] = 0;
            outputCount_ = 2;
        } else if (command_ == 0x06 && inputCount_ >= 2) {
            output_[0] = clamp16(static_cast<int>(input_[0]) + static_cast<int>(input_[1]));
            output_[1] = clamp16(static_cast<int>(input_[0]) - static_cast<int>(input_[1]));
            outputCount_ = 2;
        } else if (command_ == 0x10 && inputCount_ >= 3) {
            const int dx = input_[0];
            const int dy = input_[1];
            const int scale = input_[2] == 0 ? 1 : input_[2];
            output_[0] = clamp16(dx * 256 / scale);
            output_[1] = clamp16(dy * 256 / scale);
            outputCount_ = 2;
        } else {
            output_[0] = inputCount_ > 0 ? input_[0] : 0;
            outputCount_ = 1;
        }
    } else if (chip_ == Chip::Dsp4) {
        if (command_ == 0x00 || command_ == 0x01) {
            // Track projection: distance → foreshortened X/Y for the road strip.
            const int distance = inputCount_ > 0 ? input_[0] : 0;
            const int offset = inputCount_ > 1 ? input_[1] : 0;
            const int horizon = inputCount_ > 2 ? input_[2] : 0x4000;
            const int denom = distance == 0 ? 1 : distance;
            output_[0] = clamp16(horizon / denom);
            output_[1] = clamp16(offset * horizon / denom);
            output_[2] = clamp16(horizon);
            outputCount_ = 3;
        } else if (command_ == 0x05 && inputCount_ >= 2) {
            output_[0] = clamp16((static_cast<int>(input_[0]) * static_cast<int>(input_[1])) >> 15);
            outputCount_ = 1;
        } else if (command_ == 0x08 && inputCount_ >= 3) {
            std::int16_t sine = 0;
            std::int16_t cosine = 0;
            anglePair(static_cast<std::uint16_t>(input_[0]), 32767, sine, cosine);
            output_[0] = clamp16((input_[1] * cosine - input_[2] * sine) / 32767);
            output_[1] = clamp16((input_[1] * sine + input_[2] * cosine) / 32767);
            outputCount_ = 2;
        } else {
            output_[0] = inputCount_ > 0 ? input_[0] : 0;
            outputCount_ = 1;
        }
    } else if (command_ == 0x00 && inputCount_ >= 2) {
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
    } else if (command_ == 0x08 && inputCount_ >= 3) {
        const double x = input_[0];
        const double y = input_[1];
        const double z = input_[2];
        output_[0] = clamp16(static_cast<int>(std::lround(std::sqrt(x * x + y * y + z * z))));
        outputCount_ = 1;
    } else if (command_ == 0x0A && inputCount_ >= 1) {
        output_[0] = input_[0] < 0 ? static_cast<std::int16_t>(-input_[0]) : input_[0];
        outputCount_ = 1;
    } else if (command_ == 0x0E && inputCount_ >= 2) {
        const double x = input_[0];
        const double y = input_[1];
        output_[0] = clamp16(static_cast<int>(std::lround(std::sqrt(x * x + y * y))));
        outputCount_ = 1;
    } else if (command_ == 0x18 && inputCount_ >= 3) {
        const double x = input_[0];
        const double y = input_[1];
        const double z = input_[2];
        output_[0] = clamp16(static_cast<int>(std::lround(std::sqrt(x * x + y * y + z * z))));
        output_[1] = clamp16(static_cast<int>(std::lround(std::atan2(y, x) * 65536.0 / 6.283185307179586)));
        output_[2] = clamp16(static_cast<int>(std::lround(std::atan2(z, std::sqrt(x * x + y * y)) * 65536.0 / 6.283185307179586)));
        outputCount_ = 3;
    } else if (command_ == 0x1C && inputCount_ >= 3) {
        std::int16_t sine = 0;
        std::int16_t cosine = 0;
        anglePair(static_cast<std::uint16_t>(input_[0]), input_[2], sine, cosine);
        output_[0] = cosine;
        output_[1] = sine;
        output_[2] = input_[1];
        outputCount_ = 3;
    } else if ((command_ == 0x01 || command_ == 0x11 || command_ == 0x20 || command_ == 0x28 || command_ == 0x2C) &&
               inputCount_ >= 1) {
        // Attitude helpers: return an identity-sized reply so Pilotwings-style
        // boards leave the transfer loop.
        output_[0] = 0;
        output_[1] = 0;
        output_[2] = 0x4000;
        outputCount_ = command_ == 0x01 ? 8 : 3;
        for (int index = 3; index < outputCount_; ++index) {
            output_[index] = 0;
        }
    } else if (command_ == 0x06 && inputCount_ >= 3) {
        output_[0] = input_[0];
        output_[1] = input_[1];
        output_[2] = input_[2];
        output_[3] = 0;
        outputCount_ = 4;
    } else if (command_ == 0x14 && inputCount_ >= 2) {
        output_[0] = clamp16(static_cast<int>(std::lround(std::atan2(input_[1], input_[0]) * 65536.0 / 6.283185307179586)));
        outputCount_ = 1;
    } else if (command_ == 0x02 && inputCount_ >= 3) {
        const double x = input_[0];
        const double y = input_[1];
        const double z = input_[2] == 0 ? 1.0 : input_[2];
        output_[0] = clamp16(static_cast<int>(std::lround(x * 32767.0 / z)));
        output_[1] = clamp16(static_cast<int>(std::lround(y * 32767.0 / z)));
        output_[2] = clamp16(static_cast<int>(std::lround(32767.0 * 32767.0 / z)));
        outputCount_ = 3;
    } else if (command_ == 0x0D && inputCount_ >= 3) {
        output_[0] = clamp16(input_[0] + input_[1]);
        output_[1] = clamp16(input_[1] + input_[2]);
        output_[2] = clamp16(input_[2] + input_[0]);
        outputCount_ = 3;
    } else if (command_ == 0x03 && inputCount_ >= 3) {
        // Scale vector by a 1.15 factor (host math stand-in for the coefficient ROM).
        const int factor = input_[2];
        output_[0] = clamp16((static_cast<int>(input_[0]) * factor) >> 15);
        output_[1] = clamp16((static_cast<int>(input_[1]) * factor) >> 15);
        outputCount_ = 2;
    } else if (command_ == 0x05 && inputCount_ >= 2) {
        output_[0] = clamp16(static_cast<int>(input_[0]) + static_cast<int>(input_[1]));
        output_[1] = clamp16(static_cast<int>(input_[0]) - static_cast<int>(input_[1]));
        outputCount_ = 2;
    } else if (command_ == 0x07 && inputCount_ >= 1) {
        std::int16_t sine = 0;
        std::int16_t cosine = 0;
        anglePair(static_cast<std::uint16_t>(input_[0]), 32767, sine, cosine);
        output_[0] = cosine;
        output_[1] = sine;
        outputCount_ = 2;
    } else if (command_ == 0x13 || command_ == 0x1D || command_ == 0x21 || command_ == 0x22 || command_ == 0x25 ||
               command_ == 0x2A || command_ == 0x2D || command_ == 0x2E || command_ == 0x30) {
        output_[0] = inputCount_ > 0 ? input_[0] : 0;
        output_[1] = inputCount_ > 1 ? input_[1] : 0;
        output_[2] = 0x4000;
        outputCount_ = 3;
    }
    phase_ = outputCount_ == 0 ? Phase::Command : Phase::Output;
}

void Dsp1::saveState(iris::ByteWriter& out) const {
    for (const std::int16_t value : input_) {
        out.u16(static_cast<std::uint16_t>(value));
    }
    for (const std::int16_t value : output_) {
        out.u16(static_cast<std::uint16_t>(value));
    }
    out.u16(word_);
    out.u32(static_cast<std::uint32_t>(inputCount_));
    out.u32(static_cast<std::uint32_t>(inputNeed_));
    out.u32(static_cast<std::uint32_t>(outputCount_));
    out.u32(static_cast<std::uint32_t>(outputIndex_));
    out.u32(static_cast<std::uint32_t>(byteIndex_));
    out.u8(command_);
    out.u8(static_cast<std::uint8_t>(phase_));
}

bool Dsp1::loadState(iris::ByteReader& in) {
    std::int16_t input[8] = {};
    std::int16_t output[8] = {};
    for (std::int16_t& value : input) {
        value = static_cast<std::int16_t>(in.u16());
    }
    for (std::int16_t& value : output) {
        value = static_cast<std::int16_t>(in.u16());
    }
    const auto word = in.u16();
    const auto inputCount = static_cast<int>(in.u32());
    const auto inputNeed = static_cast<int>(in.u32());
    const auto outputCount = static_cast<int>(in.u32());
    const auto outputIndex = static_cast<int>(in.u32());
    const auto byteIndex = static_cast<int>(in.u32());
    const auto command = in.u8();
    const auto phase = in.u8();
    if (!in.ok() || phase > static_cast<std::uint8_t>(Phase::Output)) {
        return false;
    }
    for (int index = 0; index < 8; ++index) {
        input_[index] = input[index];
        output_[index] = output[index];
    }
    word_ = word;
    inputCount_ = inputCount;
    inputNeed_ = inputNeed;
    outputCount_ = outputCount;
    outputIndex_ = outputIndex;
    byteIndex_ = byteIndex;
    command_ = command;
    phase_ = static_cast<Phase>(phase);
    return true;
}

int Dsp1::inputsFor(std::uint8_t command) const {
    if (chip_ == Chip::Dsp2) {
        if (command == 0x05) {
            return 2;
        }
        return 1;
    }
    if (chip_ == Chip::Dsp3) {
        if (command == 0x2E || command == 0x2F) {
            return 1;
        }
        if (command == 0x06) {
            return 2;
        }
        if (command == 0x10) {
            return 3;
        }
        return 1;
    }
    if (chip_ == Chip::Dsp4) {
        if (command == 0x00 || command == 0x01) {
            return 3;
        }
        if (command == 0x05) {
            return 2;
        }
        if (command == 0x08) {
            return 3;
        }
        return 1;
    }
    switch (command) {
    case 0x00:
        return 2;
    case 0x01:
        return 4;
    case 0x02:
        return 3;
    case 0x03:
        return 3;
    case 0x04:
        return 2;
    case 0x05:
        return 2;
    case 0x06:
        return 3;
    case 0x07:
        return 1;
    case 0x08:
        return 3;
    case 0x0A:
        return 1;
    case 0x0C:
        return 3;
    case 0x0D:
        return 3;
    case 0x0E:
        return 2;
    case 0x10:
        return 1;
    case 0x11:
        return 3;
    case 0x13:
    case 0x1D:
        return 3;
    case 0x14:
        return 2;
    case 0x18:
        return 3;
    case 0x1C:
        return 3;
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x25:
    case 0x28:
    case 0x2A:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x30:
        return 3;
    default:
        return 0;
    }
}

}  // namespace iris::systems::snes
