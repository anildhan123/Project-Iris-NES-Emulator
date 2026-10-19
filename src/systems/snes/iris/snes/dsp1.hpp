#pragma once

#include "iris/snes/cartridge.hpp"

#include <cstdint>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

// DSP-1 through DSP-4 data and status ports. Geometry commands use host
// arithmetic, not a Nintendo coefficient ROM. DSP-2 through DSP-4 accept the
// board and answer a documented command slice so the cartridge can continue.
class Dsp1 {
public:
    void power();
    void setChip(Chip chip) { chip_ = chip; }
    [[nodiscard]] bool read(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t& value);
    [[nodiscard]] bool write(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t value);
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

private:
    enum class Phase { Command, Input, Output };

    void acceptWord(std::uint16_t word);
    void execute();
    [[nodiscard]] int inputsFor(std::uint8_t command) const;

    Chip chip_ = Chip::Dsp1;
    std::int16_t input_[8] = {};
    std::int16_t output_[8] = {};
    std::uint16_t word_ = 0;
    int inputCount_ = 0;
    int inputNeed_ = 0;
    int outputCount_ = 0;
    int outputIndex_ = 0;
    int byteIndex_ = 0;
    std::uint8_t command_ = 0;
    Phase phase_ = Phase::Command;
};

}  // namespace iris::systems::snes
