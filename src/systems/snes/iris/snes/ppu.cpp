#include "iris/snes/ppu.hpp"

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

void Ppu::power() {
    vram_.assign(64 * 1024, 0);
    oam_.assign(544, 0);
    cgram_.assign(256, 0);
    frame_.assign(static_cast<std::size_t>(kWidth * kHeight * 4), 0);
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
    m7a_ = 0;
    m7b_ = 0;
    m7c_ = 0;
    m7d_ = 0;
    m7x_ = 0;
    m7y_ = 0;
    m7hofs_ = 0;
    m7vofs_ = 0;
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
}

std::uint8_t Ppu::read(std::uint16_t address) {
    if (address == 0x213F) {
        return 0x01;
    }
    if (address == 0x2139 || address == 0x213A) {
        const std::size_t byte = static_cast<std::size_t>(vramAddress_ & 0x7FFF) * 2 + (address == 0x213A ? 1 : 0);
        if (byte >= vram_.size()) {
            return 0;
        }
        const std::uint8_t value = vram_[byte];
        if ((address == 0x213A) == ((vmain_ & 0x80) != 0)) {
            vramAddress_ = static_cast<std::uint16_t>((vramAddress_ + 1) & 0x7FFF);
        }
        return value;
    }
    return 0;
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
    case 0x211B:
        writeMode7(m7a_, value);
        break;
    case 0x211C:
        writeMode7(m7b_, value);
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
    const int orgX = x + m7hofs_ - m7x_;
    const int orgY = y + m7vofs_ - m7y_;
    const int vx = ((static_cast<int>(m7a_) * orgX + static_cast<int>(m7b_) * orgY) >> 8) + m7x_;
    const int vy = ((static_cast<int>(m7c_) * orgX + static_cast<int>(m7d_) * orgY) >> 8) + m7y_;
    if (vx < 0 || vy < 0 || vx >= 1024 || vy >= 1024 || vram_.size() < 2) {
        return -1;
    }
    const std::size_t mapWordIndex = static_cast<std::size_t>((vy >> 3) * 128 + (vx >> 3));
    if (mapWordIndex * 2 >= vram_.size()) {
        return -1;
    }
    const int tile = vram_[mapWordIndex * 2];
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

std::uint16_t Ppu::applyMath(std::uint16_t color, int addRed, int addGreen, int addBlue) const {
    const bool subtract = (cgadsub_ & 0x80) != 0;
    const bool half = (cgadsub_ & 0x40) != 0;
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
    static constexpr Slot kModes[] = {
        {4, 3}, {0, 1}, {1, 1}, {4, 2}, {0, 0}, {1, 0}, {4, 1}, {4, 0},
    };
    static constexpr Slot kMode7[] = {
        {4, 3}, {4, 2}, {0, 0}, {4, 1}, {4, 0},
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
    } else if (mode == 7) {
        table = kMode7;
        count = 5;
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
    if ((mode == 2 || mode == 4 || mode == 6) && layer <= 1 && !(mode == 6 && layer != 0)) {
        const int column = x >> 3;
        const int row = static_cast<int>(vofs_[2]) >> 3;
        const int table = static_cast<int>(hofs_[2]) >> 3;
        const auto applyOffset = [&](int word, int pixel, int& sample) {
            if ((word & 0x2000) == 0) {
                return;
            }
            if (mode != 6 && ((word & 0x8000) != 0) != (layer == 1)) {
                return;
            }
            sample = pixel + (word & 0x03FF);
        };
        applyOffset(mapWord(bg3sc_, table + column * 2, row), x, sampleX);
        applyOffset(mapWord(bg3sc_, table + column * 2 + 1, row), y, sampleY);
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

void Ppu::renderLine(int y) {
    if (y < 0 || y >= kHeight) {
        return;
    }
    const int level = inidisp_ & 0x0F;
    const bool blank = (inidisp_ & 0x80) != 0 || level == 0;
    const int mode = bgmode_ & 0x07;
    const bool mode7 = mode == 7;
    const std::uint16_t backdrop = cgram_.empty() ? 0 : cgram_[0];
    Texel sprites[kWidth] = {};
    if (!blank && ((tm_ | ts_) & 0x10) != 0) {
        collectSprites(y, sprites);
    }
    for (int x = 0; x < kWidth; ++x) {
        struct Hit {
            bool opaque = false;
            int index = 0;
            int mathBit = 0;
            int rank = 100;
        };
        const auto consider = [&](Hit& best, int index, int source, int priority, int mathBit) {
            if (index <= 0) {
                return;
            }
            const int rank = stackRank(mode, source, priority);
            if (rank < 0) {
                return;
            }
            if (!best.opaque || rank < best.rank) {
                best.opaque = true;
                best.index = index;
                best.mathBit = mathBit;
                best.rank = rank;
            }
        };
        const auto sampleScreen = [&](std::uint8_t enable, std::uint8_t windowMask) {
            Hit best;
            if (mode7 && (enable & 0x01) != 0 && !layerMasked(x, w12sel_, (windowMask & 0x01) != 0, wlog_ & 0x03)) {
                consider(best, mode7Pixel(x, y), 0, 0, 0x01);
            } else if (!mode7) {
                for (int layer = 3; layer >= 0; --layer) {
                    if ((enable & (1 << layer)) == 0) {
                        continue;
                    }
                    const Texel texel = sampleLayer(layer, x, y, windowMask);
                    consider(best, texel.index, layer, texel.priority, 1 << layer);
                }
            }
            if ((enable & 0x10) != 0 &&
                !layerMasked(x, wobjsel_, (windowMask & 0x10) != 0, static_cast<std::uint8_t>(wobjlog_ & 0x03))) {
                consider(best, sprites[x].index, 4, sprites[x].priority, 0x10);
            }
            return best;
        };
        const Hit main = blank ? Hit{} : sampleScreen(tm_, tmw_);
        const Hit sub = blank ? Hit{} : sampleScreen(ts_, tsw_);
        std::uint16_t color = blank ? 0 : backdrop;
        int mathBit = 0;
        if (main.opaque && static_cast<std::size_t>(main.index) < cgram_.size()) {
            color = cgram_[static_cast<std::size_t>(main.index)];
            mathBit = main.mathBit;
        }
        const bool mathBackdrop = !main.opaque && (cgadsub_ & 0x20) != 0;
        if (!blank && (mathBackdrop || (mathBit != 0 && (cgadsub_ & mathBit) != 0))) {
            int addRed = fixedR_;
            int addGreen = fixedG_;
            int addBlue = fixedB_;
            if ((cgwsel_ & 0x02) != 0) {
                addRed = 0;
                addGreen = 0;
                addBlue = 0;
                if (sub.opaque && static_cast<std::size_t>(sub.index) < cgram_.size()) {
                    const std::uint16_t behind = cgram_[static_cast<std::size_t>(sub.index)];
                    addRed = behind & 0x1F;
                    addGreen = (behind >> 5) & 0x1F;
                    addBlue = (behind >> 10) & 0x1F;
                }
            }
            color = applyMath(color, addRed, addGreen, addBlue);
        }
        const int red = brightness(color & 0x1F, blank ? 0 : level);
        const int green = brightness((color >> 5) & 0x1F, blank ? 0 : level);
        const int blue = brightness((color >> 10) & 0x1F, blank ? 0 : level);
        const std::size_t pixelIndex = static_cast<std::size_t>((y * kWidth + x) * 4);
        frame_[pixelIndex] = static_cast<std::uint8_t>(red);
        frame_[pixelIndex + 1] = static_cast<std::uint8_t>(green);
        frame_[pixelIndex + 2] = static_cast<std::uint8_t>(blue);
        frame_[pixelIndex + 3] = 255;
    }
}

void Ppu::collectSprites(int y, Texel* dots) const {
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
        const int originX = oam_[entry] | (((high >> shift) & 0x01) != 0 ? 0x100 : 0);
        const int originY = oam_[entry + 1];
        const int tile = oam_[entry + 2] | ((oam_[entry + 3] & 0x01) << 8);
        const int palette = (oam_[entry + 3] >> 1) & 0x07;
        const int priority = (oam_[entry + 3] >> 4) & 0x03;
        const bool flipX = (oam_[entry + 3] & 0x40) != 0;
        const bool flipY = (oam_[entry + 3] & 0x80) != 0;
        for (int py = 0; py < spriteHeight; ++py) {
            const int screenY = originY + py;
            if (screenY != y) {
                continue;
            }
            const int localY = flipY ? spriteHeight - 1 - py : py;
            const int row = localY & 7;
            const int tileRow = localY >> 3;
            for (int px = 0; px < spriteWidth; ++px) {
                const int screenX = (originX + px) & 0x1FF;
                if (screenX >= kWidth) {
                    continue;
                }
                const int localX = flipX ? spriteWidth - 1 - px : px;
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
}

}  // namespace iris::systems::snes
