#pragma once

#include "iris/nes/cartridge.hpp"

namespace iris::systems::nes {

class Cnrom final : public Mapper {
public:
    explicit Cnrom(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }

private:
    Cartridge& cartridge_;
    int bank_ = 0;
};

}  // namespace iris::systems::nes
