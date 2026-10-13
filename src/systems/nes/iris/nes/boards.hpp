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

}  // namespace iris::systems::nes
