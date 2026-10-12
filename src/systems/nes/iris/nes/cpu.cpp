#include "iris/nes/cpu.hpp"

#include "iris/byte_io.hpp"
#include "iris/log.hpp"

#include <array>

namespace iris::systems::nes {
namespace {

constexpr std::uint8_t kCarry = 0x01;
constexpr std::uint8_t kZero = 0x02;
constexpr std::uint8_t kInterrupt = 0x04;
constexpr std::uint8_t kDecimal = 0x08;
constexpr std::uint8_t kUnused = 0x20;
constexpr std::uint8_t kOverflow = 0x40;
constexpr std::uint8_t kNegative = 0x80;

}  // namespace

std::array<Cpu::Entry, 256> Cpu::makeTable() {
    std::array<Entry, 256> table{};
    const auto add = [&](std::uint8_t opcode, OpKind kind, AddrMode mode, std::uint8_t cycles, bool penalty = false) {
        table[opcode] = Entry{kind, mode, cycles, penalty};
    };
    const auto alu = [&](OpKind kind, std::uint8_t imm, std::uint8_t zp, std::uint8_t zpx, std::uint8_t abs, std::uint8_t absx,
                         std::uint8_t absy, std::uint8_t indx, std::uint8_t indy) {
        add( imm, kind, AddrMode::Immediate, 2);
        add( zp, kind, AddrMode::ZeroPage, 3);
        add( zpx, kind, AddrMode::ZeroPageX, 4);
        add( abs, kind, AddrMode::Absolute, 4);
        add( absx, kind, AddrMode::AbsoluteX, 4, true);
        add( absy, kind, AddrMode::AbsoluteY, 4, true);
        add( indx, kind, AddrMode::IndirectX, 6);
        add( indy, kind, AddrMode::IndirectY, 5, true);
    };
    alu(OpKind::Adc, 0x69, 0x65, 0x75, 0x6D, 0x7D, 0x79, 0x61, 0x71);
    alu(OpKind::And, 0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31);
    alu(OpKind::Cmp, 0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1);
    alu(OpKind::Eor, 0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51);
    alu(OpKind::Lda, 0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1);
    alu(OpKind::Ora, 0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11);
    alu(OpKind::Sbc, 0xE9, 0xE5, 0xF5, 0xED, 0xFD, 0xF9, 0xE1, 0xF1);

    const auto rmw = [&](OpKind kind, std::uint8_t acc, std::uint8_t zp, std::uint8_t zpx, std::uint8_t abs, std::uint8_t absx) {
        add( acc, kind, AddrMode::Accumulator, 2);
        add( zp, kind, AddrMode::ZeroPage, 5);
        add( zpx, kind, AddrMode::ZeroPageX, 6);
        add( abs, kind, AddrMode::Absolute, 6);
        add( absx, kind, AddrMode::AbsoluteX, 7);
    };
    rmw(OpKind::Asl, 0x0A, 0x06, 0x16, 0x0E, 0x1E);
    rmw(OpKind::Lsr, 0x4A, 0x46, 0x56, 0x4E, 0x5E);
    rmw(OpKind::Rol, 0x2A, 0x26, 0x36, 0x2E, 0x3E);
    rmw(OpKind::Ror, 0x6A, 0x66, 0x76, 0x6E, 0x7E);

    const auto inc = [&](OpKind kind, std::uint8_t zp, std::uint8_t zpx, std::uint8_t abs, std::uint8_t absx) {
        add( zp, kind, AddrMode::ZeroPage, 5);
        add( zpx, kind, AddrMode::ZeroPageX, 6);
        add( abs, kind, AddrMode::Absolute, 6);
        add( absx, kind, AddrMode::AbsoluteX, 7);
    };
    inc(OpKind::Dec, 0xC6, 0xD6, 0xCE, 0xDE);
    inc(OpKind::Inc, 0xE6, 0xF6, 0xEE, 0xFE);

    const auto store = [&](OpKind kind, std::uint8_t zp, std::uint8_t zpx, std::uint8_t abs, std::uint8_t absx, std::uint8_t absy,
                           std::uint8_t indx, std::uint8_t indy) {
        add( zp, kind, AddrMode::ZeroPage, 3);
        add( zpx, kind, AddrMode::ZeroPageX, 4);
        add( abs, kind, AddrMode::Absolute, 4);
        add( absx, kind, AddrMode::AbsoluteX, 5);
        add( absy, kind, AddrMode::AbsoluteY, 5);
        add( indx, kind, AddrMode::IndirectX, 6);
        add( indy, kind, AddrMode::IndirectY, 6);
    };
    store(OpKind::Sta, 0x85, 0x95, 0x8D, 0x9D, 0x99, 0x81, 0x91);

    add( 0xA2, OpKind::Ldx, AddrMode::Immediate, 2);
    add( 0xA6, OpKind::Ldx, AddrMode::ZeroPage, 3);
    add( 0xB6, OpKind::Ldx, AddrMode::ZeroPageY, 4);
    add( 0xAE, OpKind::Ldx, AddrMode::Absolute, 4);
    add( 0xBE, OpKind::Ldx, AddrMode::AbsoluteY, 4, true);
    add( 0xA0, OpKind::Ldy, AddrMode::Immediate, 2);
    add( 0xA4, OpKind::Ldy, AddrMode::ZeroPage, 3);
    add( 0xB4, OpKind::Ldy, AddrMode::ZeroPageX, 4);
    add( 0xAC, OpKind::Ldy, AddrMode::Absolute, 4);
    add( 0xBC, OpKind::Ldy, AddrMode::AbsoluteX, 4, true);
    add( 0x86, OpKind::Stx, AddrMode::ZeroPage, 3);
    add( 0x96, OpKind::Stx, AddrMode::ZeroPageY, 4);
    add( 0x8E, OpKind::Stx, AddrMode::Absolute, 4);
    add( 0x84, OpKind::Sty, AddrMode::ZeroPage, 3);
    add( 0x94, OpKind::Sty, AddrMode::ZeroPageX, 4);
    add( 0x8C, OpKind::Sty, AddrMode::Absolute, 4);

    add( 0xE0, OpKind::Cpx, AddrMode::Immediate, 2);
    add( 0xE4, OpKind::Cpx, AddrMode::ZeroPage, 3);
    add( 0xEC, OpKind::Cpx, AddrMode::Absolute, 4);
    add( 0xC0, OpKind::Cpy, AddrMode::Immediate, 2);
    add( 0xC4, OpKind::Cpy, AddrMode::ZeroPage, 3);
    add( 0xCC, OpKind::Cpy, AddrMode::Absolute, 4);
    add( 0x24, OpKind::Bit, AddrMode::ZeroPage, 3);
    add( 0x2C, OpKind::Bit, AddrMode::Absolute, 4);

    add( 0x10, OpKind::Bpl, AddrMode::Relative, 2);
    add( 0x30, OpKind::Bmi, AddrMode::Relative, 2);
    add( 0x50, OpKind::Bvc, AddrMode::Relative, 2);
    add( 0x70, OpKind::Bvs, AddrMode::Relative, 2);
    add( 0x90, OpKind::Bcc, AddrMode::Relative, 2);
    add( 0xB0, OpKind::Bcs, AddrMode::Relative, 2);
    add( 0xD0, OpKind::Bne, AddrMode::Relative, 2);
    add( 0xF0, OpKind::Beq, AddrMode::Relative, 2);

    add( 0x4C, OpKind::Jmp, AddrMode::Absolute, 3);
    add( 0x6C, OpKind::Jmp, AddrMode::Indirect, 5);
    add( 0x20, OpKind::Jsr, AddrMode::Absolute, 6);
    add( 0x00, OpKind::Brk, AddrMode::Implied, 7);
    add( 0x40, OpKind::Rti, AddrMode::Implied, 6);
    add( 0x60, OpKind::Rts, AddrMode::Implied, 6);
    add( 0xEA, OpKind::Nop, AddrMode::Implied, 2);
    add( 0x48, OpKind::Pha, AddrMode::Implied, 3);
    add( 0x08, OpKind::Php, AddrMode::Implied, 3);
    add( 0x68, OpKind::Pla, AddrMode::Implied, 4);
    add( 0x28, OpKind::Plp, AddrMode::Implied, 4);
    add( 0x18, OpKind::Clc, AddrMode::Implied, 2);
    add( 0xD8, OpKind::Cld, AddrMode::Implied, 2);
    add( 0x58, OpKind::Cli, AddrMode::Implied, 2);
    add( 0xB8, OpKind::Clv, AddrMode::Implied, 2);
    add( 0x38, OpKind::Sec, AddrMode::Implied, 2);
    add( 0xF8, OpKind::Sed, AddrMode::Implied, 2);
    add( 0x78, OpKind::Sei, AddrMode::Implied, 2);
    add( 0xCA, OpKind::Dex, AddrMode::Implied, 2);
    add( 0x88, OpKind::Dey, AddrMode::Implied, 2);
    add( 0xE8, OpKind::Inx, AddrMode::Implied, 2);
    add( 0xC8, OpKind::Iny, AddrMode::Implied, 2);
    add( 0xAA, OpKind::Tax, AddrMode::Implied, 2);
    add( 0xA8, OpKind::Tay, AddrMode::Implied, 2);
    add( 0xBA, OpKind::Tsx, AddrMode::Implied, 2);
    add( 0x8A, OpKind::Txa, AddrMode::Implied, 2);
    add( 0x9A, OpKind::Txs, AddrMode::Implied, 2);
    add( 0x98, OpKind::Tya, AddrMode::Implied, 2);
    return table;
}

void Cpu::reset(CpuMemory& memory) {
    a_ = 0;
    x_ = 0;
    y_ = 0;
    sp_ = 0xFD;
    p_ = kUnused | kInterrupt;
    nmiPending_ = false;
    irqDelay_ = false;
    const std::uint8_t low = memory.read(0xFFFC);
    const std::uint8_t high = memory.read(0xFFFD);
    pc_ = static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8));
}

void Cpu::signalNmi() { nmiPending_ = true; }

void Cpu::setIrqLine(bool active) { irqLine_ = active; }

void Cpu::captureState(ByteWriter& writer) const {
    writer.u8(a_);
    writer.u8(x_);
    writer.u8(y_);
    writer.u8(sp_);
    writer.u8(p_);
    writer.u16(pc_);
    writer.boolean(nmiPending_);
    writer.boolean(irqLine_);
    writer.boolean(irqDelay_);
    writer.u32(static_cast<std::uint32_t>(unknownOpcodes_));
    writer.boolean(loggedUnknown_);
}

bool Cpu::applyState(ByteReader& reader) {
    const std::uint8_t a = reader.u8();
    const std::uint8_t x = reader.u8();
    const std::uint8_t y = reader.u8();
    const std::uint8_t sp = reader.u8();
    const std::uint8_t p = reader.u8();
    const std::uint16_t pc = reader.u16();
    const bool nmiPending = reader.boolean();
    const bool irqLine = reader.boolean();
    const bool irqDelay = reader.boolean();
    const std::uint32_t unknownOpcodes = reader.u32();
    const bool loggedUnknown = reader.boolean();
    if (!reader.ok() || !reader.exhausted()) {
        return false;
    }
    a_ = a;
    x_ = x;
    y_ = y;
    sp_ = sp;
    p_ = static_cast<std::uint8_t>(p | 0x20);
    pc_ = pc;
    nmiPending_ = nmiPending;
    irqLine_ = irqLine;
    irqDelay_ = irqDelay;
    unknownOpcodes_ = static_cast<int>(unknownOpcodes);
    loggedUnknown_ = loggedUnknown;
    return true;
}

CpuState Cpu::state() const { return CpuState{a_, x_, y_, sp_, p_, pc_}; }

const Cpu::Entry& Cpu::decode(std::uint8_t opcode) {
    static const std::array<Entry, 256> table = makeTable();
    return table[opcode];
}

std::uint16_t Cpu::read16(CpuMemory& memory, std::uint16_t address) {
    const std::uint8_t low = memory.read(address);
    const std::uint8_t high = memory.read(static_cast<std::uint16_t>(address + 1));
    return static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8));
}

std::uint16_t Cpu::readZeroPage16(CpuMemory& memory, std::uint8_t address) {
    const std::uint8_t low = memory.read(address);
    const std::uint8_t high = memory.read(static_cast<std::uint8_t>(address + 1));
    return static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8));
}

void Cpu::setZN(std::uint8_t value) {
    p_ = static_cast<std::uint8_t>((p_ & ~(kZero | kNegative)) | (value & kNegative) | (value == 0 ? kZero : 0));
}

void Cpu::push(CpuMemory& memory, std::uint8_t value) {
    memory.write(static_cast<std::uint16_t>(0x0100 | sp_), value);
    --sp_;
}

std::uint8_t Cpu::pop(CpuMemory& memory) {
    ++sp_;
    return memory.read(static_cast<std::uint16_t>(0x0100 | sp_));
}

Cpu::Operand Cpu::address(CpuMemory& memory, AddrMode mode, bool penalty) {
    Operand operand;
    switch (mode) {
    case AddrMode::Implied:
    case AddrMode::Accumulator:
        break;
    case AddrMode::Immediate:
    case AddrMode::Relative:
        operand.immediate = memory.read(pc_++);
        operand.immediateValue = true;
        break;
    case AddrMode::ZeroPage:
        operand.address = memory.read(pc_++);
        break;
    case AddrMode::ZeroPageX:
        operand.address = static_cast<std::uint8_t>(memory.read(pc_++) + x_);
        break;
    case AddrMode::ZeroPageY:
        operand.address = static_cast<std::uint8_t>(memory.read(pc_++) + y_);
        break;
    case AddrMode::Absolute:
        operand.address = read16(memory, pc_);
        pc_ = static_cast<std::uint16_t>(pc_ + 2);
        break;
    case AddrMode::AbsoluteX:
    case AddrMode::AbsoluteY: {
        const std::uint16_t base = read16(memory, pc_);
        pc_ = static_cast<std::uint16_t>(pc_ + 2);
        const std::uint8_t index = mode == AddrMode::AbsoluteX ? x_ : y_;
        operand.address = static_cast<std::uint16_t>(base + index);
        if (penalty && (base & 0xFF00) != (operand.address & 0xFF00)) {
            operand.penalty = 1;
        }
        break;
    }
    case AddrMode::Indirect: {
        const std::uint16_t pointer = read16(memory, pc_);
        pc_ = static_cast<std::uint16_t>(pc_ + 2);
        const std::uint8_t low = memory.read(pointer);
        const std::uint16_t highAddress = static_cast<std::uint16_t>((pointer & 0xFF00) | ((pointer + 1) & 0x00FF));
        const std::uint8_t high = memory.read(highAddress);
        operand.address = static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8));
        break;
    }
    case AddrMode::IndirectX: {
        const std::uint8_t pointer = static_cast<std::uint8_t>(memory.read(pc_++) + x_);
        operand.address = readZeroPage16(memory, pointer);
        break;
    }
    case AddrMode::IndirectY: {
        const std::uint8_t pointer = memory.read(pc_++);
        const std::uint16_t base = readZeroPage16(memory, pointer);
        operand.address = static_cast<std::uint16_t>(base + y_);
        if (penalty && (base & 0xFF00) != (operand.address & 0xFF00)) {
            operand.penalty = 1;
        }
        break;
    }
    }
    return operand;
}

int Cpu::branch(bool condition, const Operand& operand) {
    if (!condition) {
        return 2;
    }
    const auto offset = static_cast<std::int8_t>(operand.immediate);
    const std::uint16_t next = pc_;
    pc_ = static_cast<std::uint16_t>(pc_ + offset);
    if ((next & 0xFF00) != (pc_ & 0xFF00)) {
        return 4;
    }
    return 3;
}

int Cpu::service(CpuMemory& memory, std::uint16_t vector, bool brk) {
    if (brk) {
        pc_ = static_cast<std::uint16_t>(pc_ + 1);
    }
    push(memory, static_cast<std::uint8_t>(pc_ >> 8));
    push(memory, static_cast<std::uint8_t>(pc_ & 0xFF));
    const std::uint8_t flags = brk ? static_cast<std::uint8_t>(p_ | 0x30) : static_cast<std::uint8_t>((p_ & ~0x10) | kUnused);
    push(memory, flags);
    p_ = static_cast<std::uint8_t>((p_ | kInterrupt) & ~0x10);
    p_ = static_cast<std::uint8_t>(p_ | kUnused);
    const std::uint8_t low = memory.read(vector);
    const std::uint8_t high = memory.read(static_cast<std::uint16_t>(vector + 1));
    pc_ = static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8));
    return 7;
}

void Cpu::adc(std::uint8_t value) {
    const unsigned sum = static_cast<unsigned>(a_) + value + (p_ & kCarry);
    const auto result = static_cast<std::uint8_t>(sum);
    const bool overflow = ((~(a_ ^ value) & (a_ ^ result)) & 0x80) != 0;
    p_ = static_cast<std::uint8_t>((p_ & ~(kCarry | kOverflow)) | (sum > 0xFF ? kCarry : 0) | (overflow ? kOverflow : 0));
    a_ = result;
    setZN(a_);
}

void Cpu::compare(std::uint8_t left, std::uint8_t right) {
    const auto result = static_cast<std::uint8_t>(left - right);
    p_ = static_cast<std::uint8_t>((p_ & ~kCarry) | (left >= right ? kCarry : 0));
    setZN(result);
}

int Cpu::execute(CpuMemory& memory, OpKind kind, const Operand& operand, int cycles) {
    const auto shift = [&](auto operation) {
        const bool accumulator = cycles == 2;
        const std::uint8_t value = accumulator ? a_ : memory.read(operand.address);
        if (!accumulator) {
            memory.write(operand.address, value);
        }
        const std::uint8_t result = operation(value);
        if (accumulator) {
            a_ = result;
        } else {
            memory.write(operand.address, result);
        }
        setZN(result);
    };

    switch (kind) {
    case OpKind::Adc:
        adc(operand.read(memory));
        return cycles + operand.penalty;
    case OpKind::Sbc:
        adc(static_cast<std::uint8_t>(operand.read(memory) ^ 0xFF));
        return cycles + operand.penalty;
    case OpKind::And:
        a_ = static_cast<std::uint8_t>(a_ & operand.read(memory));
        setZN(a_);
        return cycles + operand.penalty;
    case OpKind::Eor:
        a_ = static_cast<std::uint8_t>(a_ ^ operand.read(memory));
        setZN(a_);
        return cycles + operand.penalty;
    case OpKind::Ora:
        a_ = static_cast<std::uint8_t>(a_ | operand.read(memory));
        setZN(a_);
        return cycles + operand.penalty;
    case OpKind::Lda:
        a_ = operand.read(memory);
        setZN(a_);
        return cycles + operand.penalty;
    case OpKind::Ldx:
        x_ = operand.read(memory);
        setZN(x_);
        return cycles + operand.penalty;
    case OpKind::Ldy:
        y_ = operand.read(memory);
        setZN(y_);
        return cycles + operand.penalty;
    case OpKind::Sta:
        memory.write(operand.address, a_);
        return cycles;
    case OpKind::Stx:
        memory.write(operand.address, x_);
        return cycles;
    case OpKind::Sty:
        memory.write(operand.address, y_);
        return cycles;
    case OpKind::Cmp:
        compare(a_, operand.read(memory));
        return cycles + operand.penalty;
    case OpKind::Cpx:
        compare(x_, operand.read(memory));
        return cycles + operand.penalty;
    case OpKind::Cpy:
        compare(y_, operand.read(memory));
        return cycles + operand.penalty;
    case OpKind::Bit: {
        const std::uint8_t value = operand.read(memory);
        p_ = static_cast<std::uint8_t>((p_ & ~(kZero | kOverflow | kNegative)) | (value & (kOverflow | kNegative)) |
                                       ((a_ & value) == 0 ? kZero : 0));
        return cycles;
    }
    case OpKind::Asl:
        shift([&](std::uint8_t value) {
            p_ = static_cast<std::uint8_t>((p_ & ~kCarry) | ((value & 0x80) != 0 ? kCarry : 0));
            return static_cast<std::uint8_t>(value << 1);
        });
        return cycles;
    case OpKind::Lsr:
        shift([&](std::uint8_t value) {
            p_ = static_cast<std::uint8_t>((p_ & ~kCarry) | ((value & 0x01) != 0 ? kCarry : 0));
            return static_cast<std::uint8_t>(value >> 1);
        });
        return cycles;
    case OpKind::Rol:
        shift([&](std::uint8_t value) {
            const std::uint8_t carry = static_cast<std::uint8_t>(p_ & kCarry);
            p_ = static_cast<std::uint8_t>((p_ & ~kCarry) | ((value & 0x80) != 0 ? kCarry : 0));
            return static_cast<std::uint8_t>((value << 1) | carry);
        });
        return cycles;
    case OpKind::Ror:
        shift([&](std::uint8_t value) {
            const std::uint8_t carry = static_cast<std::uint8_t>((p_ & kCarry) << 7);
            p_ = static_cast<std::uint8_t>((p_ & ~kCarry) | (value & kCarry));
            return static_cast<std::uint8_t>((value >> 1) | carry);
        });
        return cycles;
    case OpKind::Inc:
    case OpKind::Dec: {
        const std::uint8_t value = memory.read(operand.address);
        memory.write(operand.address, value);
        const std::uint8_t result = kind == OpKind::Inc ? static_cast<std::uint8_t>(value + 1) : static_cast<std::uint8_t>(value - 1);
        memory.write(operand.address, result);
        setZN(result);
        return cycles;
    }
    case OpKind::Inx:
        x_ = static_cast<std::uint8_t>(x_ + 1);
        setZN(x_);
        return cycles;
    case OpKind::Dex:
        x_ = static_cast<std::uint8_t>(x_ - 1);
        setZN(x_);
        return cycles;
    case OpKind::Iny:
        y_ = static_cast<std::uint8_t>(y_ + 1);
        setZN(y_);
        return cycles;
    case OpKind::Dey:
        y_ = static_cast<std::uint8_t>(y_ - 1);
        setZN(y_);
        return cycles;
    case OpKind::Tax:
        x_ = a_;
        setZN(x_);
        return cycles;
    case OpKind::Tay:
        y_ = a_;
        setZN(y_);
        return cycles;
    case OpKind::Txa:
        a_ = x_;
        setZN(a_);
        return cycles;
    case OpKind::Tya:
        a_ = y_;
        setZN(a_);
        return cycles;
    case OpKind::Tsx:
        x_ = sp_;
        setZN(x_);
        return cycles;
    case OpKind::Txs:
        sp_ = x_;
        return cycles;
    case OpKind::Pha:
        push(memory, a_);
        return cycles;
    case OpKind::Php:
        push(memory, static_cast<std::uint8_t>(p_ | 0x30));
        return cycles;
    case OpKind::Pla:
        a_ = pop(memory);
        setZN(a_);
        return cycles;
    case OpKind::Plp: {
        const std::uint8_t value = pop(memory);
        if ((p_ & kInterrupt) != 0 && (value & kInterrupt) == 0) {
            irqDelay_ = true;
        }
        p_ = static_cast<std::uint8_t>((value & ~0x10) | kUnused);
        return cycles;
    }
    case OpKind::Clc:
        p_ = static_cast<std::uint8_t>(p_ & ~kCarry);
        return cycles;
    case OpKind::Sec:
        p_ = static_cast<std::uint8_t>(p_ | kCarry);
        return cycles;
    case OpKind::Cli:
        if ((p_ & kInterrupt) != 0) {
            irqDelay_ = true;
        }
        p_ = static_cast<std::uint8_t>(p_ & ~kInterrupt);
        return cycles;
    case OpKind::Sei:
        p_ = static_cast<std::uint8_t>(p_ | kInterrupt);
        return cycles;
    case OpKind::Cld:
        p_ = static_cast<std::uint8_t>(p_ & ~kDecimal);
        return cycles;
    case OpKind::Sed:
        p_ = static_cast<std::uint8_t>(p_ | kDecimal);
        return cycles;
    case OpKind::Clv:
        p_ = static_cast<std::uint8_t>(p_ & ~kOverflow);
        return cycles;
    case OpKind::Bpl:
        return branch((p_ & kNegative) == 0, operand);
    case OpKind::Bmi:
        return branch((p_ & kNegative) != 0, operand);
    case OpKind::Bvc:
        return branch((p_ & kOverflow) == 0, operand);
    case OpKind::Bvs:
        return branch((p_ & kOverflow) != 0, operand);
    case OpKind::Bcc:
        return branch((p_ & kCarry) == 0, operand);
    case OpKind::Bcs:
        return branch((p_ & kCarry) != 0, operand);
    case OpKind::Bne:
        return branch((p_ & kZero) == 0, operand);
    case OpKind::Beq:
        return branch((p_ & kZero) != 0, operand);
    case OpKind::Jmp:
        pc_ = operand.address;
        return cycles;
    case OpKind::Jsr: {
        const std::uint16_t returnAddress = static_cast<std::uint16_t>(pc_ - 1);
        push(memory, static_cast<std::uint8_t>(returnAddress >> 8));
        push(memory, static_cast<std::uint8_t>(returnAddress & 0xFF));
        pc_ = operand.address;
        return cycles;
    }
    case OpKind::Rts: {
        const std::uint8_t low = pop(memory);
        const std::uint8_t high = pop(memory);
        pc_ = static_cast<std::uint16_t>((static_cast<unsigned>(high) << 8 | low) + 1);
        return cycles;
    }
    case OpKind::Rti: {
        p_ = static_cast<std::uint8_t>((pop(memory) & ~0x10) | kUnused);
        const std::uint8_t low = pop(memory);
        const std::uint8_t high = pop(memory);
        pc_ = static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8));
        return cycles;
    }
    case OpKind::Brk:
        return service(memory, 0xFFFE, true);
    case OpKind::Nop:
        return cycles;
    case OpKind::Unknown:
        ++unknownOpcodes_;
        if (!loggedUnknown_) {
            loggedUnknown_ = true;
            log::warn("An unimplemented CPU opcode was treated as a one-byte NOP.");
        }
        return 2;
    }
    return cycles;
}

int Cpu::step(CpuMemory& memory) {
    const bool suppressIrq = irqDelay_;
    irqDelay_ = false;
    if (nmiPending_) {
        nmiPending_ = false;
        return service(memory, 0xFFFA, false);
    }
    if (!suppressIrq && irqLine_ && (p_ & kInterrupt) == 0) {
        return service(memory, 0xFFFE, false);
    }

    const std::uint8_t opcode = memory.read(pc_++);
    const Entry& entry = decode(opcode);
    const Operand operand = address(memory, entry.mode, entry.penalty);
    return execute(memory, entry.kind, operand, entry.cycles);
}

}  // namespace iris::systems::nes
