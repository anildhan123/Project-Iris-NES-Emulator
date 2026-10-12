#pragma once

#include <cstdint>

namespace iris::systems::nes {

enum class Mirroring {
    Horizontal,
    Vertical,
    FourScreen,
    SingleScreen,
};

class Mapper {
public:
    virtual ~Mapper() = default;

    [[nodiscard]] virtual std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) = 0;
    virtual void writeCpu(std::uint16_t address, std::uint8_t value) = 0;
    [[nodiscard]] virtual std::uint8_t readChr(std::uint16_t address) = 0;
    virtual void writeChr(std::uint16_t address, std::uint8_t value) = 0;
    [[nodiscard]] virtual Mirroring mirroring() const = 0;
};

}  // namespace iris::systems::nes
