#pragma once

#include "iris/snes/apu.hpp"
#include "iris/snes/cartridge.hpp"
#include "iris/snes/dsp1.hpp"
#include "iris/snes/expansion.hpp"
#include "iris/snes/gsu.hpp"
#include "iris/snes/ppu.hpp"

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

class Bus {
public:
    void power();
    void map(std::vector<std::uint8_t> rom, bool hiRom, bool exHiRom, std::size_t sramBytes, Chip chip,
             bool fastRom = false);
    enum class InputDevice : std::uint8_t { Pad = 0, Multitap = 1, Mouse = 2, SuperScope = 3, Justifier = 4 };

    void setController(int port, std::uint8_t buttons);
    void setInputDevice(int port, InputDevice device);
    void setPointer(int x, int y);
    // cycle is master clocks from frame start; lineDot is clocks into the current
    // scanline; scanline is the current line index (for $4212 without % 1364 drift).
    void setCycle(int cycle, int lineDot = 0, int scanline = 0);
    // Charge master clocks for one memory access so mid-instruction timing reaches the APU and IRQs.
    // pageCross adds the half-scale penalty used for unaligned 16-bit FastROM/SlowROM reads.
    void tickAccess(std::uint8_t bank, std::uint16_t address, bool pageCross = false);
    void setMasterClock(int masterClock) { masterClock_ = masterClock; }
    [[nodiscard]] int cycle() const { return cycle_; }
    void raiseNmi();
    void raiseIrq();
    void present();
    void resetApu();
    void resetChips();
    void stepGsu(int budget);
    [[nodiscard]] int gsuBudgetForLine() const;
    [[nodiscard]] bool takeSa1Start();
    [[nodiscard]] bool sa1Running() const { return sa1Running_; }
    [[nodiscard]] std::uint16_t sa1ResetVector() const { return crv_; }
    void setSa1Access(bool enabled) { sa1Access_ = enabled; }
    void beginFrame();
    void catchUpApu(int masterCycle, int masterClock);
    void finishAudio(int masterCycles, int masterClock);
    void pollApuBoot() { apu_.pollBoot(); }
    // Clear the mid-instruction APU I/O hold so catch-up may run again.
    void endApuIoInstruction() { apuIoHold_ = false; }
    [[nodiscard]] const float* audioSamples() const { return apu_.samples(); }
    [[nodiscard]] int audioCount() const { return apu_.sampleCount(); }
    [[nodiscard]] int audioChannels() const { return apu_.channels(); }
    [[nodiscard]] bool installIpl(const std::uint8_t* data, std::size_t size) { return apu_.installIpl(data, size); }
    [[nodiscard]] bool hasIpl() const { return apu_.hasIpl(); }
    [[nodiscard]] bool virqEnabled() const { return (nmitimen_ & 0x20) != 0; }
    [[nodiscard]] bool hirqEnabled() const { return (nmitimen_ & 0x10) != 0; }
    [[nodiscard]] int irqLine() const { return vtime_; }
    [[nodiscard]] int irqDot() const { return htime_; }
    // VBlank starts at line 225, or 240 when SETINI selects overscan.
    [[nodiscard]] int vblankLine() const { return ppu_.overscan() ? 240 : 225; }
    [[nodiscard]] int takeDmaStall();
    [[nodiscard]] int beginHdma();
    [[nodiscard]] int transferHdma();
    void drawLine(int line);
    void copySram(std::vector<std::uint8_t>& bytes) const;
    [[nodiscard]] bool restoreSram(const std::uint8_t* data, std::size_t size);
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

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
    struct Pad {
        std::uint8_t low = 0;
        std::uint8_t high = 0;
        std::uint16_t shift = 0;
        int bitsLeft = 0;
    };

    void encodePad(Pad& pad, std::uint8_t buttons) const;
    void latchJoypad(int port);
    void commitAutoJoy();
    void pollAutoJoy();
    [[nodiscard]] std::uint8_t readJoypad(bool portTwo);
    [[nodiscard]] bool autoJoyBusy() const;

    Pad pad_[4]{};
    InputDevice device_[2] = {InputDevice::Pad, InputDevice::Pad};
    int pointerX_ = 0;
    int pointerY_ = 0;
    // Latched auto-read results for JOY1–JOY4. Multitap fills JOY3/JOY4 from pads 2–3.
    std::uint8_t joy1Low_ = 0;
    std::uint8_t joy1High_ = 0;
    std::uint8_t joy2Low_ = 0;
    std::uint8_t joy2High_ = 0;
    std::uint8_t joy3Low_ = 0;
    std::uint8_t joy3High_ = 0;
    std::uint8_t joy4Low_ = 0;
    std::uint8_t joy4High_ = 0;
    bool joyStrobe_ = false;
    bool autoJoyPending_ = false;
    // Auto-read is busy for a few scanlines after vblank starts. Games wait for this bit to clear
    // before trusting $4218. Leaving it clear makes that wait fall through and some games skip the read.
    int joyBusyUntil_ = 0;
    std::uint8_t nmitimen_ = 0;
    std::uint8_t hdmaen_ = 0;
    Apu apu_;
    Dsp1 dsp_;
    Gsu gsu_;
    Expansion expansion_;
    std::vector<std::uint8_t> iram_;
    std::vector<std::uint8_t> bwram_;
    Chip chip_ = Chip::None;
    std::uint8_t ccnt_ = 0;
    std::uint16_t crv_ = 0;
    std::uint8_t sa1DmaControl_ = 0;
    std::uint32_t sa1DmaSource_ = 0;
    std::uint32_t sa1DmaDest_ = 0;
    std::uint16_t sa1DmaCount_ = 0;
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
    int lineDot_ = 0;
    int scanline_ = 0;
    // Once an instruction touches $2140–$217F, defer SPC catch-up until it ends
    // so multi-port and 16-bit command writes commit together (Akao-style traffic).
    bool apuIoHold_ = false;
    bool nmiFlag_ = false;
    bool irqFlag_ = false;
    bool hiRom_ = false;
    bool exHiRom_ = false;
    bool fastRom_ = false;
    int masterClock_ = 21477272;
    std::uint32_t sa1BitData_ = 0;
    int sa1BitCount_ = 0;
    std::uint8_t sa1BitmapReg_ = 0;
    std::uint8_t sa1ArithMode_ = 0;
    std::uint16_t sa1Ma_ = 0;
    std::uint16_t sa1Mb_ = 0;
    std::uint32_t sa1ArithProduct_ = 0;
    std::uint16_t sa1ArithRemainder_ = 0;
    std::uint8_t sa1Cdma_ = 0;
    std::uint8_t sa1Brf_[16] = {};
    int sa1CharRow_ = 0;
    std::uint8_t sa1BbF_ = 0;
    std::uint32_t sa1BitStream_ = 0;
    int sa1BitLeft_ = 0;

    void runSa1Arithmetic();
    void runSa1CharConv1();
    void runSa1CharConv2();
    void extractSa1Bits(int count);
};

}  // namespace iris::systems::snes
