#pragma once

#include "iris/snes/apu.hpp"
#include "iris/snes/cartridge.hpp"
#include "iris/snes/dsp1.hpp"
#include "iris/snes/gsu.hpp"
#include "iris/snes/ppu.hpp"

#include <cstdint>
#include <vector>

namespace iris::systems::snes {

class Bus {
public:
    void power();
    void map(std::vector<std::uint8_t> rom, bool hiRom, bool exHiRom, std::size_t sramBytes, Chip chip);
    void setController(std::uint8_t buttons);
    void setCycle(int cycle);
    void raiseNmi();
    void raiseIrq();
    void present();
    void resetApu();
    void resetChips();
    void stepGsu(int budget);
    [[nodiscard]] bool takeSa1Start();
    [[nodiscard]] bool sa1Running() const { return sa1Running_; }
    [[nodiscard]] std::uint16_t sa1ResetVector() const { return crv_; }
    void setSa1Access(bool enabled) { sa1Access_ = enabled; }
    void beginFrame();
    void catchUpApu(int masterCycle, int masterClock);
    void finishAudio(int masterCycles, int masterClock);
    [[nodiscard]] const float* audioSamples() const { return apu_.samples(); }
    [[nodiscard]] int audioCount() const { return apu_.sampleCount(); }
    [[nodiscard]] bool virqEnabled() const { return (nmitimen_ & 0x20) != 0; }
    [[nodiscard]] bool hirqEnabled() const { return (nmitimen_ & 0x10) != 0; }
    [[nodiscard]] int irqLine() const { return vtime_; }
    [[nodiscard]] int irqDot() const { return htime_; }
    [[nodiscard]] int takeDmaStall();
    [[nodiscard]] int beginHdma();
    [[nodiscard]] int transferHdma();
    void drawLine(int line);
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
    [[nodiscard]] bool readSa1(std::uint8_t bank, std::uint16_t address, std::uint8_t& value);
    [[nodiscard]] bool writeSa1(std::uint8_t bank, std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t readSa1Reg(std::uint16_t address) const;
    void writeSa1Reg(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] bool sramIndex(std::uint8_t bank, std::uint16_t address, std::size_t& index) const;
    void writeDma(std::uint16_t address, std::uint8_t value);
    void startDma(std::uint8_t mask);
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
    Apu apu_;
    Dsp1 dsp_;
    Gsu gsu_;
    std::vector<std::uint8_t> iram_;
    std::vector<std::uint8_t> bwram_;
    Chip chip_ = Chip::None;
    std::uint8_t ccnt_ = 0;
    std::uint16_t crv_ = 0;
    bool sa1Running_ = false;
    bool sa1Start_ = false;
    bool sa1Access_ = false;
    std::uint8_t mulA_ = 0;
    std::uint16_t dividend_ = 0;
    std::uint16_t quotient_ = 0;
    std::uint16_t product_ = 0;
    std::uint16_t vtime_ = 0;
    std::uint16_t htime_ = 0;
    int dmaStall_ = 0;
    int cycle_ = 0;
    bool nmiFlag_ = false;
    bool irqFlag_ = false;
    bool hiRom_ = false;
    bool exHiRom_ = false;
};

}  // namespace iris::systems::snes
