#pragma once

#include <array>
#include <cstdint>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::nes {

class CpuMemory {
public:
    virtual ~CpuMemory() = default;
    [[nodiscard]] virtual std::uint8_t read(std::uint16_t address) = 0;
    virtual void write(std::uint16_t address, std::uint8_t value) = 0;
};

struct CpuState {
    std::uint8_t a = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t sp = 0;
    std::uint8_t p = 0;
    std::uint16_t pc = 0;
};

// Official 6502 opcodes. Decimal mode can be set, but ADC and SBC stay binary, matching the NES.
class Cpu {
public:
    void reset(CpuMemory& memory);
    void signalNmi();
    void setIrqLine(bool active);
    int step(CpuMemory& memory);

    [[nodiscard]] CpuState state() const;
    [[nodiscard]] int unknownOpcodes() const { return unknownOpcodes_; }
    void captureState(iris::ByteWriter& writer) const;
    [[nodiscard]] bool applyState(iris::ByteReader& reader);

private:
    enum class OpKind : std::uint8_t {
        Adc, And, Asl, Bcc, Bcs, Beq, Bit, Bmi, Bne, Bpl, Brk, Bvc, Bvs,
        Clc, Cld, Cli, Clv, Cmp, Cpx, Cpy, Dec, Dex, Dey, Eor, Inc, Inx, Iny,
        Jmp, Jsr, Lda, Ldx, Ldy, Lsr, Nop, Ora, Pha, Php, Pla, Plp, Rol, Ror,
        Rti, Rts, Sbc, Sec, Sed, Sei, Sta, Stx, Sty, Tax, Tay, Tsx, Txa, Txs, Tya,
        Unknown
    };

    enum class AddrMode : std::uint8_t {
        Implied, Accumulator, Immediate, ZeroPage, ZeroPageX, ZeroPageY,
        Absolute, AbsoluteX, AbsoluteY, Indirect, IndirectX, IndirectY, Relative
    };

    struct Entry {
        OpKind kind = OpKind::Unknown;
        AddrMode mode = AddrMode::Implied;
        std::uint8_t cycles = 2;
        bool penalty = false;
    };

    struct Operand {
        std::uint16_t address = 0;
        std::uint8_t immediate = 0;
        bool immediateValue = false;
        int penalty = 0;

        [[nodiscard]] std::uint8_t read(CpuMemory& memory) const {
            return immediateValue ? immediate : memory.read(address);
        }
    };

    [[nodiscard]] static const Entry& decode(std::uint8_t opcode);
    [[nodiscard]] static std::array<Entry, 256> makeTable();
    [[nodiscard]] Operand address(CpuMemory& memory, AddrMode mode, bool penalty);
    int execute(CpuMemory& memory, OpKind kind, const Operand& operand, int cycles);
    int branch(bool condition, const Operand& operand);
    int service(CpuMemory& memory, std::uint16_t vector, bool brk);
    void adc(std::uint8_t value);
    void compare(std::uint8_t left, std::uint8_t right);
    void setZN(std::uint8_t value);
    void push(CpuMemory& memory, std::uint8_t value);
    [[nodiscard]] std::uint8_t pop(CpuMemory& memory);
    [[nodiscard]] std::uint16_t read16(CpuMemory& memory, std::uint16_t address);
    [[nodiscard]] std::uint16_t readZeroPage16(CpuMemory& memory, std::uint8_t address);

    std::uint8_t a_ = 0;
    std::uint8_t x_ = 0;
    std::uint8_t y_ = 0;
    std::uint8_t sp_ = 0xFD;
    std::uint8_t p_ = 0x24;
    std::uint16_t pc_ = 0;
    bool nmiPending_ = false;
    bool irqLine_ = false;
    bool irqDelay_ = false;
    int unknownOpcodes_ = 0;
    bool loggedUnknown_ = false;
};

}  // namespace iris::systems::nes
