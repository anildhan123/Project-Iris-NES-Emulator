#pragma once

#include "iris/nes/mapper.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::nes {

class Ppu {
public:
    static constexpr int kWidth = 256;
    static constexpr int kHeight = 240;

    void connect(Mapper* mapper, Mirroring mirroring);
    void setTiming(int scanlines, bool oddFrameSkip);
    void power();
    void reset();
    void clock();
    void writeRegister(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t readRegister(std::uint16_t address);
    void writeOam(std::uint8_t value);

    void captureState(iris::ByteWriter& writer) const;
    [[nodiscard]] bool applyState(iris::ByteReader& reader);

    [[nodiscard]] bool nmiLine() const;
    [[nodiscard]] std::uint64_t frame() const { return frame_; }
    [[nodiscard]] const std::uint8_t* pixels() const { return frameBuffer_.data(); }

private:
    [[nodiscard]] bool rendering() const { return (mask_ & 0x18) != 0; }
    [[nodiscard]] Mirroring activeMirroring() const;
    [[nodiscard]] bool renderingA12() const;
    void noteA12(bool high);
    void renderScanline(int scanline);
    void writeData(std::uint8_t value);
    [[nodiscard]] std::uint8_t readData();
    [[nodiscard]] std::uint8_t readVram(std::uint16_t address);
    void writeVram(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint16_t nametableIndex(std::uint16_t address) const;
    void incrementCoarseX();
    void incrementY();
    void copyHorizontal();
    void copyVertical();
    void writePixel(int x, int y, std::uint8_t paletteIndex);

    Mapper* mapper_ = nullptr;
    Mirroring mirroring_ = Mirroring::Horizontal;
    std::array<std::uint8_t, 4096> vram_{};
    std::array<std::uint8_t, 32> palette_{};
    std::array<std::uint8_t, 256> oam_{};
    std::vector<std::uint8_t> frameBuffer_ = std::vector<std::uint8_t>(static_cast<std::size_t>(kWidth * kHeight * 4), 0);

    std::uint8_t ctrl_ = 0;
    std::uint8_t mask_ = 0;
    std::uint8_t status_ = 0;
    std::uint8_t oamAddr_ = 0;
    std::uint8_t dataLatch_ = 0;
    std::uint8_t readBuffer_ = 0;
    std::uint8_t fineX_ = 0;
    std::uint16_t v_ = 0;
    std::uint16_t t_ = 0;
    bool w_ = false;
    int scanline_ = 0;
    int dot_ = 0;
    int scanlines_ = 262;
    bool oddFrameSkip_ = true;
    bool a12High_ = false;
    int a12LowDots_ = 0;
    std::uint64_t frame_ = 0;
};

}  // namespace iris::systems::nes
