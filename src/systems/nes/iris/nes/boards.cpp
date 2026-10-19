#include "iris/nes/boards.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::nes {
namespace {

constexpr std::size_t k16K = 16 * 1024;
constexpr std::size_t k32K = 32 * 1024;
constexpr std::size_t k8K = 8 * 1024;

std::uint8_t readRam(Cartridge& cartridge, std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x6000 || cartridge.prgRam().empty()) {
        return openBus;
    }
    return cartridge.prgRam()[address - 0x6000];
}

void writeRam(Cartridge& cartridge, std::uint16_t address, std::uint8_t value) {
    if (address >= 0x6000 && address < 0x8000 && !cartridge.prgRam().empty()) {
        cartridge.prgRam()[address - 0x6000] = value;
    }
}

std::uint8_t readBank(const std::vector<std::uint8_t>& rom, int bank, std::size_t bankSize, std::uint16_t address) {
    if (rom.empty() || bankSize == 0) {
        return 0;
    }
    const int banks = static_cast<int>(rom.size() / bankSize);
    const int selected = banks == 0 ? 0 : bank % banks;
    const std::size_t offset = static_cast<std::size_t>(selected) * bankSize + (address & (bankSize - 1));
    return offset < rom.size() ? rom[offset] : 0;
}

std::uint8_t readChrBank(Cartridge& cartridge, int bank, std::size_t bankSize, std::uint16_t address) {
    return readBank(cartridge.chr(), bank, bankSize, address);
}

void writeChrRam(Cartridge& cartridge, std::uint16_t address, std::uint8_t value) {
    if (cartridge.chrRam() && !cartridge.chr().empty()) {
        cartridge.chr()[address % cartridge.chr().size()] = value;
    }
}

}  // namespace

std::uint8_t Axrom::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), bank_, k32K, address);
}

void Axrom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k32K);
    bank_ = banks == 0 ? 0 : (value & 0x07) % banks;
    upper_ = (value & 0x10) != 0;
}

std::uint8_t Axrom::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Axrom::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Axrom::mirroring() const { return upper_ ? Mirroring::SingleScreenUpper : Mirroring::SingleScreenLower; }

void Axrom::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(bank_));
    writer.boolean(upper_);
}

bool Axrom::applyState(ByteReader& reader) {
    const std::uint8_t bank = reader.u8();
    const bool upper = reader.boolean();
    const int banks = static_cast<int>(cartridge_.prg().size() / k32K);
    if (!reader.ok() || !reader.exhausted() || bank >= banks) {
        return false;
    }
    bank_ = bank;
    upper_ = upper;
    return true;
}

std::uint8_t ColorDreams::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), prg_, k32K, address);
}

void ColorDreams::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k32K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
    prg_ = prgBanks == 0 ? 0 : (value & 0x03) % prgBanks;
    chr_ = chrBanks == 0 ? 0 : ((value >> 4) & 0x0F) % chrBanks;
}

std::uint8_t ColorDreams::readChr(std::uint16_t address) { return readChrBank(cartridge_, chr_, k8K, address); }

void ColorDreams::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void ColorDreams::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chr_));
}

bool ColorDreams::applyState(ByteReader& reader) {
    const std::uint8_t prg = reader.u8();
    const std::uint8_t chr = reader.u8();
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k32K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
    if (!reader.ok() || !reader.exhausted() || prg >= prgBanks || chr >= chrBanks) {
        return false;
    }
    prg_ = prg;
    chr_ = chr;
    return true;
}

std::uint8_t Bnrom::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), bank_, k32K, address);
}

void Bnrom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k32K);
    bank_ = banks == 0 ? 0 : (value & 0x03) % banks;
}

std::uint8_t Bnrom::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Bnrom::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Bnrom::captureState(ByteWriter& writer) const { writer.u8(static_cast<std::uint8_t>(bank_)); }

bool Bnrom::applyState(ByteReader& reader) {
    const std::uint8_t bank = reader.u8();
    const int banks = static_cast<int>(cartridge_.prg().size() / k32K);
    if (!reader.ok() || !reader.exhausted() || bank >= banks) {
        return false;
    }
    bank_ = bank;
    return true;
}

std::uint8_t Gxrom::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), prg_, k32K, address);
}

void Gxrom::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k32K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
    prg_ = prgBanks == 0 ? 0 : ((value >> 4) & 0x03) % prgBanks;
    chr_ = chrBanks == 0 ? 0 : (value & 0x03) % chrBanks;
}

std::uint8_t Gxrom::readChr(std::uint16_t address) { return readChrBank(cartridge_, chr_, k8K, address); }

void Gxrom::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Gxrom::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chr_));
}

bool Gxrom::applyState(ByteReader& reader) {
    const std::uint8_t prg = reader.u8();
    const std::uint8_t chr = reader.u8();
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k32K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
    if (!reader.ok() || !reader.exhausted() || prg >= prgBanks || (chrBanks > 0 && chr >= chrBanks)) {
        return false;
    }
    prg_ = prg;
    chr_ = chr;
    return true;
}

std::uint8_t Camerica::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    const int bank = address < 0xC000 ? bank_ : (banks == 0 ? 0 : banks - 1);
    return readBank(cartridge_.prg(), bank, k16K, address);
}

void Camerica::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (cartridge_.submapper() == 1 && address < 0xC000) {
        upper_ = (value & 0x10) != 0;
        return;
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    bank_ = banks == 0 ? 0 : (value & 0x0F) % banks;
}

std::uint8_t Camerica::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Camerica::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Camerica::mirroring() const {
    if (cartridge_.submapper() == 1) {
        return upper_ ? Mirroring::SingleScreenUpper : Mirroring::SingleScreenLower;
    }
    return cartridge_.mirroring();
}

void Camerica::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(bank_));
    writer.boolean(upper_);
}

bool Camerica::applyState(ByteReader& reader) {
    const std::uint8_t bank = reader.u8();
    const bool upper = reader.boolean();
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    if (!reader.ok() || !reader.exhausted() || bank >= banks) {
        return false;
    }
    bank_ = bank;
    upper_ = upper;
    return true;
}

Mmc2::Mmc2(Cartridge& cartridge) : cartridge_(cartridge) {
    chrLow_[0] = 0;
    chrLow_[1] = 0;
    chrHigh_[0] = 0;
    chrHigh_[1] = 0;
}

std::uint8_t Mmc2::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    if (banks == 0) {
        return openBus;
    }
    int bank = 0;
    if (address < 0xA000) {
        bank = prg_;
    } else if (address < 0xC000) {
        bank = banks - 3;
    } else if (address < 0xE000) {
        bank = banks - 2;
    } else {
        bank = banks - 1;
    }
    if (bank < 0) {
        bank = 0;
    }
    return readBank(cartridge_.prg(), bank % banks, k8K, address);
}

void Mmc2::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k8K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / (4 * 1024));
    if (address >= 0xA000 && address <= 0xAFFF) {
        prg_ = prgBanks == 0 ? 0 : (value & 0x0F) % prgBanks;
    } else if (address >= 0xB000 && address <= 0xBFFF) {
        chrLow_[0] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xC000 && address <= 0xCFFF) {
        chrLow_[1] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xD000 && address <= 0xDFFF) {
        chrHigh_[0] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xE000 && address <= 0xEFFF) {
        chrHigh_[1] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xF000) {
        vertical_ = (value & 0x01) == 0;
    }
}

std::uint8_t Mmc2::readChr(std::uint16_t address) {
    const int bank = address < 0x1000 ? chrLow_[latchLow_] : chrHigh_[latchHigh_];
    const std::uint8_t value = readChrBank(cartridge_, bank, 4 * 1024, address);
    if (address >= 0x0FD8 && address <= 0x0FDF) {
        latchLow_ = 0;
    } else if (address >= 0x0FE8 && address <= 0x0FEF) {
        latchLow_ = 1;
    } else if (address >= 0x1FD8 && address <= 0x1FDF) {
        latchHigh_ = 0;
    } else if (address >= 0x1FE8 && address <= 0x1FEF) {
        latchHigh_ = 1;
    }
    return value;
}

void Mmc2::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Mmc2::mirroring() const { return vertical_ ? Mirroring::Vertical : Mirroring::Horizontal; }

void Mmc2::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chrLow_[0]));
    writer.u8(static_cast<std::uint8_t>(chrLow_[1]));
    writer.u8(static_cast<std::uint8_t>(chrHigh_[0]));
    writer.u8(static_cast<std::uint8_t>(chrHigh_[1]));
    writer.u8(static_cast<std::uint8_t>(latchLow_));
    writer.u8(static_cast<std::uint8_t>(latchHigh_));
    writer.boolean(vertical_);
}

bool Mmc2::applyState(ByteReader& reader) {
    const std::uint8_t prg = reader.u8();
    const std::uint8_t low0 = reader.u8();
    const std::uint8_t low1 = reader.u8();
    const std::uint8_t high0 = reader.u8();
    const std::uint8_t high1 = reader.u8();
    const std::uint8_t latchLow = reader.u8();
    const std::uint8_t latchHigh = reader.u8();
    const bool vertical = reader.boolean();
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k8K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / (4 * 1024));
    if (!reader.ok() || !reader.exhausted() || prg >= prgBanks || low0 >= chrBanks || low1 >= chrBanks ||
        high0 >= chrBanks || high1 >= chrBanks || latchLow > 1 || latchHigh > 1) {
        return false;
    }
    prg_ = prg;
    chrLow_[0] = low0;
    chrLow_[1] = low1;
    chrHigh_[0] = high0;
    chrHigh_[1] = high1;
    latchLow_ = latchLow;
    latchHigh_ = latchHigh;
    vertical_ = vertical;
    return true;
}

std::uint8_t Nina003::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), prg_, k32K, address);
}

void Nina003::writeCpu(std::uint16_t address, std::uint8_t value) {
    if ((address & 0xE100) == 0x4100) {
        const int prgBanks = static_cast<int>(cartridge_.prg().size() / k32K);
        const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
        prg_ = prgBanks == 0 ? 0 : ((value >> 3) & 0x01) % prgBanks;
        chr_ = chrBanks == 0 ? 0 : (value & 0x07) % chrBanks;
        return;
    }
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
    }
}

std::uint8_t Nina003::readChr(std::uint16_t address) { return readChrBank(cartridge_, chr_, k8K, address); }

void Nina003::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Nina003::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chr_));
}

bool Nina003::applyState(ByteReader& reader) {
    const std::uint8_t prg = reader.u8();
    const std::uint8_t chr = reader.u8();
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k32K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
    if (!reader.ok() || !reader.exhausted() || prg >= prgBanks || chr >= chrBanks) {
        return false;
    }
    prg_ = prg;
    chr_ = chr;
    return true;
}

Mmc4::Mmc4(Cartridge& cartridge) : cartridge_(cartridge) {
    chrLow_[0] = 0;
    chrLow_[1] = 0;
    chrHigh_[0] = 0;
    chrHigh_[1] = 0;
}

std::uint8_t Mmc4::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    if (banks == 0) {
        return openBus;
    }
    const int bank = address < 0xC000 ? prg_ : banks - 1;
    return readBank(cartridge_.prg(), bank % banks, k16K, address);
}

void Mmc4::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k16K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / (4 * 1024));
    if (address >= 0xA000 && address <= 0xAFFF) {
        prg_ = prgBanks == 0 ? 0 : (value & 0x0F) % prgBanks;
    } else if (address >= 0xB000 && address <= 0xBFFF) {
        chrLow_[0] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xC000 && address <= 0xCFFF) {
        chrLow_[1] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xD000 && address <= 0xDFFF) {
        chrHigh_[0] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xE000 && address <= 0xEFFF) {
        chrHigh_[1] = chrBanks == 0 ? 0 : (value & 0x1F) % chrBanks;
    } else if (address >= 0xF000) {
        vertical_ = (value & 0x01) == 0;
    }
}

std::uint8_t Mmc4::readChr(std::uint16_t address) {
    const int bank = address < 0x1000 ? chrLow_[latchLow_] : chrHigh_[latchHigh_];
    const std::uint8_t value = readChrBank(cartridge_, bank, 4 * 1024, address);
    if (address >= 0x0FD8 && address <= 0x0FDF) {
        latchLow_ = 0;
    } else if (address >= 0x0FE8 && address <= 0x0FEF) {
        latchLow_ = 1;
    } else if (address >= 0x1FD8 && address <= 0x1FDF) {
        latchHigh_ = 0;
    } else if (address >= 0x1FE8 && address <= 0x1FEF) {
        latchHigh_ = 1;
    }
    return value;
}

void Mmc4::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Mmc4::mirroring() const { return vertical_ ? Mirroring::Vertical : Mirroring::Horizontal; }

void Mmc4::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_));
    writer.u8(static_cast<std::uint8_t>(chrLow_[0]));
    writer.u8(static_cast<std::uint8_t>(chrLow_[1]));
    writer.u8(static_cast<std::uint8_t>(chrHigh_[0]));
    writer.u8(static_cast<std::uint8_t>(chrHigh_[1]));
    writer.u8(static_cast<std::uint8_t>(latchLow_));
    writer.u8(static_cast<std::uint8_t>(latchHigh_));
    writer.boolean(vertical_);
}

bool Mmc4::applyState(ByteReader& reader) {
    const std::uint8_t prg = reader.u8();
    const std::uint8_t low0 = reader.u8();
    const std::uint8_t low1 = reader.u8();
    const std::uint8_t high0 = reader.u8();
    const std::uint8_t high1 = reader.u8();
    const std::uint8_t latchLow = reader.u8();
    const std::uint8_t latchHigh = reader.u8();
    const bool vertical = reader.boolean();
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k16K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / (4 * 1024));
    if (!reader.ok() || !reader.exhausted() || prg >= prgBanks || low0 >= chrBanks || low1 >= chrBanks ||
        high0 >= chrBanks || high1 >= chrBanks || latchLow > 1 || latchHigh > 1) {
        return false;
    }
    prg_ = prg;
    chrLow_[0] = low0;
    chrLow_[1] = low1;
    chrHigh_[0] = high0;
    chrHigh_[1] = high1;
    latchLow_ = latchLow;
    latchHigh_ = latchHigh;
    vertical_ = vertical;
    return true;
}

std::uint8_t Mapper87::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    return readBank(cartridge_.prg(), 0, cartridge_.prg().size() >= k32K ? k32K : k16K, address);
}

void Mapper87::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address >= 0x6000 && address < 0x8000) {
        const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
        chr_ = chrBanks == 0 ? 0 : (((value >> 1) & 0x01) | ((value << 1) & 0x02)) % chrBanks;
        return;
    }
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
    }
}

std::uint8_t Mapper87::readChr(std::uint16_t address) { return readChrBank(cartridge_, chr_, k8K, address); }

void Mapper87::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper87::captureState(ByteWriter& writer) const { writer.u8(static_cast<std::uint8_t>(chr_)); }

bool Mapper87::applyState(ByteReader& reader) {
    const std::uint8_t chr = reader.u8();
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / k8K);
    if (!reader.ok() || !reader.exhausted() || chr >= chrBanks) {
        return false;
    }
    chr_ = chr;
    return true;
}

Mapper206::Mapper206(Cartridge& cartridge) : cartridge_(cartridge) {
    banks_[0] = 0;
    banks_[1] = 2;
    banks_[2] = 4;
    banks_[3] = 5;
    banks_[4] = 6;
    banks_[5] = 7;
    banks_[6] = 0;
    banks_[7] = 1;
}

std::uint8_t Mapper206::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    if (banks == 0) {
        return openBus;
    }
    int bank = 0;
    if (address < 0xA000) {
        bank = banks_[6];
    } else if (address < 0xC000) {
        bank = banks_[7];
    } else if (address < 0xE000) {
        bank = banks - 2;
    } else {
        bank = banks - 1;
    }
    return readBank(cartridge_.prg(), bank % banks, k8K, address);
}

void Mapper206::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if ((address & 0xE001) == 0x8000) {
        select_ = value & 0x07;
        return;
    }
    if ((address & 0xE001) == 0x8001) {
        const int prgBanks = static_cast<int>(cartridge_.prg().size() / k8K);
        const int chrBanks = static_cast<int>(cartridge_.chr().size() / 1024);
        if (select_ <= 1) {
            banks_[select_] = static_cast<std::uint8_t>(chrBanks == 0 ? 0 : (value & 0x3E) % chrBanks);
        } else if (select_ <= 5) {
            banks_[select_] = static_cast<std::uint8_t>(chrBanks == 0 ? 0 : (value & 0x3F) % chrBanks);
        } else {
            banks_[select_] = static_cast<std::uint8_t>(prgBanks == 0 ? 0 : (value & 0x0F) % prgBanks);
        }
    }
}

std::uint8_t Mapper206::readChr(std::uint16_t address) {
    int bank = 0;
    if (address < 0x0800) {
        bank = banks_[0] + ((address >> 10) & 1);
    } else if (address < 0x1000) {
        bank = banks_[1] + ((address >> 10) & 1);
    } else if (address < 0x1400) {
        bank = banks_[2];
    } else if (address < 0x1800) {
        bank = banks_[3];
    } else if (address < 0x1C00) {
        bank = banks_[4];
    } else {
        bank = banks_[5];
    }
    return readChrBank(cartridge_, bank, 1024, address);
}

void Mapper206::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper206::captureState(ByteWriter& writer) const {
    for (const std::uint8_t bank : banks_) {
        writer.u8(bank);
    }
    writer.u8(select_);
}

bool Mapper206::applyState(ByteReader& reader) {
    std::uint8_t banks[8] = {};
    for (std::uint8_t& bank : banks) {
        bank = reader.u8();
    }
    const std::uint8_t select = reader.u8();
    if (!reader.ok() || !reader.exhausted() || select > 7) {
        return false;
    }
    for (int index = 0; index < 8; ++index) {
        banks_[index] = banks[index];
    }
    select_ = select;
    return true;
}

std::uint8_t Mapper232::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k16K);
    if (banks == 0) {
        return openBus;
    }
    const int bank = address < 0xC000 ? ((outer_ << 2) | inner_) : ((outer_ << 2) | 0x03);
    return readBank(cartridge_.prg(), bank % banks, k16K, address);
}

void Mapper232::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    if (address < 0xC000) {
        outer_ = (value >> 3) & 0x03;
    } else {
        inner_ = value & 0x03;
    }
}

std::uint8_t Mapper232::readChr(std::uint16_t address) { return readChrBank(cartridge_, 0, k8K, address); }

void Mapper232::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

void Mapper232::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(outer_));
    writer.u8(static_cast<std::uint8_t>(inner_));
}

bool Mapper232::applyState(ByteReader& reader) {
    const std::uint8_t outer = reader.u8();
    const std::uint8_t inner = reader.u8();
    if (!reader.ok() || !reader.exhausted() || outer > 3 || inner > 3) {
        return false;
    }
    outer_ = outer;
    inner_ = inner;
    return true;
}

std::uint8_t Vrc1::readCpu(std::uint16_t address, std::uint8_t openBus) {
    if (address < 0x8000) {
        return readRam(cartridge_, address, openBus);
    }
    const int banks = static_cast<int>(cartridge_.prg().size() / k8K);
    if (banks == 0) {
        return openBus;
    }
    int bank = 0;
    if (address < 0xA000) {
        bank = prg_[0];
    } else if (address < 0xC000) {
        bank = prg_[1];
    } else if (address < 0xE000) {
        bank = prg_[2];
    } else {
        bank = banks - 1;
    }
    return readBank(cartridge_.prg(), bank % banks, k8K, address);
}

void Vrc1::writeCpu(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) {
        writeRam(cartridge_, address, value);
        return;
    }
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k8K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / (4 * 1024));
    if (address >= 0x8000 && address <= 0x8FFF) {
        prg_[0] = prgBanks == 0 ? 0 : (value & 0x0F) % prgBanks;
    } else if (address >= 0x9000 && address <= 0x9FFF) {
        vertical_ = (value & 0x01) == 0;
        chr_[0] = chrBanks == 0 ? 0 : ((chr_[0] & 0x0F) | ((value & 0x02) << 3)) % chrBanks;
        chr_[1] = chrBanks == 0 ? 0 : ((chr_[1] & 0x0F) | ((value & 0x04) << 2)) % chrBanks;
    } else if (address >= 0xA000 && address <= 0xAFFF) {
        prg_[1] = prgBanks == 0 ? 0 : (value & 0x0F) % prgBanks;
    } else if (address >= 0xC000 && address <= 0xCFFF) {
        prg_[2] = prgBanks == 0 ? 0 : (value & 0x0F) % prgBanks;
    } else if (address >= 0xE000 && address <= 0xEFFF) {
        chr_[0] = chrBanks == 0 ? 0 : ((chr_[0] & 0x10) | (value & 0x0F)) % chrBanks;
    } else if (address >= 0xF000) {
        chr_[1] = chrBanks == 0 ? 0 : ((chr_[1] & 0x10) | (value & 0x0F)) % chrBanks;
    }
}

std::uint8_t Vrc1::readChr(std::uint16_t address) {
    return readChrBank(cartridge_, address < 0x1000 ? chr_[0] : chr_[1], 4 * 1024, address);
}

void Vrc1::writeChr(std::uint16_t address, std::uint8_t value) { writeChrRam(cartridge_, address, value); }

Mirroring Vrc1::mirroring() const { return vertical_ ? Mirroring::Vertical : Mirroring::Horizontal; }

void Vrc1::captureState(ByteWriter& writer) const {
    writer.u8(static_cast<std::uint8_t>(prg_[0]));
    writer.u8(static_cast<std::uint8_t>(prg_[1]));
    writer.u8(static_cast<std::uint8_t>(prg_[2]));
    writer.u8(static_cast<std::uint8_t>(chr_[0]));
    writer.u8(static_cast<std::uint8_t>(chr_[1]));
    writer.boolean(vertical_);
}

bool Vrc1::applyState(ByteReader& reader) {
    const std::uint8_t prg0 = reader.u8();
    const std::uint8_t prg1 = reader.u8();
    const std::uint8_t prg2 = reader.u8();
    const std::uint8_t chr0 = reader.u8();
    const std::uint8_t chr1 = reader.u8();
    const bool vertical = reader.boolean();
    const int prgBanks = static_cast<int>(cartridge_.prg().size() / k8K);
    const int chrBanks = static_cast<int>(cartridge_.chr().size() / (4 * 1024));
    if (!reader.ok() || !reader.exhausted() || prg0 >= prgBanks || prg1 >= prgBanks || prg2 >= prgBanks ||
        chr0 >= chrBanks || chr1 >= chrBanks) {
        return false;
    }
    prg_[0] = prg0;
    prg_[1] = prg1;
    prg_[2] = prg2;
    chr_[0] = chr0;
    chr_[1] = chr1;
    vertical_ = vertical;
    return true;
}

}  // namespace iris::systems::nes
