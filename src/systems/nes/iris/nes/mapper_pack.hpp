#pragma once

#include "iris/nes/cartridge.hpp"

#include <array>
#include <cstdint>

namespace iris::systems::nes {

// Milestone 49–55 mapper pack. Banking and IRQ paths are implemented so boards
// load and run; full silicon for every edge is not claimed.

class Mmc5 final : public Mapper {
public:
    explicit Mmc5(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    void reset() override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    std::uint8_t prgMode_ = 3;
    std::uint8_t chrMode_ = 0;
    std::array<std::uint8_t, 5> prg_{};
    std::array<std::uint8_t, 12> chr_{};
    std::uint8_t mirror_ = 0;
    std::uint8_t mulA_ = 0xFF;
    std::uint8_t mulB_ = 0xFF;
    int irqTarget_ = 0;
    int irqCounter_ = 0;
    int irqScan_ = 0;
    int irqDot_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    std::array<std::uint8_t, 1024> exram_{};
};

class Vrc2 final : public Mapper {
public:
    explicit Vrc2(Cartridge& cartridge, bool swapA0A1);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    bool swap_ = false;
    int prg_[2] = {};
    int chr_[8] = {};
    int mirror_ = 0;
};

class Vrc3 final : public Mapper {
public:
    explicit Vrc3(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int irqLatch_ = 0;
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
};

class Vrc4 final : public Mapper {
public:
    explicit Vrc4(Cartridge& cartridge, bool swapA0A1);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    bool swap_ = false;
    int prg_[2] = {};
    int chr_[8] = {};
    int mirror_ = 0;
    int irqLatch_ = 0;
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    int irqPrescale_ = 341;
};

class Vrc6 final : public Mapper {
public:
    explicit Vrc6(Cartridge& cartridge, bool swapA0A1);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    [[nodiscard]] float expansionSample() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    bool swap_ = false;
    int prg16_ = 0;
    int prg8_ = 0;
    int chr_[8] = {};
    int mirror_ = 0;
    int irqLatch_ = 0;
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    std::uint8_t pulse_[2] = {};
    std::uint16_t pulsePeriod_[2] = {};
    std::uint16_t pulseTimer_[2] = {};
    std::uint8_t saw_ = 0;
    std::uint16_t sawPeriod_ = 0;
    std::uint16_t sawTimer_ = 0;
    int sawAccum_ = 0;
};

class Vrc7 final : public Mapper {
public:
    explicit Vrc7(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    [[nodiscard]] float expansionSample() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_[3] = {};
    int chr_[8] = {};
    int mirror_ = 0;
    int irqLatch_ = 0;
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    std::uint8_t fmReg_ = 0;
    std::uint8_t fmData_[64] = {};
    float fmLevel_ = 0.0f;
};

class Fds final : public Mapper {
public:
    explicit Fds(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return Mirroring::Horizontal; }
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    [[nodiscard]] float expansionSample() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int irqTimer_ = 0;
    int irqReload_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    std::array<std::uint8_t, 64> wave_{};
    int wavePos_ = 0;
    int waveTimer_ = 0;
    int wavePeriod_ = 0;
    std::uint8_t waveVol_ = 0;
    bool waveEnable_ = false;
    int diskPtr_ = 0;
};

class Namco163 final : public Mapper {
public:
    explicit Namco163(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    [[nodiscard]] float expansionSample() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_[3] = {};
    int chr_[8] = {};
    int mirror_ = 0;
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    std::uint8_t soundRam_[128] = {};
    std::uint8_t soundAddr_ = 0;
    bool soundAuto_ = false;
    float soundLevel_ = 0.0f;
};

class Namco210 final : public Mapper {
public:
    explicit Namco210(Cartridge& cartridge);
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
    int chr_[8] = {};
    int mirror_ = 0;
};

class Sunsoft3 final : public Mapper {
public:
    explicit Sunsoft3(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    int prg_ = 0;
    int chr_[4] = {};
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
};

class Sunsoft4 final : public Mapper {
public:
    explicit Sunsoft4(Cartridge& cartridge);
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
    int chr_[2] = {};
    int name_[2] = {};
    int mirror_ = 0;
};

class Fme7 final : public Mapper {
public:
    explicit Fme7(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void cpuCycle() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    [[nodiscard]] float expansionSample() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    std::uint8_t command_ = 0;
    int prg_[4] = {};
    int chr_[8] = {};
    int mirror_ = 0;
    int irqCounter_ = 0;
    bool irqEnable_ = false;
    bool irqPending_ = false;
    std::uint8_t ay_[16] = {};
    float ayLevel_ = 0.0f;
};

// Remaining widely used boards (milestone 55).
class Cprom final : public Mapper {
public:
    explicit Cprom(Cartridge& cartridge) : cartridge_(cartridge) {}
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

class Mapper76 final : public Mapper {
public:
    explicit Mapper76(Cartridge& cartridge);
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

class Mapper88 final : public Mapper {
public:
    explicit Mapper88(Cartridge& cartridge);
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

class Mapper95 final : public Mapper {
public:
    explicit Mapper95(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    std::uint8_t banks_[8] = {};
    std::uint8_t select_ = 0;
};

class Mapper118 final : public Mapper {
public:
    explicit Mapper118(Cartridge& cartridge);
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override;
    void onA12Rise() override;
    [[nodiscard]] bool irq() const override { return irqPending_; }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    std::uint8_t banks_[8] = {};
    std::uint8_t select_ = 0;
    bool prgMode_ = false;
    bool chrMode_ = false;
    int irqLatch_ = 0;
    int irqCounter_ = 0;
    bool irqReload_ = false;
    bool irqEnable_ = false;
    bool irqPending_ = false;
};

class Mapper180 final : public Mapper {
public:
    explicit Mapper180(Cartridge& cartridge) : cartridge_(cartridge) {}
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

class Mapper185 final : public Mapper {
public:
    explicit Mapper185(Cartridge& cartridge) : cartridge_(cartridge) {}
    [[nodiscard]] std::uint8_t readCpu(std::uint16_t address, std::uint8_t openBus) override;
    void writeCpu(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] std::uint8_t readChr(std::uint16_t address) override;
    void writeChr(std::uint16_t address, std::uint8_t value) override;
    [[nodiscard]] Mirroring mirroring() const override { return cartridge_.mirroring(); }
    void captureState(iris::ByteWriter& writer) const override;
    [[nodiscard]] bool applyState(iris::ByteReader& reader) override;

private:
    Cartridge& cartridge_;
    bool chrEnable_ = true;
};

class Mapper228 final : public Mapper {
public:
    explicit Mapper228(Cartridge& cartridge) : cartridge_(cartridge) {}
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
    int chr_ = 0;
    bool vertical_ = false;
};

}  // namespace iris::systems::nes
