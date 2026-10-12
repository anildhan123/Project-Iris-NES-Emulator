#pragma once

#include "iris/nes/cartridge.hpp"

#include <array>

namespace iris::systems::nes {

class Mmc3 final : public Mapper {
public:
    explicit Mmc3(Cartridge& cartridge) : cartridge_(cartridge) { reset(); }

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void onA12Rise() override;
    [[nodiscard]] bool irq() const override { return irq_; }
    void reset() override;

private:
    [[nodiscard]] int prgBank(std::uint16_t address) const;
    [[nodiscard]] std::size_t chrOffset(std::uint16_t address) const;

    Cartridge& cartridge_;
    std::array<std::uint8_t, 8> banks_{};
    std::uint8_t select_ = 0;
    std::uint8_t latch_ = 0;
    std::uint8_t counter_ = 0;
    bool reload_ = false;
    bool irqEnabled_ = false;
    bool irq_ = false;
    bool horizontal_ = false;
};

}  // namespace iris::systems::nes
