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

}  // namespace iris::systems::nes
