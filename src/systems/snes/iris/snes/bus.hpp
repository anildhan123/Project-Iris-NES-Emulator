#pragma once

#include "iris/snes/ppu.hpp"

#include <cstdint>
#include <vector>

namespace iris::systems::snes {

class Bus {
public:
    void power();
    void map(std::vector<std::uint8_t> rom, bool hiRom, std::size_t sramBytes);
    void setController(std::uint8_t buttons);
    void setCycle(int cycle);
    void raiseNmi();
    void raiseIrq();
    void present();
    [[nodiscard]] bool virqEnabled() const { return (nmitimen_ & 0x20) != 0; }
    [[nodiscard]] int irqLine() const { return vtime_; }
    void copySram(std::vector<std::uint8_t>& bytes) const;
    [[nodiscard]] bool restoreSram(const std::uint8_t* data, std::size_t size);

    [[nodiscard]] std::uint8_t read(std::uint8_t bank, std::uint16_t address);
    void write(std::uint8_t bank, std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t read24(std::uint32_t address);
    [[nodiscard]] Ppu& ppu() { return ppu_; }
    [[nodiscard]] const Ppu& ppu() const { return ppu_; }
    [[nodiscard]] const std::vector<std::uint8_t>& rom() const { return rom_; }
    [[nodiscard]] bool nmiEnabled() const { return (nmitimen_ & 0x80) != 0; }

private:
    struct DmaChannel {
        std::uint8_t control = 0;
        std::uint8_t destination = 0;
        std::uint32_t source = 0;
        std::uint16_t count = 0;
        std::uint8_t indirectBank = 0;
        std::uint16_t table = 0;
        std::uint16_t indirect = 0;
        std::uint8_t lineCounter = 0;
        bool hdmaActive = false;
        bool hdmaTransfer = false;
    };

    [[nodiscard]] std::uint8_t romByte(std::uint8_t bank, std::uint16_t address) const;
    [[nodiscard]] bool sramIndex(std::uint8_t bank, std::uint16_t address, std::size_t& index) const;
    void writeDma(std::uint16_t address, std::uint8_t value);
    void startDma(std::uint8_t mask);
    void beginHdma();
    void hdmaScanline();
    void hdmaLoad(DmaChannel& channel);
    void hdmaTransfer(DmaChannel& channel);
    void writeWramPort(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t readWramPort(std::uint16_t address);

    std::vector<std::uint8_t> rom_;
    std::vector<std::uint8_t> wram_;
    std::vector<std::uint8_t> sram_;
    Ppu ppu_;
    DmaChannel dma_[8]{};
    std::uint32_t wmadd_ = 0;
    std::uint8_t joyLow_ = 0;
    std::uint8_t joyHigh_ = 0;
    std::uint8_t nmitimen_ = 0;
    std::uint8_t hdmaen_ = 0;
    std::uint8_t apu_[4] = {0xAA, 0xBB, 0x00, 0x00};
    std::uint8_t mulA_ = 0;
    std::uint16_t dividend_ = 0;
    std::uint16_t quotient_ = 0;
    std::uint16_t product_ = 0;
    std::uint16_t vtime_ = 0;
    int cycle_ = 0;
    bool nmiFlag_ = false;
    bool irqFlag_ = false;
    bool hiRom_ = false;
};

}  // namespace iris::systems::snes
