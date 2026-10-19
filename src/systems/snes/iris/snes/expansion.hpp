#pragma once

#include "iris/snes/cartridge.hpp"

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

// Remaining enhancement boards: Cx4, S-DD1, SPC7110, OBC-1, ST010/011/018, and BS-X.
// Each board answers its documented MMIO slice so a cartridge can continue. Full
// silicon behavior is not claimed for every command.
class Expansion {
public:
    void power();
    void setChip(Chip chip);
    void bind(const std::vector<std::uint8_t>* rom);
    [[nodiscard]] bool read(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t& value);
    [[nodiscard]] bool write(std::uint8_t bank, std::uint16_t address, bool hiRom, std::uint8_t value);
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

private:
    void acceptWord(std::uint16_t word);
    void execute();

    Chip chip_ = Chip::None;
    const std::vector<std::uint8_t>* rom_ = nullptr;
    std::uint8_t regs_[32] = {};
    std::uint8_t ram_[8192] = {};
    std::int16_t input_[8] = {};
    std::int16_t output_[16] = {};
    std::uint16_t word_ = 0;
    int inputCount_ = 0;
    int inputNeed_ = 0;
    int outputCount_ = 0;
    int outputIndex_ = 0;
    int byteIndex_ = 0;
    std::uint8_t command_ = 0;
    std::uint32_t sdd1Address_ = 0;
    std::uint16_t bitstream_ = 0;
    int bitCount_ = 0;
    bool waitingOutput_ = false;
};

}  // namespace iris::systems::snes
