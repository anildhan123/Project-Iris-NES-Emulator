#pragma once

#include "iris/snes/bus.hpp"

#include <cstdint>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

// 65816, including the addressing modes used by LoROM and HiROM games.
// STP is the instruction that stops the CPU.
class Cpu {
public:
    void reset(Bus& bus);
    void resetTo(std::uint16_t pc);
    void signalNmi();
    void signalIrq();
    // Vertical IRQ at a line above vblank must run before the vblank NMI when a
    // long instruction made both pending at the next step.
    void signalIrqBeforeNmi();
    int step(Bus& bus);
    // WAI is asleep until NMI or an IRQ the flags allow. The frame loop can
    // advance to that event instead of stepping two cycles at a time.
    [[nodiscard]] bool parked() const;
    [[nodiscard]] bool consumeSpin();

    [[nodiscard]] bool stopped() const { return stopped_; }
    [[nodiscard]] std::uint8_t stopOpcode() const { return stopOpcode_; }
    [[nodiscard]] std::uint16_t pc() const { return pc_; }
    [[nodiscard]] std::uint8_t a() const { return static_cast<std::uint8_t>(a_); }
    [[nodiscard]] std::uint8_t x() const { return static_cast<std::uint8_t>(x_); }
    [[nodiscard]] std::uint8_t y() const { return static_cast<std::uint8_t>(y_); }
    [[nodiscard]] std::uint8_t sp() const { return static_cast<std::uint8_t>(sp_); }
    [[nodiscard]] std::uint8_t p() const { return p_; }
    [[nodiscard]] bool emulation() const { return emulation_; }
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

private:
    [[nodiscard]] std::uint8_t fetch(Bus& bus);
    [[nodiscard]] std::uint8_t read(Bus& bus, std::uint16_t address);
    void write(Bus& bus, std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint16_t read16(Bus& bus, std::uint16_t address);
    void setZN(std::uint8_t value);
    void setZN16(std::uint16_t value);
    void push(Bus& bus, std::uint8_t value);
    [[nodiscard]] std::uint8_t pull(Bus& bus);
    void adc(std::uint8_t value);
    void sbc(std::uint8_t value);
    void compare(std::uint8_t left, std::uint8_t right);
    void branch(Bus& bus, bool taken);
    void applyWidths();
    void exchangeCarry();
    void interrupt(Bus& bus, std::uint16_t vector);
    int execute(Bus& bus, std::uint8_t opcode);
    [[nodiscard]] std::uint16_t directBase() const { return emulation_ ? 0 : d_; }
    [[nodiscard]] std::uint16_t indexValue() const { return index8() ? static_cast<std::uint16_t>(x_ & 0xFF) : x_; }
    [[nodiscard]] std::uint16_t indexY() const { return index8() ? static_cast<std::uint16_t>(y_ & 0xFF) : y_; }
    [[nodiscard]] bool accumulator8() const { return emulation_ || (p_ & 0x20) != 0; }
    [[nodiscard]] bool index8() const { return emulation_ || (p_ & 0x10) != 0; }
    void stop(std::uint8_t opcode);

    std::uint16_t a_ = 0;
    std::uint16_t x_ = 0;
    std::uint16_t y_ = 0;
    std::uint16_t sp_ = 0x01FF;
    std::uint8_t p_ = 0x34;
    std::uint8_t pbr_ = 0;
    std::uint8_t dbr_ = 0;
    std::uint16_t d_ = 0;
    std::uint16_t pc_ = 0;
    bool emulation_ = true;
    bool nmiPending_ = false;
    bool irqPending_ = false;
    bool irqFirst_ = false;
    bool waiting_ = false;
    bool spinning_ = false;
    bool stopped_ = false;
    std::uint8_t stopOpcode_ = 0;
};

}  // namespace iris::systems::snes
