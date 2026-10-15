#pragma once

#include <cstdint>

namespace iris::systems::snes {

// DSP-1 data and status ports. Multiply, inverse, triangle, and rotate are
// implemented. Other commands are consumed and return no words. Inverse and
// the angle commands use host arithmetic, not a coefficient ROM.
class Dsp1 {
public:
    void power();
    [[nodiscard]] bool read(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t& value);
    [[nodiscard]] bool write(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t value);

private:
    enum class Phase { Command, Input, Output };

    void acceptWord(std::uint16_t word);
    void execute();
    [[nodiscard]] static int inputsFor(std::uint8_t command);

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
