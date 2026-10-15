#include "iris/nes/ppu.hpp"

#include "iris/byte_io.hpp"
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
    if (scanline_ >= scanlines_) {
        scanline_ = 0;
        dot_ = 0;
    }
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
    clearPicturePipeline();
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
    if (scanline_ < kHeight) {
        if (dot_ == 0) {
            beginVisibleLine();
        } else if (dot_ >= 1 && dot_ <= 256) {
            drawVisibleDot(dot_ - 1);
        } else if (dot_ == 257 && !blankLine_) {
            copyHorizontal();
        }
    }
    if ((scanline_ < kHeight || scanline_ == preRender) && dot_ >= 257 && dot_ <= 320) {
        fetchSpritePattern();
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

void Ppu::clearPicturePipeline() {
    background_.fill(0);
    lineSprites_.fill(SpriteSlot{});
    nextSprites_.fill(SpriteSlot{});
    lineSpriteCount_ = 0;
    nextSpriteCount_ = 0;
    walkV_ = 0;
    walkFineX_ = 0;
    tileLow_ = 0;
    tileHigh_ = 0;
    tilePalette_ = 0;
    tileLoaded_ = false;
    blankLine_ = true;
}

void Ppu::beginVisibleLine() {
    background_.fill(0);
    if (!rendering()) {
        blankLine_ = true;
        lineSpriteCount_ = 0;
        nextSpriteCount_ = 0;
        return;
    }
    blankLine_ = false;
    walkV_ = v_;
    walkFineX_ = fineX_;
    tileLoaded_ = false;
    lineSpriteCount_ = nextSpriteCount_;
    lineSprites_ = nextSprites_;
    nextSpriteCount_ = 0;
}

void Ppu::fetchBackgroundTile() {
    const std::uint16_t address = walkV_;
    const std::uint16_t tileAddress = static_cast<std::uint16_t>(0x2000 | (address & 0x0FFF));
    const std::uint8_t tile = readVram(tileAddress);
    const int fineY = (address >> 12) & 0x07;
    const std::uint16_t pattern =
        static_cast<std::uint16_t>(((ctrl_ & 0x10) != 0 ? 0x1000 : 0) + tile * 16 + fineY);
    tileLow_ = readVram(pattern);
    tileHigh_ = readVram(static_cast<std::uint16_t>(pattern + 8));
    const std::uint16_t attributeAddress = static_cast<std::uint16_t>(
        0x23C0 | (address & 0x0C00) | ((address >> 4) & 0x38) | ((address >> 2) & 0x07));
    const int shift = ((address >> 4) & 0x04) | (address & 0x02);
    tilePalette_ = (readVram(attributeAddress) >> shift) & 0x03;
}

void Ppu::compositeSprites(int x) {
    if ((mask_ & 0x10) == 0) {
        return;
    }
    for (int slot = lineSpriteCount_ - 1; slot >= 0; --slot) {
        const SpriteSlot& sprite = lineSprites_[static_cast<std::size_t>(slot)];
        const int column = x - sprite.x;
        if (column < 0 || column > 7 || ((mask_ & 0x04) == 0 && x < 8)) {
            continue;
        }
        const int bit = (sprite.attribute & 0x40) != 0 ? column : 7 - column;
        const int color = ((sprite.high >> bit) & 0x01) << 1 | ((sprite.low >> bit) & 0x01);
        if (color == 0) {
            continue;
        }
        if (sprite.index == 0 && background_[static_cast<std::size_t>(x)] != 0 && x != 255) {
            status_ = static_cast<std::uint8_t>(status_ | 0x40);
        }
        if ((sprite.attribute & 0x20) != 0 && background_[static_cast<std::size_t>(x)] != 0) {
            continue;
        }
        const int paletteEntry = 0x10 + (sprite.attribute & 0x03) * 4 + color;
        writePixel(x, scanline_, static_cast<std::uint8_t>(paletteEntry));
    }
}

void Ppu::drawVisibleDot(int x) {
    if (blankLine_) {
        writePixel(x, scanline_, 0);
        return;
    }
    if (!tileLoaded_) {
        fetchBackgroundTile();
        tileLoaded_ = true;
    }
    const bool showBackground = (mask_ & 0x08) != 0;
    int paletteEntry = 0;
    if (showBackground && ((mask_ & 0x02) != 0 || x >= 8)) {
        const int bit = 7 - (walkFineX_ & 0x07);
        const int color = ((tileHigh_ >> bit) & 0x01) << 1 | ((tileLow_ >> bit) & 0x01);
        background_[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(color);
        paletteEntry = color == 0 ? 0 : tilePalette_ * 4 + color;
    }
    writePixel(x, scanline_, static_cast<std::uint8_t>(paletteEntry));
    compositeSprites(x);
    ++walkFineX_;
    if ((walkFineX_ & 0x07) == 0) {
        v_ = walkV_;
        incrementCoarseX();
        walkV_ = v_;
        tileLoaded_ = false;
    }
    if (x == 255) {
        v_ = walkV_;
        incrementY();
    }
}

void Ppu::selectSprites(int line) {
    nextSpriteCount_ = 0;
    if (line < 0 || line >= kHeight) {
        return;
    }
    const int height = (ctrl_ & 0x20) != 0 ? 16 : 8;
    for (int index = 0; index < 64; ++index) {
        const int spriteY = oam_[static_cast<std::size_t>(index) * 4];
        const int row = line - (spriteY + 1);
        if (row < 0 || row >= height) {
            continue;
        }
        if (nextSpriteCount_ == 8) {
            status_ = static_cast<std::uint8_t>(status_ | 0x20);
            break;
        }
        SpriteSlot& slot = nextSprites_[static_cast<std::size_t>(nextSpriteCount_++)];
        slot.index = index;
        slot.x = oam_[static_cast<std::size_t>(index) * 4 + 3];
        slot.attribute = oam_[static_cast<std::size_t>(index) * 4 + 2];
        int useRow = row;
        if ((slot.attribute & 0x80) != 0) {
            useRow = height - 1 - useRow;
        }
        const std::uint8_t tile = oam_[static_cast<std::size_t>(index) * 4 + 1];
        if (height == 16) {
            const int table = (tile & 0x01) != 0 ? 0x1000 : 0;
            int tileIndex = tile & 0xFE;
            int tileRow = useRow;
            if (tileRow >= 8) {
                ++tileIndex;
                tileRow -= 8;
            }
            slot.pattern = static_cast<std::uint16_t>(table + tileIndex * 16 + tileRow);
        } else {
            const int table = (ctrl_ & 0x08) != 0 ? 0x1000 : 0;
            slot.pattern = static_cast<std::uint16_t>(table + tile * 16 + useRow);
        }
        slot.low = 0;
        slot.high = 0;
    }
}

void Ppu::fetchSpritePattern() {
    if (!rendering()) {
        return;
    }
    const int preRender = scanlines_ - 1;
    if (scanline_ >= kHeight && scanline_ != preRender) {
        return;
    }
    if (dot_ < 257 || dot_ > 320) {
        return;
    }
    const int nextLine = scanline_ == preRender ? 0 : scanline_ + 1;
    if (dot_ == 257) {
        selectSprites(nextLine);
    }
    const int phase = dot_ - 257;
    const int slot = phase >> 3;
    const int step = phase & 7;
    if (slot >= nextSpriteCount_) {
        return;
    }
    SpriteSlot& sprite = nextSprites_[static_cast<std::size_t>(slot)];
    if (step == 4) {
        sprite.low = readVram(sprite.pattern);
    } else if (step == 6) {
        sprite.high = readVram(static_cast<std::uint16_t>(sprite.pattern + 8));
    }
}

void Ppu::captureState(ByteWriter& writer) const {
    writer.u8(ctrl_);
    writer.u8(mask_);
    writer.u8(status_);
    writer.u8(oamAddr_);
    writer.u8(dataLatch_);
    writer.u8(readBuffer_);
    writer.u8(fineX_);
    writer.u16(v_);
    writer.u16(t_);
    writer.boolean(w_);
    writer.u16(static_cast<std::uint16_t>(scanline_));
    writer.u16(static_cast<std::uint16_t>(dot_));
    writer.u16(static_cast<std::uint16_t>(a12LowDots_));
    writer.boolean(a12High_);
    writer.u64(frame_);
    writer.bytes(vram_.data(), vram_.size());
    writer.bytes(palette_.data(), palette_.size());
    writer.bytes(oam_.data(), oam_.size());
    writer.bytes(frameBuffer_.data(), frameBuffer_.size());
}

bool Ppu::applyState(ByteReader& reader) {
    const std::uint8_t ctrl = reader.u8();
    const std::uint8_t mask = reader.u8();
    const std::uint8_t status = reader.u8();
    const std::uint8_t oamAddr = reader.u8();
    const std::uint8_t dataLatch = reader.u8();
    const std::uint8_t readBuffer = reader.u8();
    const std::uint8_t fineX = reader.u8();
    const std::uint16_t v = reader.u16();
    const std::uint16_t t = reader.u16();
    const bool w = reader.boolean();
    const std::uint16_t scanline = reader.u16();
    const std::uint16_t dot = reader.u16();
    const std::uint16_t a12LowDots = reader.u16();
    const bool a12High = reader.boolean();
    const std::uint64_t frame = reader.u64();
    std::array<std::uint8_t, 4096> vram{};
    std::array<std::uint8_t, 32> palette{};
    std::array<std::uint8_t, 256> oam{};
    std::vector<std::uint8_t> frameBuffer(frameBuffer_.size());
    reader.bytes(vram.data(), vram.size());
    reader.bytes(palette.data(), palette.size());
    reader.bytes(oam.data(), oam.size());
    reader.bytes(frameBuffer.data(), frameBuffer.size());
    if (!reader.ok() || !reader.exhausted() || fineX > 7 || scanline >= scanlines_ || dot >= 341 || a12LowDots > 32) {
        return false;
    }
    ctrl_ = ctrl;
    mask_ = mask;
    status_ = status;
    oamAddr_ = oamAddr;
    dataLatch_ = dataLatch;
    readBuffer_ = readBuffer;
    fineX_ = fineX;
    v_ = v;
    t_ = t;
    w_ = w;
    scanline_ = scanline;
    dot_ = dot;
    a12LowDots_ = a12LowDots;
    a12High_ = a12High;
    frame_ = frame;
    vram_ = vram;
    palette_ = palette;
    oam_ = oam;
    frameBuffer_ = std::move(frameBuffer);
    clearPicturePipeline();
    return true;
}

}  // namespace iris::systems::nes
