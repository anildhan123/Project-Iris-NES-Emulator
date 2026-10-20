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
    sdd1DmaEnable_ = 0;
    sdd1BankEnable_ = 0x0F;  // all $70–$73 banks enabled until a game writes $4801
    spc7110Address_ = 0;
    spc7110Status_ = 0x80;  // data-port ready
    std::fill(std::begin(spc7110Rtc_), std::end(spc7110Rtc_), static_cast<std::uint8_t>(0));
    bitstream_ = 0;
    bitCount_ = 0;
    waitingOutput_ = false;
}

void Expansion::setChip(Chip chip) { chip_ = chip; }

void Expansion::bind(const std::vector<std::uint8_t>* rom) { rom_ = rom; }

std::uint8_t Expansion::romAt(std::uint32_t address) const {
    if (rom_ == nullptr || rom_->empty()) {
        return 0;
    }
    return (*rom_)[address % rom_->size()];
}

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
        // Decompress window: host stand-in returns sequential ROM bytes (no Nintendo table).
        if (mapped >= 0x70 && mapped <= 0x73 && address < 0x8000) {
            const int bankBit = mapped - 0x70;
            if ((sdd1BankEnable_ & static_cast<std::uint8_t>(1u << bankBit)) == 0) {
                value = 0;
                return true;
            }
            value = romAt(sdd1Address_++);
            return true;
        }
        if ((mapped <= 0x3F || mapped >= 0x80) && address >= 0x4800 && address <= 0x4807) {
            value = regs_[address - 0x4800];
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Spc7110) {
        if (address == 0x4800) {
            // Data port: sequential ROM stream configured by $4805–$4807.
            value = romAt(spc7110Address_++);
            regs_[0] = value;
            spc7110Status_ = 0x80;
            return true;
        }
        if (address == 0x480C) {
            value = spc7110Status_;
            return true;
        }
        if (address >= 0x4840 && address <= 0x4846) {
            value = spc7110Rtc_[address - 0x4840];
            return true;
        }
        if (address >= 0x4801 && address <= 0x483F) {
            value = regs_[address - 0x4800];
            return true;
        }
        if (mapped >= 0x50 && mapped <= 0x5F) {
            value = ram_[address & 0x1FFF];
            return true;
        }
        // SPC7110 data ROM window $D0–$FF (HiROM-style linear map).
        if (bank >= 0xD0) {
            const std::uint32_t offset = (static_cast<std::uint32_t>(bank - 0xD0) << 16) | address;
            value = romAt(offset);
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Obc1) {
        if (mapped <= 0x3F && address >= 0x7FF0 && address <= 0x7FF7) {
            value = regs_[address - 0x7FF0];
            return true;
        }
        if (mapped <= 0x3F && address >= 0x6000 && address < 0x8000) {
            // OBC-1 remaps $6000 through the pointer in $7FF0/$7FF1.
            const std::uint16_t base = static_cast<std::uint16_t>((regs_[0] | (regs_[1] << 8)) & 0x1F80);
            const std::uint16_t offset = static_cast<std::uint16_t>((address - 0x6000) & 0x7F);
            value = ram_[(base + offset) & 0x1FFF];
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
            // $218C bit 0 reports flash/satellite ready; others echo latched regs.
            if (address == 0x218C) {
                value = static_cast<std::uint8_t>(regs_[address - 0x2188] | 0x01);
            } else if (address == 0x218E) {
                value = 0x20;  // FLASH manufacturer ID stand-in (not a Nintendo dump).
            } else {
                value = regs_[address - 0x2188];
            }
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
                runCx4(value);
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
            if (address == 0x4800) {
                sdd1DmaEnable_ = value;
            }
            if (address == 0x4801) {
                sdd1BankEnable_ = value == 0 ? 0x0F : value;
            }
            if (address == 0x4801 || address == 0x4805) {
                sdd1Address_ = (static_cast<std::uint32_t>(regs_[2]) << 16) | (static_cast<std::uint32_t>(regs_[3]) << 8) |
                               regs_[4];
            }
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Spc7110) {
        if (address >= 0x4840 && address <= 0x4846) {
            spc7110Rtc_[address - 0x4840] = value;
            return true;
        }
        if (address >= 0x4800 && address <= 0x483F) {
            regs_[address - 0x4800] = value;
            if (address == 0x4804 || address == 0x4807) {
                spc7110Address_ = (static_cast<std::uint32_t>(regs_[5]) << 16) | (static_cast<std::uint32_t>(regs_[6]) << 8) |
                                  regs_[7];
                spc7110Status_ = 0x80;
            }
            return true;
        }
        if (mapped >= 0x50 && mapped <= 0x5F) {
            ram_[address & 0x1FFF] = value;
            return true;
        }
        return false;
    }
    if (chip_ == Chip::Obc1) {
        if (mapped <= 0x3F && address >= 0x7FF0 && address <= 0x7FF7) {
            regs_[address - 0x7FF0] = value;
            return true;
        }
        if (mapped <= 0x3F && address >= 0x6000 && address < 0x8000) {
            const std::uint16_t base = static_cast<std::uint16_t>((regs_[0] | (regs_[1] << 8)) & 0x1F80);
            const std::uint16_t offset = static_cast<std::uint16_t>((address - 0x6000) & 0x7F);
            ram_[(base + offset) & 0x1FFF] = value;
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

void Expansion::runCx4(std::uint8_t command) {
    const auto a = static_cast<std::int16_t>(regs_[0] | (regs_[1] << 8));
    const auto b = static_cast<std::int16_t>(regs_[2] | (regs_[3] << 8));
    const auto c = static_cast<std::int16_t>(regs_[4] | (regs_[5] << 8));
    std::int32_t result0 = a;
    std::int32_t result1 = b;
    if (command == 0x05) {
        const auto product = static_cast<std::int32_t>(a) * static_cast<std::int32_t>(b);
        result0 = product & 0xFFFF;
        result1 = (product >> 16) & 0xFFFF;
    } else if (command == 0x00) {
        result0 = a;
        result1 = b;
    } else if (command == 0x01) {
        result0 = -a;
        result1 = -b;
    } else if (command == 0x02) {
        const double length = std::sqrt(static_cast<double>(a) * a + static_cast<double>(b) * b);
        result0 = clamp16(static_cast<int>(std::lround(length)));
        result1 = 0;
    } else if (command == 0x03) {
        result0 = clamp16(static_cast<int>(std::lround(std::atan2(b, a) * 65536.0 / 6.283185307179586)));
        result1 = 0;
    } else if (command == 0x07) {
        const double angle = static_cast<double>(static_cast<std::uint16_t>(c)) * 6.283185307179586 / 65536.0;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        result0 = clamp16(static_cast<int>(std::lround(a * cosine - b * sine)));
        result1 = clamp16(static_cast<int>(std::lround(a * sine + b * cosine)));
    } else if (command == 0x0D) {
        // DMA-style copy inside Cx4 RAM: source=regs[0..1], dest=regs[2..3], count=regs[4].
        const int source = (regs_[0] | (regs_[1] << 8)) & 0x1FFF;
        const int dest = (regs_[2] | (regs_[3] << 8)) & 0x1FFF;
        const int count = regs_[4] == 0 ? 256 : regs_[4];
        for (int index = 0; index < count; ++index) {
            ram_[(dest + index) & 0x1FFF] = ram_[(source + index) & 0x1FFF];
        }
        result0 = count;
        result1 = 0;
    } else if (command == 0x10) {
        const int scale = c == 0 ? 1 : c;
        result0 = clamp16(a * 256 / scale);
        result1 = clamp16(b * 256 / scale);
    } else {
        result0 = a;
        result1 = b;
    }
    regs_[0x10] = static_cast<std::uint8_t>(result0);
    regs_[0x11] = static_cast<std::uint8_t>(result0 >> 8);
    regs_[0x12] = static_cast<std::uint8_t>(result1);
    regs_[0x13] = static_cast<std::uint8_t>(result1 >> 8);
    if (command == 0x05) {
        const auto product = static_cast<std::int32_t>(a) * static_cast<std::int32_t>(b);
        regs_[0x10] = static_cast<std::uint8_t>(product);
        regs_[0x11] = static_cast<std::uint8_t>(product >> 8);
        regs_[0x12] = static_cast<std::uint8_t>(product >> 16);
        regs_[0x13] = static_cast<std::uint8_t>(product >> 24);
    }
    output_[0] = static_cast<std::int16_t>(result0);
    output_[1] = static_cast<std::int16_t>(result1);
    outputCount_ = 2;
    outputIndex_ = 0;
    waitingOutput_ = true;
    regs_[0x0E] = 0;
}

void Expansion::acceptWord(std::uint16_t word) {
    if (inputNeed_ == 0) {
        command_ = static_cast<std::uint8_t>(word);
        inputNeed_ = chip_ == Chip::St018 ? 3 : (command_ == 0x08 ? 3 : 2);
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
        } else if (command_ == 0x02 && inputCount_ >= 2) {
            output_[0] = clamp16(static_cast<int>(input_[0]) + static_cast<int>(input_[1]));
            outputCount_ = 1;
        } else if (command_ == 0x04 && inputCount_ >= 1) {
            const double angle = static_cast<double>(static_cast<std::uint16_t>(input_[0])) * 6.283185307179586 / 65536.0;
            output_[0] = clamp16(static_cast<int>(std::lround(std::sin(angle) * 32767.0)));
            output_[1] = clamp16(static_cast<int>(std::lround(std::cos(angle) * 32767.0)));
            outputCount_ = 2;
        } else if (command_ == 0x08 && inputCount_ >= 3) {
            const double x = input_[0];
            const double y = input_[1];
            const double z = input_[2];
            output_[0] = clamp16(static_cast<int>(std::lround(std::sqrt(x * x + y * y + z * z))));
            outputCount_ = 1;
        } else {
            output_[0] = inputCount_ > 0 ? input_[0] : 0;
            output_[1] = 0;
            outputCount_ = 2;
        }
    } else if (chip_ == Chip::St018) {
        if (command_ == 0x01 && inputCount_ >= 2) {
            output_[0] = clamp16(static_cast<int>(input_[0]) + static_cast<int>(input_[1]));
            output_[1] = clamp16(static_cast<int>(input_[0]) - static_cast<int>(input_[1]));
            output_[2] = 0;
            outputCount_ = 3;
        } else {
            output_[0] = inputCount_ > 0 ? input_[0] : 0;
            output_[1] = inputCount_ > 1 ? input_[1] : 0;
            output_[2] = inputCount_ > 2 ? input_[2] : 0;
            outputCount_ = 3;
        }
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
    out.u8(sdd1DmaEnable_);
    out.u8(sdd1BankEnable_);
    out.u32(spc7110Address_);
    out.u8(spc7110Status_);
    out.bytes(spc7110Rtc_, sizeof(spc7110Rtc_));
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
    const auto sdd1DmaEnable = in.u8();
    const auto sdd1BankEnable = in.u8();
    const auto spc7110Address = in.u32();
    const auto spc7110Status = in.u8();
    std::uint8_t spc7110Rtc[7] = {};
    in.bytes(spc7110Rtc, sizeof(spc7110Rtc));
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
    sdd1DmaEnable_ = sdd1DmaEnable;
    sdd1BankEnable_ = sdd1BankEnable == 0 ? 0x0F : sdd1BankEnable;
    spc7110Address_ = spc7110Address;
    spc7110Status_ = spc7110Status;
    std::copy(std::begin(spc7110Rtc), std::end(spc7110Rtc), std::begin(spc7110Rtc_));
    bitstream_ = bitstream;
    bitCount_ = bitCount;
    waitingOutput_ = waitingOutput;
    return true;
}

}  // namespace iris::systems::snes
