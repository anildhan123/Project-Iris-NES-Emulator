#include "iris/snes/cpu.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::snes {
namespace {

constexpr std::uint8_t kCarry = 0x01;
constexpr std::uint8_t kZero = 0x02;
constexpr std::uint8_t kIrqDisable = 0x04;
constexpr std::uint8_t kDecimal = 0x08;
constexpr std::uint8_t kOverflow = 0x40;
constexpr std::uint8_t kNegative = 0x80;

}  // namespace

void Cpu::reset(Bus& bus) {
    const std::uint16_t vector = static_cast<std::uint16_t>(bus.read(0, 0xFFFC) | (bus.read(0, 0xFFFD) << 8));
    resetTo(vector);
}

void Cpu::resetTo(std::uint16_t pc) {
    stopped_ = false;
    stopOpcode_ = 0;
    nmiPending_ = false;
    irqPending_ = false;
    waiting_ = false;
    spinning_ = false;
    emulation_ = true;
    pbr_ = 0;
    dbr_ = 0;
    d_ = 0;
    a_ = 0;
    x_ = 0;
    y_ = 0;
    sp_ = 0x01FF;
    p_ = static_cast<std::uint8_t>(p_ | kIrqDisable | 0x30);
    p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kDecimal));
    pc_ = pc;
}

void Cpu::stop(std::uint8_t opcode) {
    stopped_ = true;
    stopOpcode_ = opcode;
}

std::uint8_t Cpu::fetch(Bus& bus) {
    const std::uint8_t value = bus.read(pbr_, pc_);
    pc_ = static_cast<std::uint16_t>(pc_ + 1);
    return value;
}

std::uint8_t Cpu::read(Bus& bus, std::uint16_t address) {
    return bus.read(0, address);
}

void Cpu::write(Bus& bus, std::uint16_t address, std::uint8_t value) {
    bus.write(0, address, value);
}

std::uint16_t Cpu::read16(Bus& bus, std::uint16_t address) {
    const std::uint8_t low = read(bus, address);
    const std::uint8_t high = read(bus, static_cast<std::uint16_t>(address + 1));
    return static_cast<std::uint16_t>(low | (high << 8));
}

void Cpu::setZN(std::uint8_t value) {
    p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0) {
        p_ = static_cast<std::uint8_t>(p_ | kZero);
    }
    if ((value & 0x80) != 0) {
        p_ = static_cast<std::uint8_t>(p_ | kNegative);
    }
}

void Cpu::setZN16(std::uint16_t value) {
    p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0) {
        p_ = static_cast<std::uint8_t>(p_ | kZero);
    }
    if ((value & 0x8000) != 0) {
        p_ = static_cast<std::uint8_t>(p_ | kNegative);
    }
}

void Cpu::applyWidths() {
    if (emulation_) {
        p_ = static_cast<std::uint8_t>(p_ | 0x30);
        sp_ = static_cast<std::uint16_t>(0x0100 | (sp_ & 0xFF));
        d_ = 0;
    }
}

void Cpu::exchangeCarry() {
    const bool carry = (p_ & kCarry) != 0;
    if (emulation_) {
        p_ = static_cast<std::uint8_t>(p_ | kCarry);
    } else {
        p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kCarry));
    }
    emulation_ = carry;
    applyWidths();
}

void Cpu::push(Bus& bus, std::uint8_t value) {
    if (emulation_) {
        write(bus, static_cast<std::uint16_t>(0x0100 | (sp_ & 0xFF)), value);
        sp_ = static_cast<std::uint16_t>(0x0100 | ((sp_ - 1) & 0xFF));
        return;
    }
    write(bus, sp_, value);
    sp_ = static_cast<std::uint16_t>(sp_ - 1);
}

std::uint8_t Cpu::pull(Bus& bus) {
    if (emulation_) {
        sp_ = static_cast<std::uint16_t>(0x0100 | ((sp_ + 1) & 0xFF));
        return read(bus, sp_);
    }
    sp_ = static_cast<std::uint16_t>(sp_ + 1);
    return read(bus, sp_);
}

void Cpu::compare(std::uint8_t left, std::uint8_t right) {
    const int result = left - right;
    p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kCarry));
    if (left >= right) {
        p_ = static_cast<std::uint8_t>(p_ | kCarry);
    }
    setZN(static_cast<std::uint8_t>(result));
}

void Cpu::adc(std::uint8_t value) {
    const int carryIn = (p_ & kCarry) != 0 ? 1 : 0;
    int sum = 0;
    if ((p_ & kDecimal) != 0) {
        int low = (a_ & 0x0F) + (value & 0x0F) + carryIn;
        if (low >= 10) {
            low += 6;
        }
        int high = ((a_ & 0xF0) >> 4) + ((value & 0xF0) >> 4) + (low >> 4);
        if (high >= 10) {
            high += 6;
        }
        sum = (low & 0x0F) | ((high & 0x0F) << 4);
        p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (high >= 16) {
            p_ = static_cast<std::uint8_t>(p_ | kCarry);
        }
    } else {
        sum = (a_ & 0xFF) + value + carryIn;
        p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFF) {
            p_ = static_cast<std::uint8_t>(p_ | kCarry);
        }
        if (((a_ ^ sum) & (value ^ sum) & 0x80) != 0) {
            p_ = static_cast<std::uint8_t>(p_ | kOverflow);
        }
    }
    a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | static_cast<std::uint8_t>(sum));
    setZN(static_cast<std::uint8_t>(a_));
}

void Cpu::sbc(std::uint8_t value) {
    adc(static_cast<std::uint8_t>(value ^ 0xFF));
}

void Cpu::branch(Bus& bus, bool taken) {
    const auto displacement = static_cast<std::int8_t>(fetch(bus));
    if (!taken) {
        return;
    }
    pc_ = static_cast<std::uint16_t>(pc_ + displacement);
}

void Cpu::signalNmi() {
    nmiPending_ = true;
}

void Cpu::signalIrq() {
    irqPending_ = true;
}

void Cpu::interrupt(Bus& bus, std::uint16_t vector) {
    if (!emulation_) {
        push(bus, pbr_);
    }
    push(bus, static_cast<std::uint8_t>(pc_ >> 8));
    push(bus, static_cast<std::uint8_t>(pc_));
    push(bus, emulation_ ? static_cast<std::uint8_t>(p_ | 0x30) : p_);
    p_ = static_cast<std::uint8_t>(p_ | kIrqDisable);
    pbr_ = 0;
    pc_ = static_cast<std::uint16_t>(bus.read(0, vector) | (bus.read(0, static_cast<std::uint16_t>(vector + 1)) << 8));
}

bool Cpu::parked() const {
    return waiting_ && !stopped_ && !nmiPending_ && !(irqPending_ && (p_ & kIrqDisable) == 0);
}

bool Cpu::consumeSpin() {
    const bool spin = spinning_;
    spinning_ = false;
    return spin;
}

int Cpu::step(Bus& bus) {
    if (stopped_) {
        return 0;
    }
    if (waiting_ && !nmiPending_ && !(irqPending_ && (p_ & kIrqDisable) == 0)) {
        return 2;
    }
    waiting_ = false;
    if (nmiPending_) {
        nmiPending_ = false;
        interrupt(bus, emulation_ ? 0xFFFA : 0xFFEA);
        return 7;
    }
    if (irqPending_ && (p_ & kIrqDisable) == 0) {
        irqPending_ = false;
        interrupt(bus, emulation_ ? 0xFFFE : 0xFFEE);
        return 7;
    }
    const std::uint8_t opcode = fetch(bus);
    return execute(bus, opcode);
}

namespace {

enum class Kind { Ora, And, Eor, Adc, Sbc, Cmp, Lda, Sta, Cpx, Cpy, Ldx, Ldy, Stx, Sty, Stz, Bit, Inc, Dec, Asl, Lsr, Rol, Ror, Tsb, Trb };

enum class Mode {
    ImmA, ImmX, Dp, DpX, DpY, Abs, AbsX, AbsY, Long, LongX, IndX, IndY, Ind, IndL, IndLY, Sr, SrY, Acc
};

bool grid(std::uint8_t opcode, Kind& kind, Mode& mode) {
    switch (opcode & 0x1F) {
    case 0x01: mode = Mode::IndX; break;
    case 0x03: mode = Mode::Sr; break;
    case 0x05: mode = Mode::Dp; break;
    case 0x07: mode = Mode::IndL; break;
    case 0x09: mode = Mode::ImmA; break;
    case 0x0D: mode = Mode::Abs; break;
    case 0x0F: mode = Mode::Long; break;
    case 0x11: mode = Mode::IndY; break;
    case 0x12: mode = Mode::Ind; break;
    case 0x13: mode = Mode::SrY; break;
    case 0x15: mode = Mode::DpX; break;
    case 0x17: mode = Mode::IndLY; break;
    case 0x19: mode = Mode::AbsY; break;
    case 0x1D: mode = Mode::AbsX; break;
    case 0x1F: mode = Mode::LongX; break;
    default: return false;
    }
    if ((opcode & 0x1F) == 0x09 && (opcode >> 5) == 4) {
        return false;
    }
    static constexpr Kind kRow[] = {Kind::Ora, Kind::And, Kind::Eor, Kind::Adc, Kind::Sta, Kind::Lda, Kind::Cmp, Kind::Sbc};
    kind = kRow[opcode >> 5];
    return true;
}

bool decodeExtra(std::uint8_t opcode, Kind& kind, Mode& mode) {
    switch (opcode) {
    case 0x06: kind = Kind::Asl; mode = Mode::Dp; return true;
    case 0x0A: kind = Kind::Asl; mode = Mode::Acc; return true;
    case 0x0E: kind = Kind::Asl; mode = Mode::Abs; return true;
    case 0x16: kind = Kind::Asl; mode = Mode::DpX; return true;
    case 0x1E: kind = Kind::Asl; mode = Mode::AbsX; return true;
    case 0x26: kind = Kind::Rol; mode = Mode::Dp; return true;
    case 0x2A: kind = Kind::Rol; mode = Mode::Acc; return true;
    case 0x2E: kind = Kind::Rol; mode = Mode::Abs; return true;
    case 0x36: kind = Kind::Rol; mode = Mode::DpX; return true;
    case 0x3E: kind = Kind::Rol; mode = Mode::AbsX; return true;
    case 0x46: kind = Kind::Lsr; mode = Mode::Dp; return true;
    case 0x4A: kind = Kind::Lsr; mode = Mode::Acc; return true;
    case 0x4E: kind = Kind::Lsr; mode = Mode::Abs; return true;
    case 0x56: kind = Kind::Lsr; mode = Mode::DpX; return true;
    case 0x5E: kind = Kind::Lsr; mode = Mode::AbsX; return true;
    case 0x66: kind = Kind::Ror; mode = Mode::Dp; return true;
    case 0x6A: kind = Kind::Ror; mode = Mode::Acc; return true;
    case 0x6E: kind = Kind::Ror; mode = Mode::Abs; return true;
    case 0x76: kind = Kind::Ror; mode = Mode::DpX; return true;
    case 0x7E: kind = Kind::Ror; mode = Mode::AbsX; return true;
    case 0xE6: kind = Kind::Inc; mode = Mode::Dp; return true;
    case 0xEE: kind = Kind::Inc; mode = Mode::Abs; return true;
    case 0xF6: kind = Kind::Inc; mode = Mode::DpX; return true;
    case 0xFE: kind = Kind::Inc; mode = Mode::AbsX; return true;
    case 0xC6: kind = Kind::Dec; mode = Mode::Dp; return true;
    case 0xCE: kind = Kind::Dec; mode = Mode::Abs; return true;
    case 0xD6: kind = Kind::Dec; mode = Mode::DpX; return true;
    case 0xDE: kind = Kind::Dec; mode = Mode::AbsX; return true;
    case 0x64: kind = Kind::Stz; mode = Mode::Dp; return true;
    case 0x74: kind = Kind::Stz; mode = Mode::DpX; return true;
    case 0x9C: kind = Kind::Stz; mode = Mode::Abs; return true;
    case 0x9E: kind = Kind::Stz; mode = Mode::AbsX; return true;
    case 0x24: kind = Kind::Bit; mode = Mode::Dp; return true;
    case 0x2C: kind = Kind::Bit; mode = Mode::Abs; return true;
    case 0x34: kind = Kind::Bit; mode = Mode::DpX; return true;
    case 0x3C: kind = Kind::Bit; mode = Mode::AbsX; return true;
    case 0x89: kind = Kind::Bit; mode = Mode::ImmA; return true;
    case 0x04: kind = Kind::Tsb; mode = Mode::Dp; return true;
    case 0x0C: kind = Kind::Tsb; mode = Mode::Abs; return true;
    case 0x14: kind = Kind::Trb; mode = Mode::Dp; return true;
    case 0x1C: kind = Kind::Trb; mode = Mode::Abs; return true;
    case 0xA2: kind = Kind::Ldx; mode = Mode::ImmX; return true;
    case 0xA6: kind = Kind::Ldx; mode = Mode::Dp; return true;
    case 0xAE: kind = Kind::Ldx; mode = Mode::Abs; return true;
    case 0xB6: kind = Kind::Ldx; mode = Mode::DpY; return true;
    case 0xBE: kind = Kind::Ldx; mode = Mode::AbsY; return true;
    case 0xA0: kind = Kind::Ldy; mode = Mode::ImmX; return true;
    case 0xA4: kind = Kind::Ldy; mode = Mode::Dp; return true;
    case 0xAC: kind = Kind::Ldy; mode = Mode::Abs; return true;
    case 0xB4: kind = Kind::Ldy; mode = Mode::DpX; return true;
    case 0xBC: kind = Kind::Ldy; mode = Mode::AbsX; return true;
    case 0x86: kind = Kind::Stx; mode = Mode::Dp; return true;
    case 0x8E: kind = Kind::Stx; mode = Mode::Abs; return true;
    case 0x96: kind = Kind::Stx; mode = Mode::DpY; return true;
    case 0x84: kind = Kind::Sty; mode = Mode::Dp; return true;
    case 0x8C: kind = Kind::Sty; mode = Mode::Abs; return true;
    case 0x94: kind = Kind::Sty; mode = Mode::DpX; return true;
    case 0xE0: kind = Kind::Cpx; mode = Mode::ImmX; return true;
    case 0xE4: kind = Kind::Cpx; mode = Mode::Dp; return true;
    case 0xEC: kind = Kind::Cpx; mode = Mode::Abs; return true;
    case 0xC0: kind = Kind::Cpy; mode = Mode::ImmX; return true;
    case 0xC4: kind = Kind::Cpy; mode = Mode::Dp; return true;
    case 0xCC: kind = Kind::Cpy; mode = Mode::Abs; return true;
    default: return false;
    }
}

}  // namespace

int Cpu::execute(Bus& bus, std::uint8_t opcode) {
    // Branches and jumps are the idle loop between vblanks. Handling them
    // before the arithmetic decoder keeps that spin off the large switch.
    switch (opcode) {
    case 0xEA:
        return 2;
    case 0x4C: {
        const std::uint16_t target = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
        if (target == static_cast<std::uint16_t>(pc_ - 3)) {
            spinning_ = true;
        }
        pc_ = target;
        return 3;
    }
    case 0x10: branch(bus, (p_ & kNegative) == 0); return 2;
    case 0x30: branch(bus, (p_ & kNegative) != 0); return 2;
    case 0x50: branch(bus, (p_ & kOverflow) == 0); return 2;
    case 0x70: branch(bus, (p_ & kOverflow) != 0); return 2;
    case 0x90: branch(bus, (p_ & kCarry) == 0); return 2;
    case 0xB0: branch(bus, (p_ & kCarry) != 0); return 2;
    case 0xD0: branch(bus, (p_ & kZero) == 0); return 2;
    case 0xF0: branch(bus, (p_ & kZero) != 0); return 2;
    case 0x80: branch(bus, true); return 2;
    default:
        break;
    }
    Kind kind{};
    Mode mode{};
    if (grid(opcode, kind, mode) || decodeExtra(opcode, kind, mode)) {
        const bool indexOp = kind == Kind::Ldx || kind == Kind::Ldy || kind == Kind::Cpx || kind == Kind::Cpy ||
                              kind == Kind::Stx || kind == Kind::Sty;
        const bool narrow = indexOp ? index8() : accumulator8();
        std::uint8_t bank = 0;
        std::uint16_t address = 0;
        bool immediate = false;
        bool accumulator = false;
        std::uint16_t immediateValue = 0;
        const auto addIndex = [&](std::uint16_t base, std::uint16_t index) {
            return static_cast<std::uint16_t>(base + index);
        };
        const auto addIndexed = [&](std::uint16_t base, std::uint16_t index) {
            const unsigned sum = static_cast<unsigned>(base) + index;
            if (sum > 0xFFFF) {
                bank = static_cast<std::uint8_t>(bank + 1);
            }
            return static_cast<std::uint16_t>(sum);
        };
        switch (mode) {
        case Mode::ImmA:
            immediate = true;
            immediateValue = fetch(bus);
            if (!accumulator8()) {
                immediateValue = static_cast<std::uint16_t>(immediateValue | (fetch(bus) << 8));
            }
            break;
        case Mode::ImmX:
            immediate = true;
            immediateValue = fetch(bus);
            if (!index8()) {
                immediateValue = static_cast<std::uint16_t>(immediateValue | (fetch(bus) << 8));
            }
            break;
        case Mode::Dp:
            address = static_cast<std::uint16_t>(directBase() + fetch(bus));
            break;
        case Mode::DpX:
            address = addIndex(static_cast<std::uint16_t>(directBase() + fetch(bus)), indexValue());
            break;
        case Mode::DpY:
            address = addIndex(static_cast<std::uint16_t>(directBase() + fetch(bus)), indexY());
            break;
        case Mode::Abs:
            bank = dbr_;
            address = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
            break;
        case Mode::AbsX:
            bank = dbr_;
            address = addIndexed(static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8)), indexValue());
            break;
        case Mode::AbsY:
            bank = dbr_;
            address = addIndexed(static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8)), indexY());
            break;
        case Mode::Long:
            address = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
            bank = fetch(bus);
            break;
        case Mode::LongX: {
            const std::uint32_t linear = (fetch(bus) | (static_cast<std::uint32_t>(fetch(bus)) << 8) |
                                           (static_cast<std::uint32_t>(fetch(bus)) << 16)) +
                                          indexValue();
            bank = static_cast<std::uint8_t>(linear >> 16);
            address = static_cast<std::uint16_t>(linear);
            break;
        }
        case Mode::IndX: {
            const std::uint16_t pointer = addIndex(static_cast<std::uint16_t>(directBase() + fetch(bus)), indexValue());
            address = read16(bus, pointer);
            bank = dbr_;
            break;
        }
        case Mode::Ind: {
            const std::uint16_t pointer = static_cast<std::uint16_t>(directBase() + fetch(bus));
            address = read16(bus, pointer);
            bank = dbr_;
            break;
        }
        case Mode::IndY: {
            const std::uint16_t pointer = static_cast<std::uint16_t>(directBase() + fetch(bus));
            bank = dbr_;
            address = addIndexed(read16(bus, pointer), indexY());
            break;
        }
        case Mode::IndL: {
            const std::uint16_t pointer = static_cast<std::uint16_t>(directBase() + fetch(bus));
            address = read16(bus, pointer);
            bank = read(bus, static_cast<std::uint16_t>(pointer + 2));
            break;
        }
        case Mode::IndLY: {
            const std::uint16_t pointer = static_cast<std::uint16_t>(directBase() + fetch(bus));
            const std::uint32_t linear = (read16(bus, pointer) | (static_cast<std::uint32_t>(read(bus, static_cast<std::uint16_t>(pointer + 2))) << 16)) + indexY();
            bank = static_cast<std::uint8_t>(linear >> 16);
            address = static_cast<std::uint16_t>(linear);
            break;
        }
        case Mode::Sr:
            address = static_cast<std::uint16_t>(sp_ + fetch(bus));
            break;
        case Mode::SrY: {
            const std::uint16_t pointer = static_cast<std::uint16_t>(sp_ + fetch(bus));
            bank = dbr_;
            address = addIndexed(read16(bus, pointer), indexY());
            break;
        }
        case Mode::Acc:
            accumulator = true;
            break;
        }
        const auto load = [&]() {
            if (immediate) {
                return immediateValue;
            }
            if (accumulator) {
                return narrow ? static_cast<std::uint16_t>(a_ & 0xFF) : a_;
            }
            const std::uint8_t low = bus.read(bank, address);
            if (narrow) {
                return static_cast<std::uint16_t>(low);
            }
            return static_cast<std::uint16_t>(low | (bus.read(bank, static_cast<std::uint16_t>(address + 1)) << 8));
        };
        const auto storeMem = [&](std::uint16_t value) {
            if (accumulator) {
                if (narrow) {
                    a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | (value & 0xFF));
                    setZN(static_cast<std::uint8_t>(a_));
                } else {
                    a_ = value;
                    setZN16(a_);
                }
                return;
            }
            bus.write(bank, address, static_cast<std::uint8_t>(value));
            if (!narrow) {
                bus.write(bank, static_cast<std::uint16_t>(address + 1), static_cast<std::uint8_t>(value >> 8));
            }
        };
        const auto setA = [&](std::uint16_t value) {
            if (accumulator8()) {
                a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | (value & 0xFF));
                setZN(static_cast<std::uint8_t>(a_));
            } else {
                a_ = value;
                setZN16(a_);
            }
        };
        const auto accumulatorValue = [&]() {
            return accumulator8() ? static_cast<std::uint16_t>(a_ & 0xFF) : a_;
        };
        const auto setIndex = [&](std::uint16_t& reg, std::uint16_t value) {
            if (index8()) {
                reg = static_cast<std::uint16_t>((reg & 0xFF00) | (value & 0xFF));
                setZN(static_cast<std::uint8_t>(reg));
            } else {
                reg = value;
                setZN16(reg);
            }
        };
        if (kind == Kind::Sta || kind == Kind::Stx || kind == Kind::Sty || kind == Kind::Stz) {
            std::uint16_t value = 0;
            if (kind == Kind::Sta) {
                value = accumulatorValue();
            } else if (kind == Kind::Stx) {
                value = index8() ? static_cast<std::uint16_t>(x_ & 0xFF) : x_;
            } else if (kind == Kind::Sty) {
                value = index8() ? static_cast<std::uint16_t>(y_ & 0xFF) : y_;
            }
            storeMem(value);
            return 4;
        }
        const std::uint16_t value = load();
        if (kind == Kind::Lda) {
            setA(value);
        } else if (kind == Kind::Ldx) {
            setIndex(x_, value);
        } else if (kind == Kind::Ldy) {
            setIndex(y_, value);
        } else if (kind == Kind::Ora) {
            setA(static_cast<std::uint16_t>(accumulatorValue() | value));
        } else if (kind == Kind::And) {
            setA(static_cast<std::uint16_t>(accumulatorValue() & value));
        } else if (kind == Kind::Eor) {
            setA(static_cast<std::uint16_t>(accumulatorValue() ^ value));
        } else if (kind == Kind::Adc) {
            if (accumulator8()) {
                adc(static_cast<std::uint8_t>(value));
            } else {
                const int left = a_;
                const int sum = left + value + ((p_ & kCarry) != 0 ? 1 : 0);
                p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
                if (sum > 0xFFFF) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                if (((left ^ sum) & (value ^ sum) & 0x8000) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kOverflow);
                }
                a_ = static_cast<std::uint16_t>(sum);
                setZN16(a_);
            }
        } else if (kind == Kind::Sbc) {
            if (accumulator8()) {
                sbc(static_cast<std::uint8_t>(value));
            } else {
                const int left = a_;
                const int inverted = value ^ 0xFFFF;
                const int sum = left + inverted + ((p_ & kCarry) != 0 ? 1 : 0);
                p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
                if (sum > 0xFFFF) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                if (((left ^ sum) & (inverted ^ sum) & 0x8000) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kOverflow);
                }
                a_ = static_cast<std::uint16_t>(sum);
                setZN16(a_);
            }
        } else if (kind == Kind::Cmp || kind == Kind::Cpx || kind == Kind::Cpy) {
            const std::uint16_t left = kind == Kind::Cmp ? accumulatorValue() : (kind == Kind::Cpx ? (index8() ? static_cast<std::uint16_t>(x_ & 0xFF) : x_) : (index8() ? static_cast<std::uint16_t>(y_ & 0xFF) : y_));
            if (narrow) {
                compare(static_cast<std::uint8_t>(left), static_cast<std::uint8_t>(value));
            } else {
                p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kCarry));
                if (left >= value) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                setZN16(static_cast<std::uint16_t>(left - value));
            }
        } else if (kind == Kind::Bit) {
            const std::uint16_t mask = narrow ? 0xFF : 0xFFFF;
            const std::uint16_t tested = static_cast<std::uint16_t>(accumulatorValue() & value & mask);
            p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kZero));
            if (tested == 0) {
                p_ = static_cast<std::uint8_t>(p_ | kZero);
            }
            if (mode != Mode::ImmA) {
                const int top = narrow ? 0x80 : 0x8000;
                p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~(kNegative | kOverflow)));
                if ((value & top) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kNegative);
                }
                if ((value & (top >> 1)) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kOverflow);
                }
            }
        } else if (kind == Kind::Inc || kind == Kind::Dec) {
            const std::uint16_t result = kind == Kind::Inc ? static_cast<std::uint16_t>(value + 1) : static_cast<std::uint16_t>(value - 1);
            storeMem(narrow ? static_cast<std::uint16_t>(result & 0xFF) : result);
            if (narrow) {
                setZN(static_cast<std::uint8_t>(result));
            } else {
                setZN16(result);
            }
        } else if (kind == Kind::Asl || kind == Kind::Lsr || kind == Kind::Rol || kind == Kind::Ror) {
            const int mask = narrow ? 0xFF : 0xFFFF;
            const int top = narrow ? 0x80 : 0x8000;
            const int bits = value & mask;
            const int oldCarry = (p_ & kCarry) != 0 ? 1 : 0;
            int result = bits;
            p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kCarry));
            if (kind == Kind::Asl) {
                if ((bits & top) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                result = (bits << 1) & mask;
            } else if (kind == Kind::Lsr) {
                if ((bits & 1) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                result = bits >> 1;
            } else if (kind == Kind::Rol) {
                if ((bits & top) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                result = ((bits << 1) | oldCarry) & mask;
            } else {
                if ((bits & 1) != 0) {
                    p_ = static_cast<std::uint8_t>(p_ | kCarry);
                }
                result = (bits >> 1) | (oldCarry != 0 ? top : 0);
            }
            storeMem(static_cast<std::uint16_t>(result));
            if (narrow) {
                setZN(static_cast<std::uint8_t>(result));
            } else {
                setZN16(static_cast<std::uint16_t>(result));
            }
        } else if (kind == Kind::Tsb || kind == Kind::Trb) {
            const std::uint16_t acc = accumulatorValue();
            p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kZero));
            if ((acc & value) == 0) {
                p_ = static_cast<std::uint8_t>(p_ | kZero);
            }
            storeMem(kind == Kind::Tsb ? static_cast<std::uint16_t>(value | acc) : static_cast<std::uint16_t>(value & static_cast<std::uint16_t>(~acc)));
        }
        return 4;
    }

    const auto push16 = [&](std::uint16_t value) {
        push(bus, static_cast<std::uint8_t>(value >> 8));
        push(bus, static_cast<std::uint8_t>(value));
    };
    const auto pull16 = [&]() {
        const std::uint8_t low = pull(bus);
        const std::uint8_t high = pull(bus);
        return static_cast<std::uint16_t>(low | (high << 8));
    };
    switch (opcode) {
    case 0x42:
        (void)fetch(bus);
        return 2;
    case 0xCB:
        waiting_ = true;
        return 3;
    case 0xDB:
        stop(opcode);
        return 0;
    case 0x00:
        (void)fetch(bus);
        interrupt(bus, emulation_ ? 0xFFFE : 0xFFE6);
        return 7;
    case 0x02:
        (void)fetch(bus);
        interrupt(bus, emulation_ ? 0xFFF4 : 0xFFE4);
        return 7;
    case 0x18: p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kCarry)); return 2;
    case 0x38: p_ = static_cast<std::uint8_t>(p_ | kCarry); return 2;
    case 0x58: p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kIrqDisable)); return 2;
    case 0x78: p_ = static_cast<std::uint8_t>(p_ | kIrqDisable); return 2;
    case 0xD8: p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kDecimal)); return 2;
    case 0xF8: p_ = static_cast<std::uint8_t>(p_ | kDecimal); return 2;
    case 0xB8: p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~kOverflow)); return 2;
    case 0xFB: exchangeCarry(); return 2;
    case 0xE2: p_ = static_cast<std::uint8_t>(p_ | fetch(bus)); applyWidths(); return 3;
    case 0xC2: p_ = static_cast<std::uint8_t>(p_ & static_cast<std::uint8_t>(~fetch(bus))); applyWidths(); return 3;
    case 0x82: pc_ = static_cast<std::uint16_t>(pc_ + static_cast<std::int16_t>(fetch(bus) | (fetch(bus) << 8))); return 3;
    case 0x1A:
        if (accumulator8()) { a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | static_cast<std::uint8_t>(a_ + 1)); setZN(static_cast<std::uint8_t>(a_)); }
        else { a_ = static_cast<std::uint16_t>(a_ + 1); setZN16(a_); }
        return 2;
    case 0x3A:
        if (accumulator8()) { a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | static_cast<std::uint8_t>(a_ - 1)); setZN(static_cast<std::uint8_t>(a_)); }
        else { a_ = static_cast<std::uint16_t>(a_ - 1); setZN16(a_); }
        return 2;
    case 0xE8:
        if (index8()) { x_ = static_cast<std::uint16_t>((x_ & 0xFF00) | static_cast<std::uint8_t>(x_ + 1)); setZN(static_cast<std::uint8_t>(x_)); }
        else { x_ = static_cast<std::uint16_t>(x_ + 1); setZN16(x_); }
        return 2;
    case 0xCA:
        if (index8()) { x_ = static_cast<std::uint16_t>((x_ & 0xFF00) | static_cast<std::uint8_t>(x_ - 1)); setZN(static_cast<std::uint8_t>(x_)); }
        else { x_ = static_cast<std::uint16_t>(x_ - 1); setZN16(x_); }
        return 2;
    case 0xC8:
        if (index8()) { y_ = static_cast<std::uint16_t>((y_ & 0xFF00) | static_cast<std::uint8_t>(y_ + 1)); setZN(static_cast<std::uint8_t>(y_)); }
        else { y_ = static_cast<std::uint16_t>(y_ + 1); setZN16(y_); }
        return 2;
    case 0x88:
        if (index8()) { y_ = static_cast<std::uint16_t>((y_ & 0xFF00) | static_cast<std::uint8_t>(y_ - 1)); setZN(static_cast<std::uint8_t>(y_)); }
        else { y_ = static_cast<std::uint16_t>(y_ - 1); setZN16(y_); }
        return 2;
    case 0xAA:
        if (index8()) { x_ = static_cast<std::uint16_t>((x_ & 0xFF00) | (a_ & 0xFF)); setZN(static_cast<std::uint8_t>(x_)); }
        else { x_ = a_; setZN16(x_); }
        return 2;
    case 0xA8:
        if (index8()) { y_ = static_cast<std::uint16_t>((y_ & 0xFF00) | (a_ & 0xFF)); setZN(static_cast<std::uint8_t>(y_)); }
        else { y_ = a_; setZN16(y_); }
        return 2;
    case 0x8A:
        if (accumulator8()) { a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | (x_ & 0xFF)); setZN(static_cast<std::uint8_t>(a_)); }
        else { a_ = index8() ? static_cast<std::uint16_t>(static_cast<std::int8_t>(x_)) : x_; setZN16(a_); }
        return 2;
    case 0x98:
        if (accumulator8()) { a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | (y_ & 0xFF)); setZN(static_cast<std::uint8_t>(a_)); }
        else { a_ = index8() ? static_cast<std::uint16_t>(static_cast<std::int8_t>(y_)) : y_; setZN16(a_); }
        return 2;
    case 0x9B:
        if (index8()) { y_ = static_cast<std::uint16_t>((y_ & 0xFF00) | (x_ & 0xFF)); setZN(static_cast<std::uint8_t>(y_)); }
        else { y_ = x_; setZN16(y_); }
        return 2;
    case 0xBB:
        if (index8()) { x_ = static_cast<std::uint16_t>((x_ & 0xFF00) | (y_ & 0xFF)); setZN(static_cast<std::uint8_t>(x_)); }
        else { x_ = y_; setZN16(x_); }
        return 2;
    case 0x9A:
        sp_ = emulation_ ? static_cast<std::uint16_t>(0x0100 | (x_ & 0xFF)) : x_;
        return 2;
    case 0xBA:
        if (index8()) { x_ = static_cast<std::uint16_t>((x_ & 0xFF00) | (sp_ & 0xFF)); setZN(static_cast<std::uint8_t>(x_)); }
        else { x_ = sp_; setZN16(x_); }
        return 2;
    case 0x1B:
        sp_ = emulation_ ? static_cast<std::uint16_t>(0x0100 | (a_ & 0xFF)) : a_;
        return 2;
    case 0x3B: a_ = sp_; if (accumulator8()) setZN(static_cast<std::uint8_t>(a_)); else setZN16(a_); return 2;
    case 0x5B: d_ = a_; if (emulation_) d_ = 0; if (accumulator8()) setZN(static_cast<std::uint8_t>(a_)); else setZN16(a_); return 2;
    case 0x7B: a_ = d_; if (accumulator8()) setZN(static_cast<std::uint8_t>(a_)); else setZN16(a_); return 2;
    case 0xEB: {
        const std::uint8_t low = static_cast<std::uint8_t>(a_);
        const std::uint8_t high = static_cast<std::uint8_t>(a_ >> 8);
        a_ = static_cast<std::uint16_t>((low << 8) | high);
        setZN(high);
        return 3;
    }
    case 0x48:
        if (!accumulator8()) push(bus, static_cast<std::uint8_t>(a_ >> 8));
        push(bus, static_cast<std::uint8_t>(a_));
        return 3;
    case 0x68:
        if (!accumulator8()) {
            const std::uint8_t low = pull(bus);
            a_ = static_cast<std::uint16_t>(low | (pull(bus) << 8));
            setZN16(a_);
        } else {
            a_ = static_cast<std::uint16_t>((a_ & 0xFF00) | pull(bus));
            setZN(static_cast<std::uint8_t>(a_));
        }
        return 4;
    case 0x08: push(bus, emulation_ ? static_cast<std::uint8_t>(p_ | 0x30) : p_); return 3;
    case 0x28: {
        std::uint8_t flags = pull(bus);
        if (emulation_) flags = static_cast<std::uint8_t>((flags & 0xCF) | 0x30);
        p_ = flags;
        applyWidths();
        return 4;
    }
    case 0xDA: if (!index8()) push(bus, static_cast<std::uint8_t>(x_ >> 8)); push(bus, static_cast<std::uint8_t>(x_)); return 3;
    case 0x5A: if (!index8()) push(bus, static_cast<std::uint8_t>(y_ >> 8)); push(bus, static_cast<std::uint8_t>(y_)); return 3;
    case 0xFA:
        if (!index8()) { const std::uint8_t low = pull(bus); x_ = static_cast<std::uint16_t>(low | (pull(bus) << 8)); setZN16(x_); }
        else { x_ = static_cast<std::uint16_t>((x_ & 0xFF00) | pull(bus)); setZN(static_cast<std::uint8_t>(x_)); }
        return 4;
    case 0x7A:
        if (!index8()) { const std::uint8_t low = pull(bus); y_ = static_cast<std::uint16_t>(low | (pull(bus) << 8)); setZN16(y_); }
        else { y_ = static_cast<std::uint16_t>((y_ & 0xFF00) | pull(bus)); setZN(static_cast<std::uint8_t>(y_)); }
        return 4;
    case 0x4B: push(bus, pbr_); return 3;
    case 0x8B: push(bus, dbr_); return 3;
    case 0xAB: dbr_ = pull(bus); setZN(dbr_); return 4;
    case 0x0B: push16(d_); return 4;
    case 0x2B: d_ = pull16(); if (emulation_) d_ = 0; setZN16(d_); return 5;
    case 0xF4: push16(static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8))); return 5;
    case 0xD4: push16(read16(bus, static_cast<std::uint16_t>(directBase() + fetch(bus)))); return 6;
    case 0x62: push16(static_cast<std::uint16_t>(pc_ + static_cast<std::int16_t>(fetch(bus) | (fetch(bus) << 8)))); return 6;
    case 0x6C: pc_ = read16(bus, static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8))); return 5;
    case 0x7C: {
        const std::uint16_t pointer = static_cast<std::uint16_t>((fetch(bus) | (fetch(bus) << 8)) + indexValue());
        pc_ = static_cast<std::uint16_t>(bus.read(pbr_, pointer) | (bus.read(pbr_, static_cast<std::uint16_t>(pointer + 1)) << 8));
        return 6;
    }
    case 0x5C: {
        const std::uint16_t target = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
        pbr_ = fetch(bus);
        pc_ = target;
        return 4;
    }
    case 0xDC: {
        const std::uint16_t pointer = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
        pc_ = read16(bus, pointer);
        pbr_ = read(bus, static_cast<std::uint16_t>(pointer + 2));
        return 6;
    }
    case 0x20: {
        const std::uint16_t target = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
        push16(static_cast<std::uint16_t>(pc_ - 1));
        pc_ = target;
        return 6;
    }
    case 0xFC: {
        const std::uint16_t pointer = static_cast<std::uint16_t>((fetch(bus) | (fetch(bus) << 8)) + indexValue());
        const std::uint16_t target = static_cast<std::uint16_t>(bus.read(pbr_, pointer) | (bus.read(pbr_, static_cast<std::uint16_t>(pointer + 1)) << 8));
        push16(static_cast<std::uint16_t>(pc_ - 1));
        pc_ = target;
        return 8;
    }
    case 0x22: {
        const std::uint16_t target = static_cast<std::uint16_t>(fetch(bus) | (fetch(bus) << 8));
        const std::uint8_t targetBank = fetch(bus);
        push(bus, pbr_);
        push16(static_cast<std::uint16_t>(pc_ - 1));
        pbr_ = targetBank;
        pc_ = target;
        return 8;
    }
    case 0x60: pc_ = static_cast<std::uint16_t>(pull16() + 1); return 6;
    case 0x6B: pc_ = static_cast<std::uint16_t>(pull16() + 1); pbr_ = pull(bus); return 6;
    case 0x40: {
        std::uint8_t flags = pull(bus);
        if (emulation_) flags = static_cast<std::uint8_t>((flags & 0xCF) | 0x30);
        p_ = flags;
        pc_ = pull16();
        if (!emulation_) pbr_ = pull(bus);
        applyWidths();
        return 6;
    }
    case 0x54:
    case 0x44: {
        const std::uint8_t destination = fetch(bus);
        const std::uint8_t source = fetch(bus);
        const bool decrement = opcode == 0x44;
        const int moves = (accumulator8() ? (a_ & 0xFF) : a_) + 1;
        for (int count = 0; count < moves; ++count) {
            bus.write(destination, y_, bus.read(source, x_));
            const std::uint16_t step = decrement ? 0xFFFF : 1;
            x_ = static_cast<std::uint16_t>(x_ + step);
            y_ = static_cast<std::uint16_t>(y_ + step);
        }
        a_ = 0xFFFF;
        dbr_ = destination;
        return 7 + moves * 7;
    }
    default:
        stop(opcode);
        return 0;
    }
}

void Cpu::saveState(iris::ByteWriter& out) const {
    out.u16(a_);
    out.u16(x_);
    out.u16(y_);
    out.u16(sp_);
    out.u16(d_);
    out.u16(pc_);
    out.u8(p_);
    out.u8(pbr_);
    out.u8(dbr_);
    out.u8(stopOpcode_);
    out.boolean(emulation_);
    out.boolean(nmiPending_);
    out.boolean(irqPending_);
    out.boolean(waiting_);
    out.boolean(stopped_);
}

bool Cpu::loadState(iris::ByteReader& in) {
    const auto a = in.u16();
    const auto x = in.u16();
    const auto y = in.u16();
    const auto sp = in.u16();
    const auto d = in.u16();
    const auto pc = in.u16();
    const auto p = in.u8();
    const auto pbr = in.u8();
    const auto dbr = in.u8();
    const auto stopOpcode = in.u8();
    const auto emulation = in.boolean();
    const auto nmiPending = in.boolean();
    const auto irqPending = in.boolean();
    const auto waiting = in.boolean();
    const auto stopped = in.boolean();
    if (!in.ok()) {
        return false;
    }
    a_ = a;
    x_ = x;
    y_ = y;
    sp_ = sp;
    d_ = d;
    pc_ = pc;
    p_ = p;
    pbr_ = pbr;
    dbr_ = dbr;
    stopOpcode_ = stopOpcode;
    emulation_ = emulation;
    nmiPending_ = nmiPending;
    irqPending_ = irqPending;
    waiting_ = waiting;
    spinning_ = false;
    stopped_ = stopped;
    return true;
}

}  // namespace iris::systems::snes
