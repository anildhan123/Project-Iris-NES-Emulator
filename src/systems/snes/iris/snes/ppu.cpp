#include "iris/snes/ppu.hpp"

#include "iris/byte_io.hpp"

namespace iris::systems::snes {
namespace {

int brightness(int channel, int level) {
    if (level <= 0) {
        return 0;
    }
    if (level >= 15) {
        return channel * 255 / 31;
    }
    return (channel * level / 15) * 255 / 31;
}

int sign13(int value) {
    value &= 0x1FFF;
    if ((value & 0x1000) != 0) {
        value |= ~0x1FFF;
    }
    return value;
}

}  // namespace

Ppu::Ppu() {
    power();
}

bool Ppu::highResolution() const {
    const int mode = bgmode_ & 0x07;
    return mode == 5 || mode == 6;
}

int Ppu::width() const {
    return highResolution() ? kHiResWidth : kWidth;
}

int Ppu::height() const {
    return overscan() ? kOverscanHeight : kHeight;
}

void Ppu::beginLine(int y) {
    // Snapshot registers at the start of the line. CPU or HDMA writes during the
    // line (and HBlank HDMA) apply from the next beginLine call.
    if (y == 0 && interlaced()) {
        field_ ^= 1;
    }
    relatchLine();
}

void Ppu::relatchLine() {
    latchedBgmode_ = bgmode_;
    latchedTm_ = tm_;
    latchedTs_ = ts_;
    latchedCgwsel_ = cgwsel_;
    latchedCgadsub_ = cgadsub_;
}

void Ppu::power() {
    vram_.assign(64 * 1024, 0);
    oam_.assign(544, 0);
    cgram_.assign(256, 0);
    frame_.assign(static_cast<std::size_t>(kMaxWidth * kMaxHeight * 4), 0);
    reset();
}

void Ppu::reset() {
    inidisp_ = 0x8F;
    obsel_ = 0;
    bgmode_ = 0;
    bg1sc_ = 0;
    bg2sc_ = 0;
    bg3sc_ = 0;
    bg4sc_ = 0;
    bg12nba_ = 0;
    bg34nba_ = 0;
    for (int layer = 0; layer < 4; ++layer) {
        hofs_[layer] = 0;
        vofs_[layer] = 0;
    }
    scrollLatch_ = 0;
    mosaic_ = 0;
    tm_ = 0;
    ts_ = 0;
    vmain_ = 0;
    vramAddress_ = 0;
    oamWord_ = 0;
    oamByte_ = 0;
    cgAddress_ = 0;
    cgHigh_ = false;
    cgLatch_ = 0;
    m7Latch_ = 0;
    m7a_ = 0x0100;
    m7b_ = 0;
    m7c_ = 0;
    m7d_ = 0x0100;
    m7x_ = 0;
    m7y_ = 0;
    m7hofs_ = 0;
    m7vofs_ = 0;
    m7sel_ = 0;
    m7Product_ = 0;
    w12sel_ = 0;
    w34sel_ = 0;
    wobjsel_ = 0;
    wlog_ = 0;
    wobjlog_ = 0;
    wh0_ = 0;
    wh1_ = 0;
    wh2_ = 0;
    wh3_ = 0;
    tmw_ = 0;
    tsw_ = 0;
    cgwsel_ = 0;
    cgadsub_ = 0;
    fixedR_ = 0;
    fixedG_ = 0;
    fixedB_ = 0;
    setini_ = 0;
    latchedBgmode_ = 0;
    latchedTm_ = 0;
    latchedTs_ = 0;
    latchedCgwsel_ = 0;
    latchedCgadsub_ = 0;
    field_ = 0;
    ophct_ = 0;
    opvct_ = 0;
    ophHigh_ = false;
    opvHigh_ = false;
    ppuOpenBus_ = 0;
}

void Ppu::latchCounters(int scanline, int lineDot) {
    // H is in PPU dots (four master clocks). V is the current scanline index.
    ophct_ = static_cast<std::uint16_t>((lineDot / 4) & 0x1FF);
    opvct_ = static_cast<std::uint16_t>(scanline & 0x1FF);
    ophHigh_ = false;
    opvHigh_ = false;
}

std::uint8_t Ppu::read(std::uint16_t address) {
    std::uint8_t value = ppuOpenBus_;
    if (address == 0x2134) {
        value = static_cast<std::uint8_t>(m7Product_);
    } else if (address == 0x2135) {
        value = static_cast<std::uint8_t>(m7Product_ >> 8);
    } else if (address == 0x2136) {
        value = static_cast<std::uint8_t>(m7Product_ >> 16);
    } else if (address == 0x2137) {
        // SLHV is latched by the bus before this read; the data bus keeps open bus.
        value = ppuOpenBus_;
    } else if (address == 0x213C) {
        value = ophHigh_ ? static_cast<std::uint8_t>((ophct_ >> 8) & 0x01) : static_cast<std::uint8_t>(ophct_);
        ophHigh_ = !ophHigh_;
    } else if (address == 0x213D) {
        value = opvHigh_ ? static_cast<std::uint8_t>((opvct_ >> 8) & 0x01) : static_cast<std::uint8_t>(opvct_);
        opvHigh_ = !opvHigh_;
    } else if (address == 0x213F) {
        // Bit 0: 5C77. Bit 7: interlace field when SETINI selects interlaced output.
        value = static_cast<std::uint8_t>(0x01 | ((field_ & 1) != 0 ? 0x80 : 0));
    } else if (address == 0x2139 || address == 0x213A) {
        const std::size_t byte = static_cast<std::size_t>(vramAddress_ & 0x7FFF) * 2 + (address == 0x213A ? 1 : 0);
        if (byte < vram_.size()) {
            value = vram_[byte];
        }
        if ((address == 0x213A) == ((vmain_ & 0x80) != 0)) {
            vramAddress_ = static_cast<std::uint16_t>((vramAddress_ + 1) & 0x7FFF);
        }
    } else if (address == 0x2138) {
        const int index = oamByte_;
        value = index >= 0 && static_cast<std::size_t>(index) < oam_.size() ? oam_[static_cast<std::size_t>(index)] : 0;
        if (oamByte_ < 544) {
            ++oamByte_;
        }
    }
    ppuOpenBus_ = value;
    return value;
}

void Ppu::write(std::uint16_t address, std::uint8_t value) {
    switch (address) {
    case 0x2100:
        inidisp_ = value;
        break;
    case 0x2101:
        obsel_ = value;
        break;
    case 0x2102:
        oamWord_ = static_cast<std::uint16_t>((oamWord_ & 0x100) | value);
        oamByte_ = static_cast<int>(oamWord_ & 0x1FF) * 2;
        break;
    case 0x2103:
        oamWord_ = static_cast<std::uint16_t>((oamWord_ & 0x00FF) | ((value & 0x01) << 8));
        oamByte_ = static_cast<int>(oamWord_ & 0x1FF) * 2;
        break;
    case 0x2104:
        if (oamByte_ >= 0 && oamByte_ < static_cast<int>(oam_.size())) {
            oam_[static_cast<std::size_t>(oamByte_)] = value;
        }
        if (oamByte_ < 544) {
            ++oamByte_;
        }
        break;
    case 0x2105:
        bgmode_ = value;
        break;
    case 0x210D:
        writeScroll(0, false, value);
        break;
    case 0x210E:
        writeScroll(0, true, value);
        break;
    case 0x210F:
        writeScroll(1, false, value);
        break;
    case 0x2110:
        writeScroll(1, true, value);
        break;
    case 0x2111:
        writeScroll(2, false, value);
        break;
    case 0x2112:
        writeScroll(2, true, value);
        break;
    case 0x2113:
        writeScroll(3, false, value);
        break;
    case 0x2114:
        writeScroll(3, true, value);
        break;
    case 0x211A:
        m7sel_ = value;
        break;
    case 0x211B:
        writeMode7(m7a_, value);
        break;
    case 0x211C:
        writeMode7(m7b_, value);
        // 16×8 signed product of M7A and the byte written to M7B.
        m7Product_ = static_cast<std::int32_t>(m7a_) * static_cast<std::int8_t>(value);
        break;
    case 0x211D:
        writeMode7(m7c_, value);
        break;
    case 0x211E:
        writeMode7(m7d_, value);
        break;
    case 0x211F:
        m7x_ = sign13(m7Latch_ | ((value & 0x1F) << 8));
        m7Latch_ = value;
        break;
    case 0x2120:
        m7y_ = sign13(m7Latch_ | ((value & 0x1F) << 8));
        m7Latch_ = value;
        break;
    case 0x2123:
        w12sel_ = value;
        break;
    case 0x2124:
        w34sel_ = value;
        break;
    case 0x2125:
        wobjsel_ = value;
        break;
    case 0x2126:
        wh0_ = value;
        break;
    case 0x2127:
        wh1_ = value;
        break;
    case 0x2128:
        wh2_ = value;
        break;
    case 0x2129:
        wh3_ = value;
        break;
    case 0x212A:
        wlog_ = value;
        break;
    case 0x212B:
        wobjlog_ = value;
        break;
    case 0x212E:
        tmw_ = value;
        break;
    case 0x212F:
        tsw_ = value;
        break;
    case 0x2130:
        cgwsel_ = value;
        break;
    case 0x2131:
        cgadsub_ = value;
        break;
    case 0x2132:
        if ((value & 0x20) != 0) {
            fixedR_ = static_cast<std::uint8_t>(value & 0x1F);
        }
        if ((value & 0x40) != 0) {
            fixedG_ = static_cast<std::uint8_t>(value & 0x1F);
        }
        if ((value & 0x80) != 0) {
            fixedB_ = static_cast<std::uint8_t>(value & 0x1F);
        }
        break;
    case 0x2133:
        setini_ = value;
        break;
    case 0x2106:
        mosaic_ = value;
        break;
    case 0x2107:
        bg1sc_ = value;
        break;
    case 0x2108:
        bg2sc_ = value;
        break;
    case 0x2109:
        bg3sc_ = value;
        break;
    case 0x210A:
        bg4sc_ = value;
        break;
    case 0x210B:
        bg12nba_ = value;
        break;
    case 0x210C:
        bg34nba_ = value;
        break;
    case 0x2115:
        vmain_ = value;
        break;
    case 0x2116:
        vramAddress_ = static_cast<std::uint16_t>((vramAddress_ & 0xFF00) | value);
        break;
    case 0x2117:
        vramAddress_ = static_cast<std::uint16_t>((vramAddress_ & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
        break;
    case 0x2118:
        writeVram(false, value);
        break;
    case 0x2119:
        writeVram(true, value);
        break;
    case 0x2121:
        cgAddress_ = value;
        cgHigh_ = false;
        break;
    case 0x2122:
        writeCgram(value);
        break;
    case 0x212C:
        tm_ = value;
        break;
    case 0x212D:
        ts_ = value;
        break;
    default:
        break;
    }
}

void Ppu::writeScroll(int layer, bool vertical, std::uint8_t value) {
    const int low = scrollLatch_;
    scrollLatch_ = value;
    const auto bg = static_cast<std::uint16_t>((low | ((value & 0x03) << 8)) & 0x03FF);
    if (vertical) {
        vofs_[layer] = bg;
    } else {
        hofs_[layer] = bg;
    }
    if (layer == 0) {
        const int wide = sign13(low | ((value & 0x1F) << 8));
        if (vertical) {
            m7vofs_ = wide;
        } else {
            m7hofs_ = wide;
        }
    }
}

void Ppu::writeVram(bool high, std::uint8_t value) {
    const std::size_t byte = static_cast<std::size_t>(vramAddress_ & 0x7FFF) * 2 + (high ? 1 : 0);
    if (byte >= vram_.size()) {
        return;
    }
    vram_[byte] = value;
    if (high == ((vmain_ & 0x80) != 0)) {
        int step = 1;
        if ((vmain_ & 0x03) == 1) {
            step = 32;
        } else if ((vmain_ & 0x03) != 0) {
            step = 128;
        }
        vramAddress_ = static_cast<std::uint16_t>((vramAddress_ + step) & 0x7FFF);
    }
}

void Ppu::writeCgram(std::uint8_t value) {
    const std::size_t index = cgAddress_;
    if (!cgHigh_) {
        cgLatch_ = value;
        cgHigh_ = true;
        return;
    }
    cgram_[index] = static_cast<std::uint16_t>(cgLatch_ | (static_cast<std::uint16_t>(value & 0x7F) << 8));
    cgAddress_ = static_cast<std::uint8_t>(cgAddress_ + 1);
    cgHigh_ = false;
}

void Ppu::writeMode7(std::int16_t& reg, std::uint8_t value) {
    reg = static_cast<std::int16_t>(m7Latch_ | (static_cast<int>(value) << 8));
    m7Latch_ = value;
}

int Ppu::mode7Pixel(int x, int y) const {
    if (vram_.size() < 2) {
        return -1;
    }
    int sx = x;
    int sy = y;
    if ((m7sel_ & 0x01) != 0) {
        sx = 255 - x;
    }
    if ((m7sel_ & 0x02) != 0) {
        sy = 255 - y;
    }
    const int orgX = sx + m7hofs_ - m7x_;
    const int orgY = sy + m7vofs_ - m7y_;
    int vx = ((static_cast<int>(m7a_) * orgX + static_cast<int>(m7b_) * orgY) >> 8) + m7x_;
    int vy = ((static_cast<int>(m7c_) * orgX + static_cast<int>(m7d_) * orgY) >> 8) + m7y_;
    bool outside = vx < 0 || vy < 0 || vx >= 1024 || vy >= 1024;
    if (outside) {
        if ((m7sel_ & 0x80) == 0) {
            // Tilemap repeat (default).
            vx &= 1023;
            vy &= 1023;
            outside = false;
        } else if ((m7sel_ & 0x40) == 0) {
            return -1;
        }
        // Else fill with character 0; keep low bits for the pixel within the tile.
    }
    int tile = 0;
    if (!outside) {
        const std::size_t mapWordIndex = static_cast<std::size_t>((vy >> 3) * 128 + (vx >> 3));
        if (mapWordIndex * 2 >= vram_.size()) {
            return -1;
        }
        tile = vram_[mapWordIndex * 2];
    }
    const std::size_t pixelWord = static_cast<std::size_t>(tile * 64 + (vy & 7) * 8 + (vx & 7));
    if (pixelWord * 2 + 1 >= vram_.size()) {
        return -1;
    }
    return vram_[pixelWord * 2 + 1];
}

int Ppu::mapWord(std::uint8_t screen, int tileX, int tileY) const {
    const int width = (screen & 0x01) != 0 ? 64 : 32;
    const int height = (screen & 0x02) != 0 ? 64 : 32;
    tileX &= width - 1;
    tileY &= height - 1;
    int page = 0;
    if (tileX >= 32) {
        page += 1;
        tileX -= 32;
    }
    if (tileY >= 32) {
        page += width == 64 ? 2 : 1;
        tileY -= 32;
    }
    const int base = ((screen & 0xFC) >> 2) * 2048;
    const int offset = base + page * 2048 + (tileY * 32 + tileX) * 2;
    if (offset < 0 || static_cast<std::size_t>(offset + 1) >= vram_.size()) {
        return 0;
    }
    return vram_[static_cast<std::size_t>(offset)] | (vram_[static_cast<std::size_t>(offset + 1)] << 8);
}

bool Ppu::layerMasked(int x, std::uint8_t select, bool enabled, std::uint8_t combine) const {
    if (!enabled) {
        return false;
    }
    const bool window1 = (select & 0x02) != 0;
    const bool window2 = (select & 0x08) != 0;
    if (!window1 && !window2) {
        return false;
    }
    const auto region = [](int pixel, int left, int right, bool invert) {
        const bool inside = left <= right && pixel >= left && pixel <= right;
        return invert ? !inside : inside;
    };
    const bool in1 = region(x, wh0_, wh1_, (select & 0x01) != 0);
    const bool in2 = region(x, wh2_, wh3_, (select & 0x04) != 0);
    if (window1 && !window2) {
        return in1;
    }
    if (window2 && !window1) {
        return in2;
    }
    switch (combine & 0x03) {
    case 1:
        return in1 && in2;
    case 2:
        return in1 != in2;
    case 3:
        return in1 == in2;
    default:
        return in1 || in2;
    }
}

bool Ppu::colorWindowMath(int x, bool main) const {
    // CGWSEL bits 4–5 (main) / 6–7 (sub): 00 always, 01 inside, 10 outside, 11 never.
    const int mode = main ? ((latchedCgwsel_ >> 4) & 0x03) : ((latchedCgwsel_ >> 6) & 0x03);
    if (mode == 0) {
        return true;
    }
    if (mode == 3) {
        return false;
    }
    const bool inside = layerMasked(x, wobjsel_ >> 4, true, static_cast<std::uint8_t>(wobjlog_ >> 2));
    return mode == 1 ? inside : !inside;
}

std::uint16_t Ppu::applyMath(std::uint16_t color, int addRed, int addGreen, int addBlue) const {
    const bool subtract = (latchedCgadsub_ & 0x80) != 0;
    const bool half = (latchedCgadsub_ & 0x40) != 0;
    const auto mix = [&](int channel, int addend) {
        int value = subtract ? channel - addend : channel + addend;
        if (half) {
            value /= 2;
        }
        if (value < 0) {
            value = 0;
        }
        if (value > 31) {
            value = 31;
        }
        return value;
    };
    const int red = mix(color & 0x1F, addRed);
    const int green = mix((color >> 5) & 0x1F, addGreen);
    const int blue = mix((color >> 10) & 0x1F, addBlue);
    return static_cast<std::uint16_t>(red | (green << 5) | (blue << 10));
}

void Ppu::applyOffsetPerTile(int layer, int mode, int x, int y, int& sampleX, int& sampleY) const {
    if ((mode != 2 && mode != 4 && mode != 6) || layer > 1 || (mode == 6 && layer != 0)) {
        return;
    }
    const int hofs = hofs_[layer] & 0x3FF;
    // Leftmost visible 8×8 column is never OPT-offset.
    if (((x + (hofs & 7)) >> 3) == 0) {
        return;
    }
    const int bg3h = hofs_[2] & 0x3FF;
    const int bg3v = vofs_[2] & 0x3FF;
    // Anomie: optX = (HOFS&7) | (((X-8)&~7) + (BG3HOFS&~7)), HOFS = X + BGnHOFS.
    const int optX = ((x + hofs) & 7) | (((x - 8) & ~7) + (bg3h & ~7));
    const int validBit = layer == 0 ? 0x2000 : 0x4000;
    int h = sampleX;
    int v = sampleY;
    if (mode == 4) {
        const int word = mapWord(bg3sc_, optX >> 3, bg3v >> 3);
        if ((word & validBit) != 0) {
            if ((word & 0x8000) != 0) {
                v = y + (word & 0x3FF);
            } else {
                h = (h & 7) | ((x & ~7) + ((word & 0x3FF) & ~7));
            }
        }
    } else {
        const int hWord = mapWord(bg3sc_, optX >> 3, bg3v >> 3);
        const int vWord = mapWord(bg3sc_, optX >> 3, (bg3v + 8) >> 3);
        if ((hWord & validBit) != 0) {
            h = (h & 7) | ((x & ~7) + ((hWord & 0x3FF) & ~7));
        }
        if ((vWord & validBit) != 0) {
            v = y + (vWord & 0x3FF);
        }
    }
    sampleX = h;
    sampleY = v;
}

int Ppu::stackRank(int mode, int source, int priority) const {
    struct Slot {
        int source;
        int priority;
    };
    static constexpr Slot kMode0[] = {
        {4, 3}, {0, 1}, {1, 1}, {4, 2}, {0, 0}, {1, 0}, {4, 1}, {2, 1}, {3, 1}, {4, 0}, {2, 0}, {3, 0},
    };
    static constexpr Slot kMode1Low[] = {
        {4, 3}, {0, 1}, {1, 1}, {4, 2}, {0, 0}, {1, 0}, {4, 1}, {2, 1}, {4, 0}, {2, 0},
    };
    static constexpr Slot kMode1High[] = {
        {2, 1}, {4, 3}, {0, 1}, {1, 1}, {4, 2}, {0, 0}, {1, 0}, {4, 1}, {2, 0}, {4, 0},
    };
    // Modes 3/5 (and similar two-BG stacks).
    static constexpr Slot kModes[] = {
        {4, 3}, {0, 1}, {1, 1}, {4, 2}, {0, 0}, {1, 0}, {4, 1}, {4, 0},
    };
    // Mode 2 / 6: OBJ3 > BG1.1 > OBJ2 > BG2.1 > OBJ1 > BG1.0 > OBJ0 > BG2.0.
    static constexpr Slot kMode2[] = {
        {4, 3}, {0, 1}, {4, 2}, {1, 1}, {4, 1}, {0, 0}, {4, 0}, {1, 0},
    };
    // Mode 7: OBJ3 > OBJ2 > BG2 (EXTBG high) > BG1 > OBJ1 > OBJ0.
    static constexpr Slot kMode7[] = {
        {4, 3}, {4, 2}, {1, 1}, {0, 0}, {4, 1}, {4, 0},
    };
    const Slot* table = kModes;
    int count = 8;
    if (mode == 0) {
        table = kMode0;
        count = 12;
    } else if (mode == 1 && (bgmode_ & 0x08) != 0) {
        table = kMode1High;
        count = 10;
    } else if (mode == 1) {
        table = kMode1Low;
        count = 10;
    } else if (mode == 2 || mode == 6) {
        table = kMode2;
        count = 8;
    } else if (mode == 7) {
        table = kMode7;
        count = 6;
    }
    for (int index = 0; index < count; ++index) {
        if (table[index].source == source && table[index].priority == priority) {
            return index;
        }
    }
    return -1;
}

int Ppu::spriteByte(int tile) const {
    const int base = (obsel_ & 0x07) << 14;
    const int name = tile & 0xFF;
    if ((tile & 0x100) != 0) {
        const int gap = ((obsel_ >> 3) & 0x03) + 1;
        return base + gap * 0x2000 + name * 32;
    }
    return base + name * 32;
}

Ppu::Texel Ppu::sampleLayer(int layer, int x, int y, std::uint8_t windowMask) const {
    const int mode = bgmode_ & 0x07;
    if (mode == 7 || layer > 3) {
        return {};
    }
    int bpp = 0;
    if (layer == 0) {
        bpp = mode == 0 ? 2 : (mode == 3 || mode == 4 ? 8 : 4);
    } else if (layer == 1) {
        if (mode == 6) {
            return {};
        }
        bpp = (mode == 0 || mode == 4 || mode == 5) ? 2 : 4;
    } else if (layer == 2) {
        if (mode == 0 || mode == 1) {
            bpp = 2;
        }
    } else if (mode == 0) {
        bpp = 2;
    }
    if (bpp == 0) {
        return {};
    }
    std::uint8_t window = w12sel_;
    if (layer == 1) {
        window = static_cast<std::uint8_t>(w12sel_ >> 4);
    } else if (layer == 2) {
        window = w34sel_;
    } else if (layer == 3) {
        window = static_cast<std::uint8_t>(w34sel_ >> 4);
    }
    if (layerMasked(x, window, (windowMask & (1 << layer)) != 0, static_cast<std::uint8_t>(wlog_ >> (layer * 2)))) {
        return {};
    }
    if ((mosaic_ & (0x10 << layer)) != 0) {
        const int size = (mosaic_ & 0x0F) + 1;
        x -= x % size;
        y -= y % size;
    }
    int sampleX = x + (hofs_[layer] & 0x3FF);
    int sampleY = y + (vofs_[layer] & 0x3FF);
    applyOffsetPerTile(layer, mode, x, y, sampleX, sampleY);
    std::uint8_t screen = bg1sc_;
    int nameNibble = bg12nba_ & 0x0F;
    if (layer == 1) {
        screen = bg2sc_;
        nameNibble = bg12nba_ >> 4;
    } else if (layer == 2) {
        screen = bg3sc_;
        nameNibble = bg34nba_ & 0x0F;
    } else if (layer == 3) {
        screen = bg4sc_;
        nameNibble = bg34nba_ >> 4;
    }
    const int word = mapWord(screen, sampleX >> 3, sampleY >> 3);
    const int tile = word & 0x3FF;
    const int palette = (word >> 10) & 0x07;
    const int priority = (word & 0x2000) != 0 ? 1 : 0;
    const bool flipX = (word & 0x4000) != 0;
    const bool flipY = (word & 0x8000) != 0;
    const int row = flipY ? 7 - (sampleY & 7) : (sampleY & 7);
    const int column = flipX ? (sampleX & 7) : 7 - (sampleX & 7);
    const int bytesPerTile = bpp == 2 ? 16 : (bpp == 8 ? 64 : 32);
    const int charBase = nameNibble << 13;
    const std::size_t tileByte = static_cast<std::size_t>(charBase + tile * bytesPerTile + row * 2);
    const int planeSpan = bpp == 8 ? 49 : (bpp == 4 ? 17 : 1);
    if (tileByte + static_cast<std::size_t>(planeSpan) >= vram_.size()) {
        return {};
    }
    int pixel = ((vram_[tileByte] >> column) & 1) | (((vram_[tileByte + 1] >> column) & 1) << 1);
    if (bpp >= 4) {
        pixel |= ((vram_[tileByte + 16] >> column) & 1) << 2;
        pixel |= ((vram_[tileByte + 17] >> column) & 1) << 3;
    }
    if (bpp == 8) {
        pixel |= ((vram_[tileByte + 32] >> column) & 1) << 4;
        pixel |= ((vram_[tileByte + 33] >> column) & 1) << 5;
        pixel |= ((vram_[tileByte + 48] >> column) & 1) << 6;
        pixel |= ((vram_[tileByte + 49] >> column) & 1) << 7;
    }
    if (pixel == 0) {
        return {};
    }
    Texel texel;
    texel.priority = priority;
    texel.index = bpp == 8 ? pixel : ((palette << (bpp == 2 ? 2 : 4)) | pixel);
    return texel;
}

void Ppu::render() {
    for (int y = 0; y < kHeight; ++y) {
        renderLine(y);
    }
}

void Ppu::paintLayer(int layer, int y, Texel* row, int lineWidth) const {
    for (int x = 0; x < lineWidth; ++x) {
        row[x] = {};
    }
    const int mode = bgmode_ & 0x07;
    if (mode == 7 || layer < 0 || layer > 3) {
        return;
    }
    int bpp = 0;
    if (layer == 0) {
        bpp = mode == 0 ? 2 : (mode == 3 || mode == 4 ? 8 : 4);
    } else if (layer == 1) {
        if (mode == 6) {
            return;
        }
        bpp = (mode == 0 || mode == 4 || mode == 5) ? 2 : 4;
    } else if (layer == 2) {
        if (mode == 0 || mode == 1) {
            bpp = 2;
        }
    } else if (mode == 0) {
        bpp = 2;
    }
    if (bpp == 0) {
        return;
    }
    std::uint8_t screen = bg1sc_;
    int nameNibble = bg12nba_ & 0x0F;
    if (layer == 1) {
        screen = bg2sc_;
        nameNibble = bg12nba_ >> 4;
    } else if (layer == 2) {
        screen = bg3sc_;
        nameNibble = bg34nba_ & 0x0F;
    } else if (layer == 3) {
        screen = bg4sc_;
        nameNibble = bg34nba_ >> 4;
    }
    const int bytesPerTile = bpp == 2 ? 16 : (bpp == 8 ? 64 : 32);
    const int charBase = nameNibble << 13;
    const int planeSpan = bpp == 8 ? 49 : (bpp == 4 ? 17 : 1);
    const bool mosaic = (mosaic_ & (0x10 << layer)) != 0;
    const int mosaicSize = (mosaic_ & 0x0F) + 1;
    // Modes 5 and 6 draw 512 dots. Each character pixel is one half-dot wide, so
    // the same 8 by 8 tile covers twice the horizontal map range of a low-res line.
    const std::uint8_t* vram = vram_.data();
    const std::size_t vramSize = vram_.size();
    int cacheTileX = 0x7FFFFFFF;
    int cacheTileY = 0;
    int cacheFineY = -1;
    int cachePriority = 0;
    int cachePalette = 0;
    bool cacheFlipX = false;
    bool cacheEmpty = true;
    std::uint8_t plane[8] = {};
    for (int x = 0; x < lineWidth; ++x) {
        int sampleX = x + (hofs_[layer] & 0x3FF);
        int sampleY = y + (vofs_[layer] & 0x3FF);
        int screenX = x;
        int screenY = y;
        if (mosaic) {
            const int unit = lineWidth > kWidth ? 2 : 1;
            const int mosaicX = (x / unit) - ((x / unit) % mosaicSize);
            screenX = mosaicX * unit + (x % unit);
            screenY = y - (y % mosaicSize);
            sampleX = screenX + (hofs_[layer] & 0x3FF);
            sampleY = screenY + (vofs_[layer] & 0x3FF);
        }
        applyOffsetPerTile(layer, mode, screenX, screenY, sampleX, sampleY);
        const int tileX = sampleX >> 3;
        const int tileY = sampleY >> 3;
        const int fineY = sampleY & 7;
        if (tileX != cacheTileX || tileY != cacheTileY || fineY != cacheFineY) {
            cacheTileX = tileX;
            cacheTileY = tileY;
            cacheFineY = fineY;
            cacheEmpty = true;
            const int word = mapWord(screen, tileX, tileY);
            const int tile = word & 0x3FF;
            cachePalette = (word >> 10) & 0x07;
            cachePriority = (word & 0x2000) != 0 ? 1 : 0;
            cacheFlipX = (word & 0x4000) != 0;
            const bool flipY = (word & 0x8000) != 0;
            const int tileRow = flipY ? 7 - (sampleY & 7) : (sampleY & 7);
            const std::size_t tileByte = static_cast<std::size_t>(charBase + tile * bytesPerTile + tileRow * 2);
            if (tileByte + static_cast<std::size_t>(planeSpan) < vramSize) {
                plane[0] = vram[tileByte];
                plane[1] = vram[tileByte + 1];
                if (bpp >= 4) {
                    plane[2] = vram[tileByte + 16];
                    plane[3] = vram[tileByte + 17];
                }
                if (bpp == 8) {
                    plane[4] = vram[tileByte + 32];
                    plane[5] = vram[tileByte + 33];
                    plane[6] = vram[tileByte + 48];
                    plane[7] = vram[tileByte + 49];
                }
                cacheEmpty = false;
            }
        }
        if (cacheEmpty) {
            continue;
        }
        const int column = cacheFlipX ? (sampleX & 7) : 7 - (sampleX & 7);
        int pixel = ((plane[0] >> column) & 1) | (((plane[1] >> column) & 1) << 1);
        if (bpp >= 4) {
            pixel |= ((plane[2] >> column) & 1) << 2;
            pixel |= ((plane[3] >> column) & 1) << 3;
        }
        if (bpp == 8) {
            pixel |= ((plane[4] >> column) & 1) << 4;
            pixel |= ((plane[5] >> column) & 1) << 5;
            pixel |= ((plane[6] >> column) & 1) << 6;
            pixel |= ((plane[7] >> column) & 1) << 7;
        }
        if (pixel == 0) {
            continue;
        }
        row[x].priority = cachePriority;
        row[x].index = bpp == 8 ? pixel : ((cachePalette << (bpp == 2 ? 2 : 4)) | pixel);
    }
}

void Ppu::renderLine(int y) {
    if (y < 0 || y >= height()) {
        return;
    }
    const int lineWidth = width();
    std::uint8_t* pixelOut = frame_.data() + static_cast<std::size_t>(y * lineWidth * 4);
    const int level = inidisp_ & 0x0F;
    const bool blank = (inidisp_ & 0x80) != 0 || level == 0;
    if (blank) {
        for (int x = 0; x < lineWidth; ++x) {
            pixelOut[x * 4] = 0;
            pixelOut[x * 4 + 1] = 0;
            pixelOut[x * 4 + 2] = 0;
            pixelOut[x * 4 + 3] = 255;
        }
        return;
    }
    const int mode = latchedBgmode_ & 0x07;
    const bool mode7 = mode == 7;
    const bool useSub = (latchedCgwsel_ & 0x02) != 0 && (latchedCgadsub_ & 0x3F) != 0;
    const std::uint8_t enabled =
        static_cast<std::uint8_t>(latchedTm_ | (useSub ? latchedTs_ : 0));
    // Modes 5–6 with SETINI interlace double the source grid per field. Other
    // modes still weave by sampling one line lower on the odd field.
    int sampleY = y;
    if (interlaced()) {
        if (mode == 5 || mode == 6) {
            sampleY = y * 2 + (field_ & 1);
        } else if ((field_ & 1) != 0) {
            sampleY = y + 1;
        }
    }
    const int spriteY =
        interlaced() && objInterlaced() ? y * 2 + (field_ & 1) : sampleY;
    Texel layers[4][kMaxWidth];
    bool layerReady[4] = {};
    if (!mode7) {
        for (int layer = 0; layer < 4; ++layer) {
            if ((enabled & (1 << layer)) == 0) {
                continue;
            }
            paintLayer(layer, sampleY, layers[layer], lineWidth);
            layerReady[layer] = true;
        }
    }
    const bool extbg = mode7 && (setini_ & 0x40) != 0;
    int mode7Index[kMaxWidth] = {};
    if (mode7 && ((enabled & 0x01) != 0 || (extbg && (enabled & 0x02) != 0))) {
        for (int x = 0; x < lineWidth; ++x) {
            mode7Index[x] = mode7Pixel(x, sampleY);
        }
    }
    Texel sprites[kMaxWidth] = {};
    if ((enabled & 0x10) != 0) {
        collectSprites(spriteY, sprites, lineWidth);
    }
    int rank[5][4];
    for (int source = 0; source < 5; ++source) {
        for (int priority = 0; priority < 4; ++priority) {
            rank[source][priority] = stackRank(mode, source, priority);
        }
    }
    const std::uint16_t backdrop = cgram_.empty() ? 0 : cgram_[0];
    const std::uint16_t* cgram = cgram_.data();
    const std::size_t cgramSize = cgram_.size();
    struct Hit {
        bool opaque = false;
        int index = 0;
        int mathBit = 0;
        int placed = 100;
    };
    const auto consider = [&](Hit& best, int index, int source, int priority, int mathBit) {
        if (index <= 0) {
            return;
        }
        const int placed = rank[source][priority];
        if (placed < 0) {
            return;
        }
        if (!best.opaque || placed < best.placed) {
            best.opaque = true;
            best.index = index;
            best.mathBit = mathBit;
            best.placed = placed;
        }
    };
    const int windowXScale = lineWidth > kWidth ? 2 : 1;
    for (int x = 0; x < lineWidth; ++x) {
        const int windowX = x / windowXScale;
        const auto sampleScreen = [&](std::uint8_t enable, std::uint8_t windowMask) {
            Hit best;
            if (mode7) {
                const int pixel = mode7Index[x];
                if (pixel > 0) {
                    if (extbg) {
                        const int color = pixel & 0x7F;
                        const bool high = (pixel & 0x80) != 0;
                        if (color > 0) {
                            if (high && (enable & 0x02) != 0 &&
                                !layerMasked(windowX, static_cast<std::uint8_t>(w12sel_ >> 4),
                                             (windowMask & 0x02) != 0,
                                             static_cast<std::uint8_t>(wlog_ >> 2))) {
                                consider(best, color, 1, 1, 0x02);
                            } else if (!high && (enable & 0x01) != 0 &&
                                       !layerMasked(windowX, w12sel_, (windowMask & 0x01) != 0, wlog_ & 0x03)) {
                                consider(best, color, 0, 0, 0x01);
                            }
                        }
                    } else if ((enable & 0x01) != 0 &&
                               !layerMasked(windowX, w12sel_, (windowMask & 0x01) != 0, wlog_ & 0x03)) {
                        consider(best, pixel, 0, 0, 0x01);
                    }
                }
            } else {
                for (int layer = 3; layer >= 0; --layer) {
                    if ((enable & (1 << layer)) == 0 || !layerReady[layer]) {
                        continue;
                    }
                    std::uint8_t window = w12sel_;
                    if (layer == 1) {
                        window = static_cast<std::uint8_t>(w12sel_ >> 4);
                    } else if (layer == 2) {
                        window = w34sel_;
                    } else if (layer == 3) {
                        window = static_cast<std::uint8_t>(w34sel_ >> 4);
                    }
                    if (layerMasked(
                            windowX, window, (windowMask & (1 << layer)) != 0,
                            static_cast<std::uint8_t>(wlog_ >> (layer * 2)))) {
                        continue;
                    }
                    consider(best, layers[layer][x].index, layer, layers[layer][x].priority, 1 << layer);
                }
            }
            if ((enable & 0x10) != 0 &&
                !layerMasked(
                    windowX, wobjsel_, (windowMask & 0x10) != 0, static_cast<std::uint8_t>(wobjlog_ & 0x03))) {
                consider(best, sprites[x].index, 4, sprites[x].priority, 0x10);
            }
            return best;
        };
        const Hit main = sampleScreen(latchedTm_, tmw_);
        const Hit sub = useSub ? sampleScreen(latchedTs_, tsw_) : Hit{};
        std::uint16_t color = backdrop;
        int mathBit = 0;
        if (main.opaque && static_cast<std::size_t>(main.index) < cgramSize) {
            color = cgram[static_cast<std::size_t>(main.index)];
            mathBit = main.mathBit;
        }
        const bool mathBackdrop = !main.opaque && (latchedCgadsub_ & 0x20) != 0;
        const bool wantsMath = mathBackdrop || (mathBit != 0 && (latchedCgadsub_ & mathBit) != 0);
        if (wantsMath && colorWindowMath(windowX, true)) {
            int addRed = fixedR_;
            int addGreen = fixedG_;
            int addBlue = fixedB_;
            if (useSub) {
                addRed = 0;
                addGreen = 0;
                addBlue = 0;
                // Subscreen color window can force the fixed color when the sub sample is clipped.
                if (sub.opaque && static_cast<std::size_t>(sub.index) < cgramSize &&
                    colorWindowMath(windowX, false)) {
                    const std::uint16_t behind = cgram[static_cast<std::size_t>(sub.index)];
                    addRed = behind & 0x1F;
                    addGreen = (behind >> 5) & 0x1F;
                    addBlue = (behind >> 10) & 0x1F;
                } else {
                    addRed = fixedR_;
                    addGreen = fixedG_;
                    addBlue = fixedB_;
                }
            }
            color = applyMath(color, addRed, addGreen, addBlue);
        }
        pixelOut[x * 4] = static_cast<std::uint8_t>(brightness(color & 0x1F, level));
        pixelOut[x * 4 + 1] = static_cast<std::uint8_t>(brightness((color >> 5) & 0x1F, level));
        pixelOut[x * 4 + 2] = static_cast<std::uint8_t>(brightness((color >> 10) & 0x1F, level));
        pixelOut[x * 4 + 3] = 255;
    }
}

void Ppu::collectSprites(int y, Texel* dots, int lineWidth) const {
    const int scale = lineWidth > kWidth ? 2 : 1;
    for (int index = 127; index >= 0; --index) {
        const std::size_t entry = static_cast<std::size_t>(index) * 4;
        if (entry + 3 >= oam_.size()) {
            continue;
        }
        const std::size_t highIndex = 512 + static_cast<std::size_t>(index / 4);
        if (highIndex >= oam_.size()) {
            continue;
        }
        const int high = oam_[highIndex];
        const int shift = (index & 3) * 2;
        static constexpr int kSmallW[8] = {8, 8, 8, 16, 16, 32, 16, 16};
        static constexpr int kSmallH[8] = {8, 8, 8, 16, 16, 32, 32, 32};
        static constexpr int kLargeW[8] = {16, 32, 64, 32, 64, 64, 32, 32};
        static constexpr int kLargeH[8] = {16, 32, 64, 32, 64, 64, 64, 32};
        const int sizeSelect = (obsel_ >> 5) & 0x07;
        const bool large = ((high >> (shift + 1)) & 0x01) != 0;
        const int spriteWidth = large ? kLargeW[sizeSelect] : kSmallW[sizeSelect];
        const int spriteHeight = large ? kLargeH[sizeSelect] : kSmallH[sizeSelect];
        const int originX = (oam_[entry] | (((high >> shift) & 0x01) != 0 ? 0x100 : 0)) * scale;
        const int originY = oam_[entry + 1];
        const int tile = oam_[entry + 2] | ((oam_[entry + 3] & 0x01) << 8);
        const int palette = (oam_[entry + 3] >> 1) & 0x07;
        const int priority = (oam_[entry + 3] >> 4) & 0x03;
        const bool flipX = (oam_[entry + 3] & 0x40) != 0;
        const bool flipY = (oam_[entry + 3] & 0x80) != 0;
        const int rowFromTop = y - originY;
        if (rowFromTop < 0 || rowFromTop >= spriteHeight) {
            continue;
        }
        const int localY = flipY ? spriteHeight - 1 - rowFromTop : rowFromTop;
        const int row = localY & 7;
        const int tileRow = localY >> 3;
        const int drawWidth = spriteWidth * scale;
        for (int px = 0; px < drawWidth; ++px) {
            const int screenX = (originX + px) & (scale > 1 ? 0x3FF : 0x1FF);
            if (screenX >= lineWidth) {
                continue;
            }
            const int sourceX = px / scale;
            const int localX = flipX ? spriteWidth - 1 - sourceX : sourceX;
            const int subTile = (tile + (localX >> 3) + tileRow * 16) & 0x1FF;
            const std::size_t tileByte = static_cast<std::size_t>(spriteByte(subTile) + row * 2);
            if (tileByte + 17 >= vram_.size()) {
                continue;
            }
            const int column = 7 - (localX & 7);
            const int pixel = ((vram_[tileByte] >> column) & 1) | (((vram_[tileByte + 1] >> column) & 1) << 1) |
                              (((vram_[tileByte + 16] >> column) & 1) << 2) |
                              (((vram_[tileByte + 17] >> column) & 1) << 3);
            if (pixel == 0) {
                continue;
            }
            dots[screenX].index = 128 + palette * 16 + pixel;
            dots[screenX].priority = priority;
        }
    }
}


void Ppu::saveState(iris::ByteWriter& out) const {
    out.u8(inidisp_);
    out.u8(obsel_);
    out.u8(bgmode_);
    out.u8(bg1sc_);
    out.u8(bg2sc_);
    out.u8(bg3sc_);
    out.u8(bg4sc_);
    out.u8(bg12nba_);
    out.u8(bg34nba_);
    for (int layer = 0; layer < 4; ++layer) {
        out.u16(hofs_[layer]);
        out.u16(vofs_[layer]);
    }
    out.u8(scrollLatch_);
    out.u8(mosaic_);
    out.u8(tm_);
    out.u8(ts_);
    out.u8(vmain_);
    out.u16(vramAddress_);
    out.u16(oamWord_);
    out.u32(static_cast<std::uint32_t>(oamByte_));
    out.u8(cgAddress_);
    out.boolean(cgHigh_);
    out.u8(cgLatch_);
    out.u8(m7Latch_);
    out.u16(static_cast<std::uint16_t>(m7a_));
    out.u16(static_cast<std::uint16_t>(m7b_));
    out.u16(static_cast<std::uint16_t>(m7c_));
    out.u16(static_cast<std::uint16_t>(m7d_));
    out.u32(static_cast<std::uint32_t>(m7x_));
    out.u32(static_cast<std::uint32_t>(m7y_));
    out.u32(static_cast<std::uint32_t>(m7hofs_));
    out.u32(static_cast<std::uint32_t>(m7vofs_));
    out.u8(m7sel_);
    out.u32(static_cast<std::uint32_t>(m7Product_));
    out.u8(w12sel_);
    out.u8(w34sel_);
    out.u8(wobjsel_);
    out.u8(wlog_);
    out.u8(wobjlog_);
    out.u8(wh0_);
    out.u8(wh1_);
    out.u8(wh2_);
    out.u8(wh3_);
    out.u8(tmw_);
    out.u8(tsw_);
    out.u8(cgwsel_);
    out.u8(cgadsub_);
    out.u8(fixedR_);
    out.u8(fixedG_);
    out.u8(fixedB_);
    out.u8(setini_);
    out.u8(latchedBgmode_);
    out.u8(latchedTm_);
    out.u8(latchedTs_);
    out.u32(static_cast<std::uint32_t>(field_));
    out.bytes(vram_.data(), vram_.size());
    out.bytes(oam_.data(), oam_.size());
    for (const std::uint16_t entry : cgram_) {
        out.u16(entry);
    }
    out.bytes(frame_.data(), frame_.size());
}

bool Ppu::loadState(iris::ByteReader& in) {
    const auto inidisp = in.u8();
    const auto obsel = in.u8();
    const auto bgmode = in.u8();
    const auto bg1sc = in.u8();
    const auto bg2sc = in.u8();
    const auto bg3sc = in.u8();
    const auto bg4sc = in.u8();
    const auto bg12nba = in.u8();
    const auto bg34nba = in.u8();
    std::uint16_t hofs[4] = {};
    std::uint16_t vofs[4] = {};
    for (int layer = 0; layer < 4; ++layer) {
        hofs[layer] = in.u16();
        vofs[layer] = in.u16();
    }
    const auto scrollLatch = in.u8();
    const auto mosaic = in.u8();
    const auto tm = in.u8();
    const auto ts = in.u8();
    const auto vmain = in.u8();
    const auto vramAddress = in.u16();
    const auto oamWord = in.u16();
    const auto oamByte = static_cast<int>(in.u32());
    const auto cgAddress = in.u8();
    const auto cgHigh = in.boolean();
    const auto cgLatch = in.u8();
    const auto m7Latch = in.u8();
    const auto m7a = static_cast<std::int16_t>(in.u16());
    const auto m7b = static_cast<std::int16_t>(in.u16());
    const auto m7c = static_cast<std::int16_t>(in.u16());
    const auto m7d = static_cast<std::int16_t>(in.u16());
    const auto m7x = static_cast<int>(in.u32());
    const auto m7y = static_cast<int>(in.u32());
    const auto m7hofs = static_cast<int>(in.u32());
    const auto m7vofs = static_cast<int>(in.u32());
    const auto m7sel = in.u8();
    const auto m7Product = static_cast<std::int32_t>(in.u32());
    const auto w12sel = in.u8();
    const auto w34sel = in.u8();
    const auto wobjsel = in.u8();
    const auto wlog = in.u8();
    const auto wobjlog = in.u8();
    const auto wh0 = in.u8();
    const auto wh1 = in.u8();
    const auto wh2 = in.u8();
    const auto wh3 = in.u8();
    const auto tmw = in.u8();
    const auto tsw = in.u8();
    const auto cgwsel = in.u8();
    const auto cgadsub = in.u8();
    const auto fixedR = in.u8();
    const auto fixedG = in.u8();
    const auto fixedB = in.u8();
    const auto setini = in.u8();
    const auto latchedBgmode = in.u8();
    const auto latchedTm = in.u8();
    const auto latchedTs = in.u8();
    const auto field = static_cast<int>(in.u32());
    std::vector<std::uint8_t> vram(vram_.size());
    std::vector<std::uint8_t> oam(oam_.size());
    std::vector<std::uint16_t> cgram(cgram_.size());
    std::vector<std::uint8_t> frame(frame_.size());
    in.bytes(vram.data(), vram.size());
    in.bytes(oam.data(), oam.size());
    for (std::uint16_t& entry : cgram) {
        entry = in.u16();
    }
    in.bytes(frame.data(), frame.size());
    if (!in.ok()) {
        return false;
    }
    inidisp_ = inidisp;
    obsel_ = obsel;
    bgmode_ = bgmode;
    bg1sc_ = bg1sc;
    bg2sc_ = bg2sc;
    bg3sc_ = bg3sc;
    bg4sc_ = bg4sc;
    bg12nba_ = bg12nba;
    bg34nba_ = bg34nba;
    for (int layer = 0; layer < 4; ++layer) {
        hofs_[layer] = hofs[layer];
        vofs_[layer] = vofs[layer];
    }
    scrollLatch_ = scrollLatch;
    mosaic_ = mosaic;
    tm_ = tm;
    ts_ = ts;
    vmain_ = vmain;
    vramAddress_ = vramAddress;
    oamWord_ = oamWord;
    oamByte_ = oamByte;
    cgAddress_ = cgAddress;
    cgHigh_ = cgHigh;
    cgLatch_ = cgLatch;
    m7Latch_ = m7Latch;
    m7a_ = m7a;
    m7b_ = m7b;
    m7c_ = m7c;
    m7d_ = m7d;
    m7x_ = m7x;
    m7y_ = m7y;
    m7hofs_ = m7hofs;
    m7vofs_ = m7vofs;
    m7sel_ = m7sel;
    m7Product_ = m7Product;
    w12sel_ = w12sel;
    w34sel_ = w34sel;
    wobjsel_ = wobjsel;
    wlog_ = wlog;
    wobjlog_ = wobjlog;
    wh0_ = wh0;
    wh1_ = wh1;
    wh2_ = wh2;
    wh3_ = wh3;
    tmw_ = tmw;
    tsw_ = tsw;
    cgwsel_ = cgwsel;
    cgadsub_ = cgadsub;
    fixedR_ = fixedR;
    fixedG_ = fixedG;
    fixedB_ = fixedB;
    setini_ = setini;
    latchedBgmode_ = latchedBgmode;
    latchedTm_ = latchedTm;
    latchedTs_ = latchedTs;
    latchedCgwsel_ = cgwsel;
    latchedCgadsub_ = cgadsub;
    field_ = field;
    vram_ = std::move(vram);
    oam_ = std::move(oam);
    cgram_ = std::move(cgram);
    frame_ = std::move(frame);
    return true;
}

}  // namespace iris::systems::snes
