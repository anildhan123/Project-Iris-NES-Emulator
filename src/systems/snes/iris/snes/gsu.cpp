#include "iris/snes/gsu.hpp"

#include "iris/byte_io.hpp"

#include <algorithm>

namespace iris::systems::snes {
namespace {

constexpr std::uint16_t kZero = 0x0002;
constexpr std::uint16_t kCarry = 0x0004;
constexpr std::uint16_t kSign = 0x0008;
constexpr std::uint16_t kOverflow = 0x0010;
constexpr std::uint16_t kGo = 0x0020;

}  // namespace

void Gsu::power() {
    ram_.assign(128 * 1024, 0);
    for (std::uint16_t& reg : r_) {
        reg = 0;
    }
    sfr_ = 0;
    pbr_ = 0;
    rombr_ = 0;
    ramb_ = 0;
    scbr_ = 0;
    scmr_ = 0;
    color_ = 0;
    source_ = 0;
    dest_ = 0;
    alt1_ = false;
    alt2_ = false;
    useDest_ = false;
    useSource_ = false;
    useWith_ = false;
    cacheX_ = -1;
    cacheY_ = -1;
    cacheDirty_ = false;
    std::fill(std::begin(cachePixels_), std::end(cachePixels_), static_cast<std::uint8_t>(0));
    std::fill(std::begin(cacheValid_), std::end(cacheValid_), false);
}

void Gsu::halt() {
    flushPixelCache();
    sfr_ = static_cast<std::uint16_t>(sfr_ & ~kGo);
    alt1_ = false;
    alt2_ = false;
    useDest_ = false;
    useSource_ = false;
    useWith_ = false;
}

int Gsu::budgetForLine() const {
    // SCMR bit 3 selects the higher GSU clock. Budget is ops per scanline while GO is set.
    return (scmr_ & 0x08) != 0 ? 450 : 300;
}

void Gsu::bind(const std::vector<std::uint8_t>* rom) {
    rom_ = rom;
}

std::uint8_t Gsu::ramRead(std::uint32_t address) const {
    if (address >= ram_.size()) {
        return 0;
    }
    return ram_[address];
}

void Gsu::ramWrite(std::uint32_t address, std::uint8_t value) {
    if (address < ram_.size()) {
        ram_[address] = value;
    }
}

std::uint8_t Gsu::memory(std::uint8_t bank, std::uint16_t address) const {
    if (bank == 0x70 || bank == 0x71) {
        return ramRead((static_cast<std::uint32_t>(bank & 0x01) << 16) | address);
    }
    if (rom_ == nullptr || address < 0x8000) {
        return 0;
    }
    const std::size_t offset = (static_cast<std::size_t>(bank & 0x7F) << 15) | (address & 0x7FFF);
    if (offset >= rom_->size()) {
        return 0;
    }
    return (*rom_)[offset];
}

void Gsu::memoryWrite(std::uint8_t bank, std::uint16_t address, std::uint8_t value) {
    if (bank == 0x70 || bank == 0x71) {
        ramWrite((static_cast<std::uint32_t>(bank & 0x01) << 16) | address, value);
    }
}

std::uint8_t Gsu::fetch() {
    const std::uint8_t value = memory(pbr_, r_[15]);
    r_[15] = static_cast<std::uint16_t>(r_[15] + 1);
    return value;
}

void Gsu::setAddFlags(std::uint16_t left, std::uint16_t right, std::uint32_t sum) {
    const bool overflow = ((~(left ^ right) & (left ^ sum)) & 0x8000) != 0;
    sfr_ = static_cast<std::uint16_t>(sfr_ & ~(kZero | kCarry | kSign | kOverflow));
    if ((sum & 0xFFFF) == 0) {
        sfr_ = static_cast<std::uint16_t>(sfr_ | kZero);
    }
    if (sum > 0xFFFF) {
        sfr_ = static_cast<std::uint16_t>(sfr_ | kCarry);
    }
    if ((sum & 0x8000) != 0) {
        sfr_ = static_cast<std::uint16_t>(sfr_ | kSign);
    }
    if (overflow) {
        sfr_ = static_cast<std::uint16_t>(sfr_ | kOverflow);
    }
}

void Gsu::finishPrefix() {
    alt1_ = false;
    alt2_ = false;
    useDest_ = false;
    useSource_ = false;
    useWith_ = false;
    source_ = 0;
    dest_ = 0;
}

void Gsu::writeScreenPixel(int x, int y, std::uint8_t color) {
    const int depth = scmr_ & 0x03;
    if (depth == 0) {
        const auto address = (static_cast<std::uint32_t>(scbr_) << 10) + static_cast<std::uint32_t>(y) * 256u +
                             static_cast<std::uint32_t>(x);
        ramWrite(address, color);
    } else if (depth == 1) {
        const auto address = (static_cast<std::uint32_t>(scbr_) << 10) + static_cast<std::uint32_t>(y) * 64u +
                             static_cast<std::uint32_t>(x >> 2);
        const int shift = (3 - (x & 3)) * 2;
        const auto packed =
            static_cast<std::uint8_t>((ramRead(address) & ~(0x03 << shift)) | ((color & 0x03) << shift));
        ramWrite(address, packed);
    } else {
        const auto address = (static_cast<std::uint32_t>(scbr_) << 10) + static_cast<std::uint32_t>(y) * 128u +
                             static_cast<std::uint32_t>(x >> 1);
        const int shift = (1 - (x & 1)) * 4;
        const auto packed =
            static_cast<std::uint8_t>((ramRead(address) & ~(0x0F << shift)) | ((color & 0x0F) << shift));
        ramWrite(address, packed);
    }
}

void Gsu::flushPixelCache() {
    if (!cacheDirty_) {
        return;
    }
    for (int index = 0; index < 8; ++index) {
        if (cacheValid_[index]) {
            writeScreenPixel(cacheX_ + index, cacheY_, cachePixels_[index]);
        }
    }
    cacheDirty_ = false;
    std::fill(std::begin(cacheValid_), std::end(cacheValid_), false);
}

void Gsu::plotCached(int x, int y) {
    const int line = x & ~7;
    if (cacheDirty_ && (y != cacheY_ || line != cacheX_)) {
        flushPixelCache();
    }
    if (!cacheDirty_) {
        cacheX_ = line;
        cacheY_ = y;
        std::fill(std::begin(cacheValid_), std::end(cacheValid_), false);
        cacheDirty_ = true;
    }
    cachePixels_[x & 7] = color_;
    cacheValid_[x & 7] = true;
    if ((x & 7) == 7) {
        flushPixelCache();
    }
}

void Gsu::run(int budget) {
    while (budget > 0 && (sfr_ & kGo) != 0) {
        --budget;
        const std::uint8_t op = fetch();
        const std::uint8_t from = useSource_ ? source_ : 0;
        const std::uint8_t to = useDest_ ? dest_ : 0;
        if (op == 0x00) {
            flushPixelCache();
            sfr_ = static_cast<std::uint16_t>(sfr_ & ~kGo);
            finishPrefix();
            return;
        }
        if (op == 0x01 || op == 0x02) {
            // NOP / CACHE — cache flush is a no-op beyond the pixel pipeline.
            finishPrefix();
            continue;
        }
        if (op == 0x03) {
            // LSR
            const std::uint16_t value = r_[from];
            sfr_ = static_cast<std::uint16_t>((sfr_ & ~(kZero | kCarry | kSign)) | ((value & 1) != 0 ? kCarry : 0) |
                                              ((value >> 1) == 0 ? kZero : 0));
            r_[to] = static_cast<std::uint16_t>(value >> 1);
            finishPrefix();
            continue;
        }
        if (op == 0x04) {
            // ROL through carry
            const std::uint16_t value = r_[from];
            const std::uint16_t carryIn = (sfr_ & kCarry) != 0 ? 1 : 0;
            const auto rotated = static_cast<std::uint16_t>((value << 1) | carryIn);
            sfr_ = static_cast<std::uint16_t>((sfr_ & ~(kZero | kCarry | kSign)) | ((value & 0x8000) != 0 ? kCarry : 0) |
                                              (rotated == 0 ? kZero : 0) | ((rotated & 0x8000) != 0 ? kSign : 0));
            r_[to] = rotated;
            finishPrefix();
            continue;
        }
        if (op >= 0x05 && op <= 0x0F) {
            const auto offset = static_cast<std::int8_t>(fetch());
            const bool zero = (sfr_ & kZero) != 0;
            const bool carry = (sfr_ & kCarry) != 0;
            const bool sign = (sfr_ & kSign) != 0;
            const bool overflow = (sfr_ & kOverflow) != 0;
            bool taken = op == 0x05;
            if (op == 0x06) {
                taken = sign == overflow;
            } else if (op == 0x07) {
                taken = sign != overflow;
            } else if (op == 0x08) {
                taken = !zero;
            } else if (op == 0x09) {
                taken = zero;
            } else if (op == 0x0A) {
                taken = !sign;
            } else if (op == 0x0B) {
                taken = sign;
            } else if (op == 0x0C) {
                taken = !carry;
            } else if (op == 0x0D) {
                taken = carry;
            } else if (op == 0x0E) {
                taken = !overflow;
            } else if (op == 0x0F) {
                taken = overflow;
            }
            if (taken) {
                r_[15] = static_cast<std::uint16_t>(r_[15] + offset);
            }
            finishPrefix();
            continue;
        }
        if (op >= 0x10 && op <= 0x1F) {
            dest_ = static_cast<std::uint8_t>(op & 0x0F);
            useDest_ = true;
            continue;
        }
        if (op >= 0x20 && op <= 0x2F) {
            // WITH Rn: next op uses Rn as both source and dest.
            source_ = static_cast<std::uint8_t>(op & 0x0F);
            dest_ = source_;
            useSource_ = true;
            useDest_ = true;
            useWith_ = true;
            continue;
        }
        if (op == 0x3C) {
            const auto offset = static_cast<std::int8_t>(fetch());
            r_[12] = static_cast<std::uint16_t>(r_[12] - 1);
            if (r_[12] != 0) {
                r_[15] = static_cast<std::uint16_t>(r_[15] + offset);
            }
            finishPrefix();
            continue;
        }
        if (op == 0x3D) {
            alt1_ = true;
            continue;
        }
        if (op == 0x3E) {
            alt2_ = true;
            continue;
        }
        if (op == 0x3F) {
            alt1_ = true;
            alt2_ = true;
            continue;
        }
        if ((op & 0xF0) == 0x30 && (op & 0x0F) <= 11) {
            const auto address = static_cast<std::uint32_t>(ramb_ & 0x01) << 16 | r_[op & 0x0F];
            ramWrite(address, static_cast<std::uint8_t>(r_[from]));
            if (!alt1_) {
                ramWrite(address + 1, static_cast<std::uint8_t>(r_[from] >> 8));
            }
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0x40 && (op & 0x0F) <= 11) {
            const auto address = static_cast<std::uint32_t>(ramb_ & 0x01) << 16 | r_[op & 0x0F];
            std::uint16_t value = ramRead(address);
            if (!alt1_) {
                value = static_cast<std::uint16_t>(value | (ramRead(address + 1) << 8));
            }
            r_[to] = value;
            finishPrefix();
            continue;
        }
        if (op == 0x4C) {
            plotCached(r_[1] & 0xFF, r_[2] & 0xFF);
            r_[1] = static_cast<std::uint16_t>(r_[1] + 1);
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0xC0) {
            const std::uint16_t left = r_[to];
            const std::uint16_t right = r_[op & 0x0F];
            const std::uint16_t result =
                alt1_ ? static_cast<std::uint16_t>(left & right) : static_cast<std::uint16_t>(left | right);
            r_[to] = result;
            sfr_ = static_cast<std::uint16_t>((sfr_ & ~kZero) | (result == 0 ? kZero : 0));
            finishPrefix();
            continue;
        }
        if (op == 0x3B) {
            r_[to] = static_cast<std::uint16_t>(r_[from] >> 1);
            finishPrefix();
            continue;
        }
        if (op == 0x97) {
            r_[to] = static_cast<std::uint16_t>(static_cast<std::int16_t>(r_[from]) >> 1);
            finishPrefix();
            continue;
        }
        if (op == 0xEF) {
            // GETB: low byte from ROM at ROMBR:R14; GETC (ALT1) loads color.
            const std::uint8_t byte = memory(rombr_, r_[14]);
            if (alt1_) {
                color_ = byte;
            } else {
                r_[to] = static_cast<std::uint16_t>((r_[to] & 0xFF00) | byte);
            }
            finishPrefix();
            continue;
        }
        if (op >= 0xA0 && op <= 0xAF) {
            // IBT Rn,#imm
            r_[op & 0x0F] = fetch();
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0x80) {
            // MULT / UMULT (ALT1)
            const std::uint16_t left = r_[to];
            const std::uint16_t right = r_[op & 0x0F];
            const std::uint32_t product =
                alt1_ ? static_cast<std::uint32_t>(left) * right
                      : static_cast<std::uint32_t>(static_cast<std::int16_t>(left)) *
                            static_cast<std::uint32_t>(static_cast<std::int16_t>(right));
            r_[to] = static_cast<std::uint16_t>(product);
            sfr_ = static_cast<std::uint16_t>((sfr_ & ~kZero) | ((product & 0xFFFF) == 0 ? kZero : 0));
            finishPrefix();
            continue;
        }
        if (op == 0x95) {
            // SEX
            r_[to] = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(r_[from] & 0xFF)));
            finishPrefix();
            continue;
        }
        if (op == 0x96) {
            // ROR
            const std::uint16_t value = r_[from];
            const std::uint16_t carryIn = (sfr_ & kCarry) != 0 ? 0x8000 : 0;
            const auto rotated = static_cast<std::uint16_t>((value >> 1) | carryIn);
            sfr_ = static_cast<std::uint16_t>((sfr_ & ~(kZero | kCarry | kSign)) | ((value & 1) != 0 ? kCarry : 0) |
                                              (rotated == 0 ? kZero : 0) | ((rotated & 0x8000) != 0 ? kSign : 0));
            r_[to] = rotated;
            finishPrefix();
            continue;
        }
        if (op == 0x90) {
            // SBK: store back to last RAM address using R0 as data (RAM buffer assist).
            const auto address = static_cast<std::uint32_t>(ramb_ & 0x01) << 16 | r_[0];
            ramWrite(address, static_cast<std::uint8_t>(r_[from]));
            finishPrefix();
            continue;
        }
        if (op == 0x4D) {
            r_[to] = static_cast<std::uint16_t>((r_[from] << 8) | (r_[from] >> 8));
            finishPrefix();
            continue;
        }
        if (op == 0x4E) {
            color_ = static_cast<std::uint8_t>(r_[from]);
            finishPrefix();
            continue;
        }
        if (op == 0x4F) {
            r_[to] = static_cast<std::uint16_t>(~r_[from]);
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0x50) {
            const std::uint16_t left = r_[to];
            const std::uint16_t right = r_[op & 0x0F];
            const auto sum = static_cast<std::uint32_t>(left) + right;
            setAddFlags(left, right, sum);
            r_[to] = static_cast<std::uint16_t>(sum);
            finishPrefix();
            continue;
        }
        if (op == 0x9F) {
            const auto product = static_cast<std::int32_t>(static_cast<std::int16_t>(r_[from])) *
                                 static_cast<std::int32_t>(static_cast<std::int16_t>(r_[6]));
            r_[to] = static_cast<std::uint16_t>(product >> 16);
            finishPrefix();
            continue;
        }
        if (op == 0x9E) {
            const auto product = static_cast<std::int32_t>(static_cast<std::int16_t>(r_[from])) *
                                 static_cast<std::int32_t>(static_cast<std::int16_t>(r_[6]));
            r_[to] = static_cast<std::uint16_t>(product);
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0x60) {
            const std::uint16_t left = r_[to];
            const std::uint16_t right = r_[op & 0x0F];
            const std::uint16_t result = static_cast<std::uint16_t>(left ^ right);
            r_[to] = result;
            sfr_ = static_cast<std::uint16_t>((sfr_ & ~kZero) | (result == 0 ? kZero : 0));
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0x70) {
            const std::uint16_t left = r_[to];
            const std::uint16_t right = r_[op & 0x0F];
            const auto sum = static_cast<std::uint32_t>(left) + static_cast<std::uint16_t>(~right) + 1u;
            setAddFlags(left, static_cast<std::uint16_t>(~right), sum);
            r_[to] = static_cast<std::uint16_t>(sum);
            finishPrefix();
            continue;
        }
        if (op == 0xDF) {
            r_[to] = 0;
            sfr_ = static_cast<std::uint16_t>(sfr_ | kZero);
            finishPrefix();
            continue;
        }
        if (op == 0x4B) {
            const auto offset = fetch();
            r_[11] = r_[15];
            r_[15] = static_cast<std::uint16_t>(r_[15] + offset);
            finishPrefix();
            continue;
        }
        if (op == 0x9C) {
            r_[to] = static_cast<std::uint16_t>(static_cast<std::int16_t>(r_[from]) < 0 ? -static_cast<std::int16_t>(r_[from])
                                                                                       : static_cast<std::int16_t>(r_[from]));
            finishPrefix();
            continue;
        }
        if (op == 0x9D) {
            r_[to] = static_cast<std::uint16_t>(-static_cast<std::int16_t>(r_[from]));
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0xB0) {
            source_ = static_cast<std::uint8_t>(op & 0x0F);
            useSource_ = true;
            continue;
        }
        if (op >= 0xD0 && op <= 0xDE) {
            const int index = op & 0x0F;
            const std::uint16_t left = r_[index];
            const auto sum = static_cast<std::uint32_t>(left) + 1;
            setAddFlags(left, 1, sum);
            r_[index] = static_cast<std::uint16_t>(sum);
            finishPrefix();
            continue;
        }
        if (op >= 0xE0 && op <= 0xEE) {
            const int index = op & 0x0F;
            const std::uint16_t left = r_[index];
            const auto sum = static_cast<std::uint32_t>(left) + 0xFFFF;
            setAddFlags(left, 0xFFFF, sum);
            r_[index] = static_cast<std::uint16_t>(sum);
            finishPrefix();
            continue;
        }
        if ((op & 0xF0) == 0xF0) {
            const auto low = fetch();
            const auto high = fetch();
            r_[op & 0x0F] = static_cast<std::uint16_t>(low | (high << 8));
            finishPrefix();
            continue;
        }
        finishPrefix();
    }
}

bool Gsu::cpuRead(std::uint8_t bank, std::uint16_t address, std::uint8_t& value) const {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    if (mapped == 0x70 || mapped == 0x71) {
        value = ramRead((static_cast<std::uint32_t>(mapped & 0x01) << 16) | address);
        return true;
    }
    if (mapped >= 0x40) {
        return false;
    }
    if (address >= 0x6000 && address < 0x8000) {
        value = ramRead(static_cast<std::uint32_t>(address - 0x6000));
        return true;
    }
    if (address < 0x3000 || address > 0x32FF) {
        return false;
    }
    if (address < 0x3020) {
        const auto reg = r_[(address - 0x3000) >> 1];
        value = (address & 1) == 0 ? static_cast<std::uint8_t>(reg) : static_cast<std::uint8_t>(reg >> 8);
        return true;
    }
    if (address == 0x3030) {
        value = static_cast<std::uint8_t>(sfr_);
        return true;
    }
    if (address == 0x3031) {
        value = static_cast<std::uint8_t>(sfr_ >> 8);
        return true;
    }
    if (address == 0x3034) {
        value = pbr_;
        return true;
    }
    if (address == 0x3036) {
        value = rombr_;
        return true;
    }
    if (address == 0x3038) {
        value = scbr_;
        return true;
    }
    if (address == 0x303A) {
        value = scmr_;
        return true;
    }
    if (address == 0x303C) {
        value = ramb_;
        return true;
    }
    value = 0;
    return true;
}

bool Gsu::cpuWrite(std::uint8_t bank, std::uint16_t address, std::uint8_t value) {
    const std::uint8_t mapped = static_cast<std::uint8_t>(bank & 0x7F);
    if (mapped == 0x70 || mapped == 0x71) {
        ramWrite((static_cast<std::uint32_t>(mapped & 0x01) << 16) | address, value);
        return true;
    }
    if (mapped >= 0x40) {
        return false;
    }
    if (address >= 0x6000 && address < 0x8000) {
        ramWrite(static_cast<std::uint32_t>(address - 0x6000), value);
        return true;
    }
    if (address < 0x3000 || address > 0x32FF) {
        return false;
    }
    if (address < 0x3020) {
        const int index = (address - 0x3000) >> 1;
        if ((address & 1) == 0) {
            r_[index] = static_cast<std::uint16_t>((r_[index] & 0xFF00) | value);
        } else {
            r_[index] = static_cast<std::uint16_t>((r_[index] & 0x00FF) | (value << 8));
        }
        return true;
    }
    if (address == 0x3030) {
        sfr_ = static_cast<std::uint16_t>((sfr_ & 0xFF00) | value);
        if ((sfr_ & kGo) != 0) {
            run(64);
        }
        return true;
    }
    if (address == 0x3031) {
        sfr_ = static_cast<std::uint16_t>((sfr_ & 0x00FF) | (value << 8));
        return true;
    }
    if (address == 0x3034) {
        pbr_ = value;
        return true;
    }
    if (address == 0x3036) {
        rombr_ = value;
        return true;
    }
    if (address == 0x3038) {
        scbr_ = value;
        return true;
    }
    if (address == 0x303A) {
        scmr_ = value;
        return true;
    }
    if (address == 0x303C) {
        ramb_ = value;
        return true;
    }
    return true;
}

void Gsu::saveState(iris::ByteWriter& out) const {
    for (const std::uint16_t value : r_) {
        out.u16(value);
    }
    out.u16(sfr_);
    out.u8(pbr_);
    out.u8(rombr_);
    out.u8(ramb_);
    out.u8(scbr_);
    out.u8(scmr_);
    out.u8(color_);
    out.u8(source_);
    out.u8(dest_);
    out.boolean(alt1_);
    out.boolean(useDest_);
    out.boolean(useSource_);
    out.u32(static_cast<std::uint32_t>(ram_.size()));
    out.bytes(ram_.data(), ram_.size());
}

bool Gsu::loadState(iris::ByteReader& in) {
    std::uint16_t r[16] = {};
    for (std::uint16_t& value : r) {
        value = in.u16();
    }
    const auto sfr = in.u16();
    const auto pbr = in.u8();
    const auto rombr = in.u8();
    const auto ramb = in.u8();
    const auto scbr = in.u8();
    const auto scmr = in.u8();
    const auto color = in.u8();
    const auto source = in.u8();
    const auto dest = in.u8();
    const auto alt1 = in.boolean();
    const auto useDest = in.boolean();
    const auto useSource = in.boolean();
    const auto ramBytes = in.u32();
    if (!in.ok() || ramBytes != ram_.size() || ramBytes > 128 * 1024) {
        return false;
    }
    std::vector<std::uint8_t> ram(ramBytes);
    in.bytes(ram.data(), ram.size());
    if (!in.ok()) {
        return false;
    }
    for (int index = 0; index < 16; ++index) {
        r_[index] = r[index];
    }
    sfr_ = sfr;
    pbr_ = pbr;
    rombr_ = rombr;
    ramb_ = ramb;
    scbr_ = scbr;
    scmr_ = scmr;
    color_ = color;
    source_ = source;
    dest_ = dest;
    alt1_ = alt1;
    useDest_ = useDest;
    useSource_ = useSource;
    ram_ = std::move(ram);
    return true;
}

}  // namespace iris::systems::snes
