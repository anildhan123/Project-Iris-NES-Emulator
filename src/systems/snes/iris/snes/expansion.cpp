#include "iris/snes/expansion.hpp"

#include "iris/byte_io.hpp"

#include <algorithm>
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

}  // namespace

void Expansion::power() {
    std::fill(std::begin(regs_), std::end(regs_), static_cast<std::uint8_t>(0));
    std::fill(std::begin(ram_), std::end(ram_), static_cast<std::uint8_t>(0));
    std::fill(std::begin(input_), std::end(input_), static_cast<std::int16_t>(0));
    std::fill(std::begin(output_), std::end(output_), static_cast<std::int16_t>(0));
    word_ = 0;
    inputCount_ = 0;
    inputNeed_ = 0;
    outputCount_ = 0;
    outputIndex_ = 0;
    byteIndex_ = 0;
    command_ = 0;
    sdd1Address_ = 0;
    bitstream_ = 0;
    bitCount_ = 0;
    waitingOutput_ = false;
}

void Expansion::setChip(Chip chip) { chip_ = chip; }

void Expansion::bind(const std::vector<std::uint8_t>* rom) { rom_ = rom; }

bool Expansion::read(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t& value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    if (chip_ == Chip::None) {
        return false;
    }
    if (chip_ == Chip::Cx4) {
        // Registers sit in $7F40–$7F5F inside the $6000–$7FFF Cx4 window. Do not
        // shadow LoROM at $8000 or the cartridge cannot fetch code.
        if (mapped <= 0x3F && address >= 0x7F40 && address < 0x7F60) {
            value = regs_[address - 0x7F40];
            return true;
        }
        if (mapped <= 0x3F && address >= 0x6000 && address < 0x8000) {
            value = ram_[address - 0x6000];
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Sdd1) {
        if (mapped >= 0x70 && mapped <= 0x73 && address < 0x8000) {
            value = 0;
            return true;
        }
        if ((mapped <= 0x3F || mapped >= 0x80) && address >= 0x4800 && address <= 0x4807) {
            value = regs_[address - 0x4800];
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Spc7110) {
        if (address >= 0x4800 && address <= 0x483F) {
            value = regs_[address - 0x4800];
            return true;
        }
        if (mapped >= 0x50 && mapped <= 0x5F) {
            value = ram_[address & 0x1FFF];
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Obc1) {
        if (mapped <= 0x3F && address >= 0x6000 && address < 0x8000) {
            value = ram_[(address - 0x6000) & 0x1FFF];
            return true;
        }
        if (mapped <= 0x3F && address >= 0x7FF0 && address <= 0x7FF7) {
            value = regs_[address - 0x7FF0];
            return true;
        }
        return false;
    }
    if (chip_ == Chip::St010 || chip_ == Chip::St011 || chip_ == Chip::St018) {
        const bool data = hiRom ? mapped <= 0x0F && address >= 0x6000 && address < 0x7000
                                : mapped >= 0x60 && mapped <= 0x6F && address < 0x1000;
        const bool status = hiRom ? mapped <= 0x0F && address >= 0x7000 && address < 0x8000
                                  : mapped >= 0x60 && mapped <= 0x6F && address >= 0x1000;
        if (status) {
            value = waitingOutput_ ? 0x80 : 0x00;
            return true;
        }
        if (!data) {
            return false;
        }
        if (!waitingOutput_ || outputIndex_ >= outputCount_) {
            value = 0;
            return true;
        }
        const auto word = static_cast<std::uint16_t>(output_[outputIndex_]);
        value = byteIndex_ == 0 ? static_cast<std::uint8_t>(word) : static_cast<std::uint8_t>(word >> 8);
        if (++byteIndex_ >= 2) {
            byteIndex_ = 0;
            ++outputIndex_;
            if (outputIndex_ >= outputCount_) {
                waitingOutput_ = false;
            }
        }
        return true;
    }
    if (chip_ == Chip::Bsx) {
        if (mapped == 0x00 && address >= 0x2188 && address <= 0x219F) {
            value = regs_[address - 0x2188];
            return true;
        }
        if (mapped >= 0x80 && mapped <= 0xBF && address < 0x8000) {
            value = ram_[address & 0x1FFF];
            return true;
        }
        return false;
    }
    return false;
}

bool Expansion::write(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    if (chip_ == Chip::None) {
        return false;
    }
    if (chip_ == Chip::Cx4) {
        if (mapped <= 0x3F && address >= 0x7F40 && address < 0x7F60) {
            regs_[address - 0x7F40] = value;
            if (address == 0x7F4F) {
                // Kick: identity / copy so Mega Man X2/X3 style transfers can finish.
                output_[0] = static_cast<std::int16_t>(regs_[0] | (regs_[1] << 8));
                output_[1] = static_cast<std::int16_t>(regs_[2] | (regs_[3] << 8));
                outputCount_ = 2;
                outputIndex_ = 0;
                waitingOutput_ = true;
                regs_[0x0E] = 0;
            }
            return true;
        }
        if (mapped <= 0x3F && address >= 0x6000 && address < 0x8000) {
            ram_[address - 0x6000] = value;
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Sdd1) {
        if ((mapped <= 0x3F || mapped >= 0x80) && address >= 0x4800 && address <= 0x4807) {
            regs_[address - 0x4800] = value;
            if (address == 0x4801) {
                sdd1Address_ = (static_cast<std::uint32_t>(regs_[2]) << 16) | (static_cast<std::uint32_t>(regs_[3]) << 8) |
                               regs_[4];
            }
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Spc7110) {
        if (address >= 0x4800 && address <= 0x483F) {
            regs_[address - 0x4800] = value;
            return true;
        }
        if (mapped >= 0x50 && mapped <= 0x5F) {
            ram_[address & 0x1FFF] = value;
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Obc1) {
        if (mapped <= 0x3F && address >= 0x6000 && address < 0x8000) {
            ram_[(address - 0x6000) & 0x1FFF] = value;
            return true;
        }
        if (mapped <= 0x3F && address >= 0x7FF0 && address <= 0x7FF7) {
            regs_[address - 0x7FF0] = value;
            return true;
        }
        return false;
    }
    if (chip_ == Chip::St010 || chip_ == Chip::St011 || chip_ == Chip::St018) {
        const bool data = hiRom ? mapped <= 0x0F && address >= 0x6000 && address < 0x7000
                                : mapped >= 0x60 && mapped <= 0x6F && address < 0x1000;
        if (!data || waitingOutput_) {
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
    if (chip_ == Chip::Bsx) {
        if (mapped == 0x00 && address >= 0x2188 && address <= 0x219F) {
            regs_[address - 0x2188] = value;
            return true;
        }
        if (mapped >= 0x80 && mapped <= 0xBF && address < 0x8000) {
            ram_[address & 0x1FFF] = value;
            return true;
        }
        return false;
    }
    return false;
}

void Expansion::acceptWord(std::uint16_t word) {
    if (inputNeed_ == 0) {
        command_ = static_cast<std::uint8_t>(word);
        inputNeed_ = chip_ == Chip::St018 ? 3 : 2;
        inputCount_ = 0;
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

void Expansion::execute() {
    outputCount_ = 0;
    outputIndex_ = 0;
    byteIndex_ = 0;
    if (chip_ == Chip::St010 || chip_ == Chip::St011) {
        if (command_ == 0x01 && inputCount_ >= 2) {
            output_[0] = clamp16((static_cast<int>(input_[0]) * static_cast<int>(input_[1])) >> 15);
            outputCount_ = 1;
        } else if (command_ == 0x04 && inputCount_ >= 1) {
            const double angle = static_cast<double>(static_cast<std::uint16_t>(input_[0])) * 6.283185307179586 / 65536.0;
            output_[0] = clamp16(static_cast<int>(std::lround(std::sin(angle) * 32767.0)));
            output_[1] = clamp16(static_cast<int>(std::lround(std::cos(angle) * 32767.0)));
            outputCount_ = 2;
        } else {
            output_[0] = inputCount_ > 0 ? input_[0] : 0;
            output_[1] = 0;
            outputCount_ = 2;
        }
    } else if (chip_ == Chip::St018) {
        output_[0] = inputCount_ > 0 ? input_[0] : 0;
        output_[1] = inputCount_ > 1 ? input_[1] : 0;
        output_[2] = inputCount_ > 2 ? input_[2] : 0;
        outputCount_ = 3;
    }
    waitingOutput_ = outputCount_ > 0;
    inputNeed_ = 0;
    inputCount_ = 0;
}

void Expansion::saveState(iris::ByteWriter& out) const {
    out.u8(static_cast<std::uint8_t>(chip_));
    out.bytes(regs_, sizeof(regs_));
    out.bytes(ram_, sizeof(ram_));
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
    out.u32(sdd1Address_);
    out.u16(bitstream_);
    out.u32(static_cast<std::uint32_t>(bitCount_));
    out.boolean(waitingOutput_);
}

bool Expansion::loadState(iris::ByteReader& in) {
    const auto chip = in.u8();
    std::uint8_t regs[32] = {};
    std::uint8_t ram[8192] = {};
    in.bytes(regs, sizeof(regs));
    in.bytes(ram, sizeof(ram));
    std::int16_t input[8] = {};
    std::int16_t output[16] = {};
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
    const auto sdd1Address = in.u32();
    const auto bitstream = in.u16();
    const auto bitCount = static_cast<int>(in.u32());
    const auto waitingOutput = in.boolean();
    if (!in.ok() || chip > static_cast<std::uint8_t>(Chip::Bsx)) {
        return false;
    }
    chip_ = static_cast<Chip>(chip);
    std::copy(std::begin(regs), std::end(regs), std::begin(regs_));
    std::copy(std::begin(ram), std::end(ram), std::begin(ram_));
    for (int index = 0; index < 8; ++index) {
        input_[index] = input[index];
    }
    for (int index = 0; index < 16; ++index) {
        output_[index] = output[index];
    }
    word_ = word;
    inputCount_ = inputCount;
    inputNeed_ = inputNeed;
    outputCount_ = outputCount;
    outputIndex_ = outputIndex;
    byteIndex_ = byteIndex;
    command_ = command;
    sdd1Address_ = sdd1Address;
    bitstream_ = bitstream;
    bitCount_ = bitCount;
    waitingOutput_ = waitingOutput;
    return true;
}

}  // namespace iris::systems::snes
