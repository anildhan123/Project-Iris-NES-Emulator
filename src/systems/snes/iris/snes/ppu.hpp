#pragma once

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

class Ppu {
public:
    Ppu();

    void reset();
    void power();
    [[nodiscard]] std::uint8_t read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    void render();
    void renderLine(int y);
    void reloadOam() { oamByte_ = static_cast<int>(oamWord_ & 0x1FF) * 2; }

    [[nodiscard]] const std::uint8_t* pixels() const { return frame_.data(); }
    [[nodiscard]] int width() const { return kWidth; }
    [[nodiscard]] int height() const { return kHeight; }
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

    static constexpr int kWidth = 256;
    static constexpr int kHeight = 224;

private:
    struct Texel {
        int index = -1;
        int priority = 0;
    };

    void writeVram(bool high, std::uint8_t value);
    void writeCgram(std::uint8_t value);
    void writeMode7(std::int16_t& reg, std::uint8_t value);
    void writeScroll(int layer, bool vertical, std::uint8_t value);
    [[nodiscard]] int mode7Pixel(int x, int y) const;
    [[nodiscard]] int mapWord(std::uint8_t screen, int tileX, int tileY) const;
    [[nodiscard]] Texel sampleLayer(int layer, int x, int y, std::uint8_t windowMask) const;
    void paintLayer(int layer, int y, Texel* row) const;
    [[nodiscard]] bool layerMasked(int x, std::uint8_t select, bool enabled, std::uint8_t combine) const;
    [[nodiscard]] std::uint16_t applyMath(std::uint16_t color, int addRed, int addGreen, int addBlue) const;
    [[nodiscard]] int stackRank(int mode, int source, int priority) const;
    [[nodiscard]] int spriteByte(int tile) const;
    void collectSprites(int y, Texel* dots) const;

    std::uint8_t inidisp_ = 0x8F;
    std::uint8_t obsel_ = 0;
    std::uint8_t bgmode_ = 0;
    std::uint8_t bg1sc_ = 0;
    std::uint8_t bg2sc_ = 0;
    std::uint8_t bg3sc_ = 0;
    std::uint8_t bg4sc_ = 0;
    std::uint8_t bg12nba_ = 0;
    std::uint8_t bg34nba_ = 0;
    std::uint16_t hofs_[4] = {};
    std::uint16_t vofs_[4] = {};
    std::uint8_t scrollLatch_ = 0;
    std::uint8_t mosaic_ = 0;
    std::uint8_t tm_ = 0;
    std::uint8_t ts_ = 0;
    std::uint8_t vmain_ = 0;
    std::uint16_t vramAddress_ = 0;
    std::uint16_t oamWord_ = 0;
    int oamByte_ = 0;
    std::uint8_t cgAddress_ = 0;
    bool cgHigh_ = false;
    std::uint8_t cgLatch_ = 0;
    std::uint8_t m7Latch_ = 0;
    std::int16_t m7a_ = 0;
    std::int16_t m7b_ = 0;
    std::int16_t m7c_ = 0;
    std::int16_t m7d_ = 0;
    int m7x_ = 0;
    int m7y_ = 0;
    int m7hofs_ = 0;
    int m7vofs_ = 0;
    std::uint8_t w12sel_ = 0;
    std::uint8_t w34sel_ = 0;
    std::uint8_t wobjsel_ = 0;
    std::uint8_t wlog_ = 0;
    std::uint8_t wobjlog_ = 0;
    std::uint8_t wh0_ = 0;
    std::uint8_t wh1_ = 0;
    std::uint8_t wh2_ = 0;
    std::uint8_t wh3_ = 0;
    std::uint8_t tmw_ = 0;
    std::uint8_t tsw_ = 0;
    std::uint8_t cgwsel_ = 0;
    std::uint8_t cgadsub_ = 0;
    std::uint8_t fixedR_ = 0;
    std::uint8_t fixedG_ = 0;
    std::uint8_t fixedB_ = 0;
    std::vector<std::uint8_t> vram_;
    std::vector<std::uint8_t> oam_;
    std::vector<std::uint16_t> cgram_;
    std::vector<std::uint8_t> frame_;
};

}  // namespace iris::systems::snes
