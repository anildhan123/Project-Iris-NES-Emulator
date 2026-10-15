#include "iris/snes/gsu.hpp"

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
    color_ = 0;
    source_ = 0;
    dest_ = 0;
    alt1_ = false;
    useDest_ = false;
    useSource_ = false;
}

void Gsu::halt() {
    sfr_ = static_cast<std::uint16_t>(sfr_ & ~kGo);
    alt1_ = false;
    useDest_ = false;
    useSource_ = false;
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
    useDest_ = false;
    useSource_ = false;
    source_ = 0;
    dest_ = 0;
}

void Gsu::run(int budget) {
    while (budget > 0 && (sfr_ & kGo) != 0) {
        --budget;
        const std::uint8_t op = fetch();
        const std::uint8_t from = useSource_ ? source_ : 0;
        const std::uint8_t to = useDest_ ? dest_ : 0;
        if (op == 0x00) {
            sfr_ = static_cast<std::uint16_t>(sfr_ & ~kGo);
            finishPrefix();
            return;
        }
        if (op == 0x01 || op == 0x02) {
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
            alt1_ = false;
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
        if (op == 0x3E || op == 0x3F) {
            alt1_ = true;
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
            const auto address = (static_cast<std::uint32_t>(scbr_) << 10) + static_cast<std::uint32_t>(r_[2]) * 256u +
                                 (r_[1] & 0xFF);
            ramWrite(address, color_);
            r_[1] = static_cast<std::uint16_t>(r_[1] + 1);
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
        if ((op & 0xF0) == 0xB0) {
            source_ = static_cast<std::uint8_t>(op & 0x0F);
            useSource_ = true;
            alt1_ = false;
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
    if (address == 0x303C) {
        ramb_ = value;
        return true;
    }
    return true;
}

}  // namespace iris::systems::snes
