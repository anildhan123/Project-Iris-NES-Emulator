#pragma once

#include "iris/nes/cartridge.hpp"

namespace iris::systems::nes {

class Axrom final : public Mapper {
public:
    explicit Axrom(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int bank_ = 0;
    bool upper_ = false;
};

class ColorDreams final : public Mapper {
public:
    explicit ColorDreams(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int chr_ = 0;
};

class Bnrom final : public Mapper {
public:
    explicit Bnrom(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int bank_ = 0;
};

class Gxrom final : public Mapper {
public:
    explicit Gxrom(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int chr_ = 0;
};

class Camerica final : public Mapper {
public:
    explicit Camerica(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int bank_ = 0;
    bool upper_ = false;
};

// MMC2 / PxROM (Punch-Out!!). CHR banks latch when the PPU reads the last tiles.
class Mmc2 final : public Mapper {
public:
    explicit Mmc2(Cartridge& cartridge);

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int chrLow_[2] = {};
    int chrHigh_[2] = {};
    int latchLow_ = 1;
    int latchHigh_ = 1;
    bool vertical_ = false;
};

// NINA-003 / NINA-006 (mapper 79). One register selects PRG and CHR.
class Nina003 final : public Mapper {
public:
    explicit Nina003(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int chr_ = 0;
};

// MMC4 / FxROM (mapper 10). Like MMC2 with 16 KiB PRG banks.
class Mmc4 final : public Mapper {
public:
    explicit Mmc4(Cartridge& cartridge);

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int chrLow_[2] = {};
    int chrHigh_[2] = {};
    int latchLow_ = 1;
    int latchHigh_ = 1;
    bool vertical_ = false;
};

// Jaleco JF-13 / mapper 87. CHR bank from bits 0–1 swapped.
class Mapper87 final : public Mapper {
public:
    explicit Mapper87(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int chr_ = 0;
};

// DxROM / Namcot 108 (mapper 206). MMC3 without IRQ or mirroring control.
class Mapper206 final : public Mapper {
public:
    explicit Mapper206(Cartridge& cartridge);

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    std::uint8_t banks_[8] = {};
    std::uint8_t select_ = 0;
};

// BF9096 / Camerica Quattro (mapper 232). Outer and inner 16 KiB banks.
class Mapper232 final : public Mapper {
public:
    explicit Mapper232(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int outer_ = 0;
    int inner_ = 0;
};

// Konami VRC1 (mapper 75).
class Vrc1 final : public Mapper {
public:
    explicit Vrc1(Cartridge& cartridge) : cartridge_(cartridge) {}

    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_[3] = {};
    int chr_[2] = {};
    bool vertical_ = false;
};

}  // namespace iris::systems::nes
