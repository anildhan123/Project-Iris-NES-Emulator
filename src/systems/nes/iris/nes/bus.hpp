#pragma once

#include "iris/nes/apu.hpp"
#include "iris/nes/controller.hpp"
#include "iris/nes/cpu.hpp"
#include "iris/nes/ppu.hpp"

#include <cstdint>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::nes {

class Bus final : public CpuMemory {
public:
    Bus(Ppu& ppu, Apu& apu, Controller& port1, Controller& port2);

    void connect(Mapper* mapper);
    void power();
    void reset();
    [[nodiscard]] std::uint8_t read(std::uint16_t address) override;
    void write(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] int takeDmaCycles(std::uint64_t cpuCycle);
    void captureState(iris::ByteWriter& writer) const;
    [[nodiscard]] bool applyState(iris::ByteReader& reader);

private:
    Ppu& ppu_;
    Apu& apu_;
    Controller& port1_;
    Controller& port2_;
    Mapper* mapper_ = nullptr;
    std::uint8_t ram_[2048] = {};
    std::uint8_t openBus_ = 0;
    int dmaCycles_ = 0;
};

}  // namespace iris::systems::nes
