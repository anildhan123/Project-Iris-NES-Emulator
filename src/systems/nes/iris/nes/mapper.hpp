#pragma once

#include <cstdint>

namespace iris::systems::nes {

enum class Mirroring {
    Horizontal,
    Vertical,
    FourScreen,
    SingleScreenLower,
    SingleScreenUpper,
};

class Mapper {
public:
    virtual ~Mapper() = default;

    [[nodiscard]] virtual std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) = 0;
    virtual void writeCpu(std::uint16_t address, std::uint8_t value) = 0;
    [[nodiscard]] virtual std::uint8_t readChr(std::uint16_t address) = 0;
    virtual void writeChr(std::uint16_t address, std::uint8_t value) = 0;
    [[nodiscard]] virtual Mirroring mirroring() const = 0;

    // One CPU cycle has elapsed since the previous call. MMC1 uses this to ignore a second write.
    virtual void cpuCycle() {}
    // Rising edge of PPU A12 after A12 has been low. MMC3 clocks its scanline counter here.
    virtual void onA12Rise() {}
    [[nodiscard]] virtual bool irq() const { return false; }
    virtual void reset() {}
};

}  // namespace iris::systems::nes
