#pragma once

#include <cstdint>
#include <vector>

namespace iris::systems::nes {

class Apu {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kCpuClock = 1789773;

    void reset();
    void write(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t readStatus();
    void clock(std::vector<float>& samples);

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
    [[nodiscard]] float mix() const;

    Pulse pulse1_;
    Pulse pulse2_;
    Triangle triangle_;
    Noise noise_;
    int frameDivider_ = 0;
    int samplePhase_ = 0;
    bool fiveStep_ = false;
};

}  // namespace iris::systems::nes
