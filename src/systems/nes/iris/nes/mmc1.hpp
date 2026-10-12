#pragma once

#include "iris/nes/cartridge.hpp"

namespace iris::systems::nes {

class Mmc1 final : public Mapper {
public:
    explicit Mmc1(Cartridge& cartridge) : cartridge_(cartridge) { reset(); }

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override { ignoreWrite_ = false; }
    void reset() override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    void loadRegister(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] int prgBankCount() const;
    [[nodiscard]] std::size_t chrOffset(std::uint16_t address) const;

    Cartridge& cartridge_;
    std::uint8_t shift_ = 0;
    int shiftCount_ = 0;
    bool ignoreWrite_ = false;
    std::uint8_t control_ = 0x0C;
    std::uint8_t chrBank0_ = 0;
    std::uint8_t chrBank1_ = 0;
    std::uint8_t prgBank_ = 0;
    bool prgRamDisabled_ = false;
};

}  // namespace iris::systems::nes
