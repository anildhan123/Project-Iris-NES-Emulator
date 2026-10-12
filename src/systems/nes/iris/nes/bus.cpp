#include "iris/nes/bus.hpp"

#include "iris/byte_io.hpp"

#include <algorithm>

namespace iris::systems::nes {

Bus::Bus(Ppu& ppu, Apu& apu, Controller& port1, Controller& port2)
    : ppu_(ppu), apu_(apu), port1_(port1), port2_(port2) {}

void Bus::connect(Mapper* mapper) { mapper_ = mapper; }

void Bus::power() {
    std::fill(std::begin(ram_), std::end(ram_), static_cast<std::uint8_t>(0));
    openBus_ = 0;
    dmaCycles_ = 0;
}

void Bus::reset() {
    openBus_ = 0;
    dmaCycles_ = 0;
}

std::uint8_t Bus::read(std::uint16_t address) {
    std::uint8_t value = openBus_;
    if (address < 0x2000) {
        value = ram_[address & 0x07FF];
    } else if (address < 0x4000) {
        value = ppu_.readRegister(address);
    } else if (address == 0x4015) {
        const std::uint8_t status = apu_.readStatus();
        value = static_cast<std::uint8_t>((openBus_ & 0x20) | (status & 0xDF));
    } else if (address == 0x4016) {
        value = static_cast<std::uint8_t>((openBus_ & 0xE0) | port1_.read());
    } else if (address == 0x4017) {
        value = static_cast<std::uint8_t>((openBus_ & 0xE0) | port2_.read());
    } else if (address >= 0x4020 && mapper_ != nullptr) {
        value = mapper_->readCpu(address, openBus_);
    }
    openBus_ = value;
    return value;
}

void Bus::write(std::uint16_t address, std::uint8_t value) {
    openBus_ = value;
    if (address < 0x2000) {
        ram_[address & 0x07FF] = value;
    } else if (address < 0x4000) {
        ppu_.writeRegister(address, value);
    } else if (address <= 0x4013 || address == 0x4015 || address == 0x4017) {
        apu_.write(address, value);
    } else if (address == 0x4014) {
        const std::uint16_t page = static_cast<std::uint16_t>(value << 8);
        for (int index = 0; index < 256; ++index) {
            ppu_.writeOam(read(static_cast<std::uint16_t>(page + index)));
        }
        dmaCycles_ = 513;
    } else if (address == 0x4016) {
        port1_.writeStrobe(value);
        port2_.writeStrobe(value);
    } else if (address >= 0x4020 && mapper_ != nullptr) {
        mapper_->writeCpu(address, value);
    }
}

int Bus::takeDmaCycles(std::uint64_t cpuCycle) {
    if (dmaCycles_ == 0) {
        return 0;
    }
    const int cycles = dmaCycles_ + ((cpuCycle & 1U) != 0U ? 1 : 0);
    dmaCycles_ = 0;
    return cycles;
}

void Bus::captureState(ByteWriter& writer) const {
    writer.bytes(ram_, sizeof(ram_));
    writer.u8(openBus_);
    writer.u16(static_cast<std::uint16_t>(dmaCycles_));
}

bool Bus::applyState(ByteReader& reader) {
    std::uint8_t ram[2048] = {};
    reader.bytes(ram, sizeof(ram));
    const std::uint8_t openBus = reader.u8();
    const std::uint16_t dmaCycles = reader.u16();
    if (!reader.ok() || !reader.exhausted() || dmaCycles > 514) {
        return false;
    }
    std::copy(std::begin(ram), std::end(ram), std::begin(ram_));
    openBus_ = openBus;
    dmaCycles_ = dmaCycles;
    return true;
}

}  // namespace iris::systems::nes
