#pragma once

#include "iris/nes/timing.hpp"

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::nes {

class Apu {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kCpuClock = 1789773;

    void reset();
    void setRegion(Region region);
    void write(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t readStatus();
    void clock(std::vector<float>& samples, float expansionSample = 0.0f);
    [[nodiscard]] bool irq() const { return frameIrq_ || dmcIrq_; }
    [[nodiscard]] bool pollDmc(std::uint16_t& address);
    void supplyDmc(std::uint8_t value);
    void captureState(iris::ByteWriter& writer) const;
    [[nodiscard]] bool applyState(iris::ByteReader& reader);

private:
    struct Envelope {
        bool start = false;
        bool loop = false;
        bool constant = false;
        std::uint8_t volume = 0;
        std::uint8_t divider = 0;
        std::uint8_t decay = 0;

        void clock();
        [[nodiscard]] std::uint8_t output() const;
    };

    struct Pulse {
        bool enabled = false;
        bool halt = false;
        std::uint8_t duty = 0;
        std::uint8_t sequence = 0;
        std::uint16_t timer = 0;
        std::uint16_t period = 0;
        std::uint8_t length = 0;
        bool sweepEnabled = false;
        std::uint8_t sweepPeriod = 0;
        std::uint8_t sweepDivider = 0;
        bool sweepNegate = false;
        std::uint8_t sweepShift = 0;
        bool sweepReload = false;
        Envelope envelope;

        void writeControl(std::uint8_t value);
        void writeSweep(std::uint8_t value);
        void writeLow(std::uint8_t value);
        void writeHigh(std::uint8_t value, bool onesComplement);
        void clockTimer();
        void clockLength();
        void clockSweep(bool onesComplement);
        [[nodiscard]] int targetPeriod(bool onesComplement) const;
        [[nodiscard]] std::uint8_t output(bool onesComplement) const;
    };

    struct Triangle {
        bool enabled = false;
        bool halt = false;
        bool linearReload = false;
        std::uint8_t linearPeriod = 0;
        std::uint8_t linear = 0;
        std::uint8_t sequence = 0;
        std::uint16_t timer = 0;
        std::uint16_t period = 0;
        std::uint8_t length = 0;

        void clockTimer();
        void clockLinear();
        void clockLength();
        [[nodiscard]] std::uint8_t output() const;
    };

    struct Noise {
        bool enabled = false;
        bool mode = false;
        std::uint16_t timer = 0;
        std::uint16_t period = 0;
        std::uint16_t shift = 1;
        std::uint8_t length = 0;
        bool halt = false;
        Envelope envelope;

        void clockTimer();
        void clockLength();
        [[nodiscard]] std::uint8_t output() const;
    };

    void clockQuarter();
    void clockHalf();
    void clockFrame();
    void clockDmc();
    [[nodiscard]] float mix() const;
    static void captureEnvelope(ByteWriter& writer, const Envelope& envelope);
    static bool readEnvelope(ByteReader& reader, Envelope& envelope);

    Pulse pulse1_;
    Pulse pulse2_;
    Triangle triangle_;
    Noise noise_;
    int cpuClock_ = kCpuClock;
    const std::uint16_t* noisePeriods_ = nullptr;
    const std::uint16_t* dmcPeriods_ = nullptr;
    const int* frame4_ = nullptr;
    const int* frame5_ = nullptr;
    int frameCycle_ = 0;
    int samplePhase_ = 0;
    int frameResetDelay_ = 0;
    std::uint8_t pendingFrame_ = 0;
    bool evenCycle_ = true;
    bool fiveStep_ = false;
    bool irqInhibit_ = false;
    bool frameIrq_ = false;
    void applyFrameControl(std::uint8_t value);

    bool dmcIrqEnable_ = false;
    bool dmcLoop_ = false;
    bool dmcIrq_ = false;
    bool dmcSilence_ = true;
    bool dmcSampleEmpty_ = true;
    bool dmcFetch_ = false;
    std::uint8_t dmcOutput_ = 0;
    std::uint8_t dmcShift_ = 0;
    std::uint8_t dmcSample_ = 0;
    std::uint8_t dmcBits_ = 8;
    std::uint8_t dmcRate_ = 0;
    std::uint16_t dmcTimer_ = 428;
    std::uint16_t dmcPeriod_ = 428;
    std::uint16_t dmcAddress_ = 0xC000;
    std::uint16_t dmcSampleAddress_ = 0xC000;
    int dmcBytesRemaining_ = 0;
    int dmcSampleLength_ = 1;
};

}  // namespace iris::systems::nes
