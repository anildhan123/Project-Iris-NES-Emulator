#include "iris/nes/ppu.hpp"

#include "iris/nes/palette.hpp"

#include <algorithm>

namespace iris::systems::nes {
namespace {

constexpr int kDotsPerScanline = 341;

}  // namespace

void Ppu::connect(Mapper* mapper, Mirroring mirroring) {
    mapper_ = mapper;
    mirroring_ = mirroring;
}

void Ppu::setTiming(int scanlines, bool oddFrameSkip) {
    scanlines_ = scanlines < 262 ? 262 : scanlines;
    oddFrameSkip_ = oddFrameSkip;
}

Mirroring Ppu::activeMirroring() const { return mapper_ != nullptr ? mapper_->mirroring() : mirroring_; }

void Ppu::power() {
    vram_.fill(0);
    palette_.fill(0);
    oam_.fill(0);
    std::fill(frameBuffer_.begin(), frameBuffer_.end(), static_cast<std::uint8_t>(0));
    scanline_ = 0;
    dot_ = 0;
    frame_ = 0;
    a12High_ = false;
    a12LowDots_ = 0;
    v_ = 0;
    t_ = 0;
    oamAddr_ = 0;
    reset();
}

void Ppu::reset() {
    ctrl_ = 0;
    mask_ = 0;
    status_ = 0;
    w_ = false;
    fineX_ = 0;
    dataLatch_ = 0;
    readBuffer_ = 0;
}

void Ppu::noteA12(bool high) {
    if (high) {
        if (!a12High_ && a12LowDots_ >= 8 && mapper_ != nullptr) {
            mapper_->onA12Rise();
        }
        a12LowDots_ = 0;
        a12High_ = true;
        return;
    }
    if (a12LowDots_ < 32) {
        ++a12LowDots_;
    }
    a12High_ = false;
}

bool Ppu::renderingA12() const {
    const int preRender = scanlines_ - 1;
    const bool fetchedLine = scanline_ < kHeight || scanline_ == preRender;
    if (!rendering() || !fetchedLine) {
        return false;
    }
    if (dot_ >= 257 && dot_ <= 320) {
        if (((dot_ - 257) & 7) < 4) {
            return false;
        }
        if ((ctrl_ & 0x20) != 0) {
            const int sprite = (dot_ - 257) >> 3;
            return (oam_[static_cast<std::size_t>(sprite) * 4 + 1] & 0x01) != 0;
        }
        return (ctrl_ & 0x08) != 0;
    }
    if ((dot_ >= 1 && dot_ <= 256) || (dot_ >= 321 && dot_ <= 336)) {
        if (((dot_ - 1) & 7) < 4) {
            return false;
        }
        return (ctrl_ & 0x10) != 0;
    }
    return false;
}

void Ppu::clock() {
    const int preRender = scanlines_ - 1;
    if (scanline_ == 241 && dot_ == 1) {
        status_ = static_cast<std::uint8_t>(status_ | 0x80);
    }
    if (scanline_ == preRender && dot_ == 1) {
        status_ = static_cast<std::uint8_t>(status_ & ~0xE0);
    }
    if (scanline_ < kHeight && dot_ == 0) {
        renderScanline(scanline_);
    }
    if (scanline_ == preRender && dot_ == 280 && rendering()) {
        copyVertical();
    }
    noteA12(renderingA12());

    ++dot_;
    if (oddFrameSkip_ && scanline_ == preRender && dot_ == 340 && rendering() && (frame_ & 1U) == 1U) {
        dot_ = kDotsPerScanline;
    }
    if (dot_ == kDotsPerScanline) {
        dot_ = 0;
        ++scanline_;
        if (scanline_ == scanlines_) {
            scanline_ = 0;
            ++frame_;
        }
    }
}

bool Ppu::nmiLine() const { return (status_ & 0x80) != 0 && (ctrl_ & 0x80) != 0; }

void Ppu::writeRegister(std::uint16_t address, std::uint8_t value) {
    dataLatch_ = value;
    switch (address & 0x07) {
    case 0:
        ctrl_ = value;
        t_ = static_cast<std::uint16_t>((t_ & ~0x0C00) | ((value & 0x03) << 10));
        break;
    case 1:
        mask_ = value;
        break;
    case 3:
        oamAddr_ = value;
        break;
    case 4:
        writeOam(value);
        break;
    case 5:
        if (!w_) {
            fineX_ = static_cast<std::uint8_t>(value & 0x07);
            t_ = static_cast<std::uint16_t>((t_ & ~0x001F) | (value >> 3));
            w_ = true;
        } else {
            t_ = static_cast<std::uint16_t>((t_ & ~0x73E0) | ((value & 0x07) << 12) | ((value & 0xF8) << 2));
            w_ = false;
        }
        break;
    case 6:
        if (!w_) {
            t_ = static_cast<std::uint16_t>((t_ & 0x00FF) | ((value & 0x3F) << 8));
            w_ = true;
        } else {
            t_ = static_cast<std::uint16_t>((t_ & 0xFF00) | value);
            v_ = t_;
            w_ = false;
        }
        break;
    case 7:
        writeData(value);
        break;
    default:
        break;
    }
}

std::uint8_t Ppu::readRegister(std::uint16_t address) {
    switch (address & 0x07) {
    case 2: {
        const std::uint8_t value = static_cast<std::uint8_t>((status_ & 0xE0) | (dataLatch_ & 0x1F));
        status_ = static_cast<std::uint8_t>(status_ & ~0x80);
        w_ = false;
        dataLatch_ = value;
        return value;
    }
    case 4:
        dataLatch_ = oam_[oamAddr_];
        return dataLatch_;
    case 7:
        dataLatch_ = readData();
        return dataLatch_;
    default:
        return dataLatch_;
    }
}

void Ppu::writeOam(std::uint8_t value) { oam_[oamAddr_++] = value; }

void Ppu::writeData(std::uint8_t value) {
    writeVram(v_, value);
    v_ = static_cast<std::uint16_t>((v_ + ((ctrl_ & 0x04) != 0 ? 32 : 1)) & 0x7FFF);
}

std::uint8_t Ppu::readData() {
    const std::uint16_t address = static_cast<std::uint16_t>(v_ & 0x3FFF);
    std::uint8_t value = 0;
    if (address >= 0x3F00) {
        value = readVram(address);
        readBuffer_ = readVram(static_cast<std::uint16_t>(address - 0x1000));
    } else {
        value = readBuffer_;
        readBuffer_ = readVram(address);
    }
    v_ = static_cast<std::uint16_t>((v_ + ((ctrl_ & 0x04) != 0 ? 32 : 1)) & 0x7FFF);
    return value;
}

std::uint8_t Ppu::readVram(std::uint16_t address) {
    address = static_cast<std::uint16_t>(address & 0x3FFF);
    if (address < 0x2000) {
        return mapper_ == nullptr ? 0 : mapper_->readChr(address);
    }
    if (address < 0x3F00) {
        return vram_[nametableIndex(address)];
    }
    address = static_cast<std::uint16_t>(address & 0x1F);
    if ((address & 0x13) == 0x10) {
        address = static_cast<std::uint16_t>(address & ~0x10);
    }
    return palette_[address];
}

void Ppu::writeVram(std::uint16_t address, std::uint8_t value) {
    address = static_cast<std::uint16_t>(address & 0x3FFF);
    if (address < 0x2000) {
        if (mapper_ != nullptr) {
            mapper_->writeChr(address, value);
        }
        return;
    }
    if (address < 0x3F00) {
        vram_[nametableIndex(address)] = value;
        return;
    }
    address = static_cast<std::uint16_t>(address & 0x1F);
    if ((address & 0x13) == 0x10) {
        address = static_cast<std::uint16_t>(address & ~0x10);
    }
    palette_[address] = static_cast<std::uint8_t>(value & 0x3F);
}

std::uint16_t Ppu::nametableIndex(std::uint16_t address) const {
    const int offset = address & 0x03FF;
    int table = (address & 0x0FFF) >> 10;
    switch (activeMirroring()) {
    case Mirroring::Horizontal:
        table = table < 2 ? 0 : 1;
        break;
    case Mirroring::Vertical:
        table &= 1;
        break;
    case Mirroring::SingleScreenLower:
        table = 0;
        break;
    case Mirroring::SingleScreenUpper:
        table = 1;
        break;
    case Mirroring::FourScreen:
        break;
    }
    return static_cast<std::uint16_t>(table * 0x400 + offset);
}

void Ppu::incrementCoarseX() {
    if ((v_ & 0x001F) == 31) {
        v_ = static_cast<std::uint16_t>(v_ & ~0x001F);
        v_ = static_cast<std::uint16_t>(v_ ^ 0x0400);
    } else {
        v_ = static_cast<std::uint16_t>(v_ + 1);
    }
}

void Ppu::incrementY() {
    if ((v_ & 0x7000) != 0x7000) {
        v_ = static_cast<std::uint16_t>(v_ + 0x1000);
        return;
    }
    v_ = static_cast<std::uint16_t>(v_ & ~0x7000);
    int coarseY = (v_ & 0x03E0) >> 5;
    if (coarseY == 29) {
        coarseY = 0;
        v_ = static_cast<std::uint16_t>(v_ ^ 0x0800);
    } else if (coarseY == 31) {
        coarseY = 0;
    } else {
        ++coarseY;
    }
    v_ = static_cast<std::uint16_t>((v_ & ~0x03E0) | (coarseY << 5));
}

void Ppu::copyHorizontal() { v_ = static_cast<std::uint16_t>((v_ & ~0x041F) | (t_ & 0x041F)); }

void Ppu::copyVertical() { v_ = static_cast<std::uint16_t>((v_ & ~0x7BE0) | (t_ & 0x7BE0)); }

void Ppu::writePixel(int x, int y, std::uint8_t paletteIndex) {
    std::uint8_t index = palette_[paletteIndex & 0x1F];
    if ((paletteIndex & 0x03) == 0) {
        index = palette_[0];
    }
    if ((mask_ & 0x01) != 0) {
        index = static_cast<std::uint8_t>(index & 0x30);
    }
    Rgb color = paletteColor(index);
    if ((mask_ & 0x20) != 0) {
        color.green = static_cast<std::uint8_t>(color.green * 3 / 4);
        color.blue = static_cast<std::uint8_t>(color.blue * 3 / 4);
    }
    if ((mask_ & 0x40) != 0) {
        color.red = static_cast<std::uint8_t>(color.red * 3 / 4);
        color.blue = static_cast<std::uint8_t>(color.blue * 3 / 4);
    }
    if ((mask_ & 0x80) != 0) {
        color.red = static_cast<std::uint8_t>(color.red * 3 / 4);
        color.green = static_cast<std::uint8_t>(color.green * 3 / 4);
    }
    const std::size_t offset = (static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)) * 4;
    frameBuffer_[offset] = color.red;
    frameBuffer_[offset + 1] = color.green;
    frameBuffer_[offset + 2] = color.blue;
    frameBuffer_[offset + 3] = 255;
}

void Ppu::renderScanline(int scanline) {
    std::uint8_t background[kWidth] = {};
    if (!rendering()) {
        for (int x = 0; x < kWidth; ++x) {
            writePixel(x, scanline, 0);
        }
        return;
    }

    const bool showBackground = (mask_ & 0x08) != 0;
    const bool showSprites = (mask_ & 0x10) != 0;
    std::uint16_t address = v_;
    int fineX = fineX_;
    for (int x = 0; x < kWidth; ++x) {
        int paletteEntry = 0;
        if (showBackground && ((mask_ & 0x02) != 0 || x >= 8)) {
            const std::uint16_t tileAddress = static_cast<std::uint16_t>(0x2000 | (address & 0x0FFF));
            const std::uint8_t tile = readVram(tileAddress);
            const int fineY = (address >> 12) & 0x07;
            const std::uint16_t pattern = static_cast<std::uint16_t>(((ctrl_ & 0x10) != 0 ? 0x1000 : 0) + tile * 16 + fineY);
            const std::uint8_t low = readVram(pattern);
            const std::uint8_t high = readVram(static_cast<std::uint16_t>(pattern + 8));
            const int bit = 7 - (fineX & 0x07);
            const int color = ((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01);
            const std::uint16_t attributeAddress = static_cast<std::uint16_t>(
                0x23C0 | (address & 0x0C00) | ((address >> 4) & 0x38) | ((address >> 2) & 0x07));
            const int shift = ((address >> 4) & 0x04) | (address & 0x02);
            const int palette = (readVram(attributeAddress) >> shift) & 0x03;
            background[x] = static_cast<std::uint8_t>(color);
            paletteEntry = color == 0 ? 0 : palette * 4 + color;
        }
        writePixel(x, scanline, static_cast<std::uint8_t>(paletteEntry));
        ++fineX;
        if ((fineX & 0x07) == 0) {
            v_ = address;
            incrementCoarseX();
            address = v_;
        }
    }

    if (showSprites) {
        const int height = (ctrl_ & 0x20) != 0 ? 16 : 8;
        int visible[8] = {};
        int count = 0;
        for (int index = 0; index < 64; ++index) {
            const int spriteY = oam_[index * 4];
            const int row = scanline - (spriteY + 1);
            if (row < 0 || row >= height) {
                continue;
            }
            if (count == 8) {
                status_ = static_cast<std::uint8_t>(status_ | 0x20);
                break;
            }
            visible[count++] = index;
        }
        for (int slot = count - 1; slot >= 0; --slot) {
            const int index = visible[slot];
            const int spriteY = oam_[index * 4];
            int row = scanline - (spriteY + 1);
            const std::uint8_t tile = oam_[index * 4 + 1];
            const std::uint8_t attribute = oam_[index * 4 + 2];
            const int spriteX = oam_[index * 4 + 3];
            if ((attribute & 0x80) != 0) {
                row = height - 1 - row;
            }
            std::uint16_t pattern = 0;
            if (height == 16) {
                const int table = (tile & 0x01) != 0 ? 0x1000 : 0;
                int tileIndex = tile & 0xFE;
                int tileRow = row;
                if (tileRow >= 8) {
                    ++tileIndex;
                    tileRow -= 8;
                }
                pattern = static_cast<std::uint16_t>(table + tileIndex * 16 + tileRow);
            } else {
                const int table = (ctrl_ & 0x08) != 0 ? 0x1000 : 0;
                pattern = static_cast<std::uint16_t>(table + tile * 16 + row);
            }
            const std::uint8_t low = readVram(pattern);
            const std::uint8_t high = readVram(static_cast<std::uint16_t>(pattern + 8));
            for (int column = 0; column < 8; ++column) {
                const int x = spriteX + column;
                if (x > 255 || ((mask_ & 0x04) == 0 && x < 8)) {
                    continue;
                }
                const int bit = (attribute & 0x40) != 0 ? column : 7 - column;
                const int color = ((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01);
                if (color == 0) {
                    continue;
                }
                if (index == 0 && background[x] != 0 && x != 255) {
                    status_ = static_cast<std::uint8_t>(status_ | 0x40);
                }
                if ((attribute & 0x20) != 0 && background[x] != 0) {
                    continue;
                }
                const int paletteEntry = 0x10 + (attribute & 0x03) * 4 + color;
                writePixel(x, scanline, static_cast<std::uint8_t>(paletteEntry));
            }
        }
    }

    v_ = address;
    incrementY();
    copyHorizontal();
}

}  // namespace iris::systems::nes
