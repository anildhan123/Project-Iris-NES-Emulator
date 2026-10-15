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
    bg12nba_ = 0;
    tm_ = 0;
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
    w12sel_ = 0;
    wobjsel_ = 0;
    wlog_ = 0;
    wh0_ = 0;
    wh1_ = 0;
    wh2_ = 0;
    wh3_ = 0;
    tmw_ = 0;
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
        m7x_ = m7Latch_ | ((value & 0x1F) << 8);
        m7Latch_ = value;
        break;
    case 0x2120:
        m7y_ = m7Latch_ | ((value & 0x1F) << 8);
        m7Latch_ = value;
        break;
    case 0x2123:
        w12sel_ = value;
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
    case 0x212E:
        tmw_ = value;
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
    case 0x2107:
        bg1sc_ = value;
        break;
    case 0x210B:
        bg12nba_ = value;
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
    default:
        break;
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
    const int vx = ((static_cast<int>(m7a_) * x + static_cast<int>(m7b_) * y) >> 8) + m7x_;
    const int vy = ((static_cast<int>(m7c_) * x + static_cast<int>(m7d_) * y) >> 8) + m7y_;
    if (vx < 0 || vy < 0 || vx >= 1024 || vy >= 1024 || vram_.size() < 2) {
        return -1;
    }
    const std::size_t mapWord = static_cast<std::size_t>((vy >> 3) * 128 + (vx >> 3));
    if (mapWord * 2 >= vram_.size()) {
        return -1;
    }
    const int tile = vram_[mapWord * 2];
    const std::size_t pixelWord = static_cast<std::size_t>(tile * 64 + (vy & 7) * 8 + (vx & 7));
    if (pixelWord * 2 + 1 >= vram_.size()) {
        return -1;
    }
    return vram_[pixelWord * 2 + 1];
}

bool Ppu::layerMasked(int x, std::uint8_t select, bool enabled) const {
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
    switch (wlog_ & 0x03) {
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

std::uint16_t Ppu::applyMath(std::uint16_t color) const {
    if ((cgwsel_ & 0x02) != 0) {
        return color;
    }
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
    const int red = mix(color & 0x1F, fixedR_);
    const int green = mix((color >> 5) & 0x1F, fixedG_);
    const int blue = mix((color >> 10) & 0x1F, fixedB_);
    return static_cast<std::uint16_t>(red | (green << 5) | (blue << 10));
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
    const int bpp = mode == 0 ? 2 : (mode == 3 || mode == 4 ? 8 : 4);
    const bool bg1 = (tm_ & 0x01) != 0;
    const int bytesPerTile = bpp == 2 ? 16 : (bpp == 8 ? 64 : 32);
    const int mapBase = (bg1sc_ & 0xFC) << 8;
    const int charBase = (bg12nba_ & 0x0F) << 12;
    const std::uint16_t backdrop = cgram_.empty() ? 0 : cgram_[0];
    for (int x = 0; x < kWidth; ++x) {
        std::uint16_t color = blank ? 0 : backdrop;
        bool fromBackground = false;
        const bool maskBackground = layerMasked(x, w12sel_, (tmw_ & 0x01) != 0);
        if (!blank && bg1 && !maskBackground && mode7) {
            const int pixel = mode7Pixel(x, y);
            if (pixel > 0 && static_cast<std::size_t>(pixel) < cgram_.size()) {
                color = cgram_[static_cast<std::size_t>(pixel)];
                fromBackground = true;
            }
        } else if (!blank && bg1 && !maskBackground && !mode7) {
            const int tileX = x >> 3;
            const int tileY = y >> 3;
            const int pixelX = x & 7;
            const int pixelY = y & 7;
            const std::size_t mapIndex = static_cast<std::size_t>(mapBase + (tileY * 32 + tileX) * 2);
            if (mapIndex + 1 < vram_.size()) {
                const int tile = vram_[mapIndex] | ((vram_[mapIndex + 1] & 0x03) << 8);
                const int palette = (vram_[mapIndex + 1] >> 2) & 0x07;
                const bool flipX = (vram_[mapIndex + 1] & 0x40) != 0;
                const bool flipY = (vram_[mapIndex + 1] & 0x80) != 0;
                const int row = flipY ? 7 - pixelY : pixelY;
                const int column = flipX ? pixelX : 7 - pixelX;
                const std::size_t tileByte = static_cast<std::size_t>(charBase + tile * bytesPerTile + row * 2);
                const int planeSpan = bpp == 8 ? 49 : (bpp == 4 ? 17 : 1);
                if (tileByte + static_cast<std::size_t>(planeSpan) < vram_.size()) {
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
                    if (pixel != 0) {
                        const std::size_t colorIndex =
                            bpp == 8 ? static_cast<std::size_t>(pixel)
                                     : static_cast<std::size_t>((palette << (bpp == 2 ? 2 : 4)) | pixel);
                        if (colorIndex < cgram_.size()) {
                            color = cgram_[colorIndex];
                            fromBackground = true;
                        }
                    }
                }
            }
        }
        if (!blank && fromBackground && (cgadsub_ & 0x01) != 0) {
            color = applyMath(color);
        } else if (!blank && !fromBackground && (cgadsub_ & 0x20) != 0) {
            color = applyMath(color);
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
    if (!blank && (tm_ & 0x10) != 0) {
        drawSprites(y, level);
    }
}

void Ppu::drawSprites(int y, int level) {
    const int nameBase = (obsel_ & 0x07) << 13;
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
        if (((high >> shift) & 0x01) != 0) {
            continue;
        }
        const int originX = oam_[entry];
        const int originY = oam_[entry + 1];
        const int tile = oam_[entry + 2] | ((oam_[entry + 3] & 0x01) << 8);
        const int palette = (oam_[entry + 3] >> 1) & 0x07;
        const bool flipX = (oam_[entry + 3] & 0x40) != 0;
        const bool flipY = (oam_[entry + 3] & 0x80) != 0;
        for (int py = 0; py < 8; ++py) {
            const int screenY = originY + py;
            if (screenY != y) {
                continue;
            }
            const int row = flipY ? 7 - py : py;
            const std::size_t tileByte = static_cast<std::size_t>(nameBase + tile * 32 + row * 2);
            if (tileByte + 17 >= vram_.size()) {
                continue;
            }
            const int plane0 = vram_[tileByte];
            const int plane1 = vram_[tileByte + 1];
            const int plane2 = vram_[tileByte + 16];
            const int plane3 = vram_[tileByte + 17];
            for (int px = 0; px < 8; ++px) {
                const int screenX = originX + px;
                if (screenX < 0 || screenX >= kWidth || layerMasked(screenX, wobjsel_, (tmw_ & 0x10) != 0)) {
                    continue;
                }
                const int column = flipX ? px : 7 - px;
                const int pixel = ((plane0 >> column) & 1) | (((plane1 >> column) & 1) << 1) |
                                  (((plane2 >> column) & 1) << 2) | (((plane3 >> column) & 1) << 3);
                if (pixel == 0) {
                    continue;
                }
                const std::size_t colorIndex = static_cast<std::size_t>(128 + palette * 16 + pixel);
                if (colorIndex >= cgram_.size()) {
                    continue;
                }
                std::uint16_t color = cgram_[colorIndex];
                if ((cgadsub_ & 0x10) != 0) {
                    color = applyMath(color);
                }
                const std::size_t pixelIndex = static_cast<std::size_t>((screenY * kWidth + screenX) * 4);
                frame_[pixelIndex] = static_cast<std::uint8_t>(brightness(color & 0x1F, level));
                frame_[pixelIndex + 1] = static_cast<std::uint8_t>(brightness((color >> 5) & 0x1F, level));
                frame_[pixelIndex + 2] = static_cast<std::uint8_t>(brightness((color >> 10) & 0x1F, level));
            }
        }
    }
}

}  // namespace iris::systems::snes
