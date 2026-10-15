#pragma once

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

// SPC700 and S-DSP. The Nintendo IPL ROM is not present. A transfer uses the
// documented port protocol, and a jump to $FFC0 returns to that protocol.
class Apu {
public:
    static constexpr int kOutputRate = 48000;

    void power();
    void beginFrame();
    void catchUp(int masterCycle, int masterClock);
    void finishAudio(int masterCycles, int masterClock);

    [[nodiscard]] std::uint8_t readPort(int index) const;
    void writePort(int index, std::uint8_t value);
    [[nodiscard]] const float* samples() const { return samples_.data(); }
    [[nodiscard]] int sampleCount() const { return static_cast<int>(samples_.size()); }
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

private:
    struct Voice {
        int pointer = 0;
        int loop = 0;
        int phase = 0;
        int older = 0;
        int previous = 0;
        int envelope = 0;
        int output = 0;
        bool playing = false;
        bool release = false;
        bool blockEnds = false;
        bool blockLoops = false;
        int decoded[16] = {};
    };

    void enterBoot();
    void bootPoll();
    void run(int cycles);
    [[nodiscard]] int instruction();
    void tickTimers(int cycles);
    [[nodiscard]] float renderSample();
    void keyOn(int voice);
    void decodeBlock(Voice& voice);
    [[nodiscard]] std::uint8_t read8(std::uint16_t address);
    void write8(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t readReg(std::uint16_t address);
    void writeReg(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t fetch();
    [[nodiscard]] std::uint16_t fetchWord();
    [[nodiscard]] std::uint16_t readWord(std::uint16_t address);
    [[nodiscard]] std::uint16_t page() const;
    [[nodiscard]] std::uint16_t direct(std::uint8_t offset) const;
    void setZN(std::uint8_t value);
    void add(std::uint8_t& target, std::uint8_t value);
    void compare(std::uint8_t left, std::uint8_t right);
    void branch(bool taken);
    void push(std::uint8_t value);
    [[nodiscard]] std::uint8_t pull();
    void call(std::uint16_t address);

    std::vector<std::uint8_t> ram_;
    std::vector<float> raw_;
    std::vector<float> samples_;
    std::vector<int> echoLeft_;
    std::vector<int> echoRight_;
    Voice voice_[8]{};
    std::uint8_t dsp_[128] = {};
    std::uint8_t cpuIn_[4] = {};
    std::uint8_t cpuOut_[4] = {0xAA, 0xBB, 0x00, 0x00};
    std::uint8_t control_ = 0;
    std::uint8_t dspAddress_ = 0;
    std::uint8_t timerTarget_[3] = {};
    std::uint8_t timerCount_[3] = {};
    std::uint8_t endx_ = 0;
    std::uint8_t keyOn_ = 0;
    std::uint8_t keyOff_ = 0;
    std::uint16_t pc_ = 0;
    std::uint16_t dest_ = 0;
    std::uint8_t a_ = 0;
    std::uint8_t x_ = 0;
    std::uint8_t y_ = 0;
    std::uint8_t sp_ = 0xEF;
    std::uint8_t psw_ = 0;
    std::uint8_t cursor_ = 0;
    std::uint8_t seen_ = 0;
    int clocked_ = 0;
    int samplePhase_ = 0;
    int timerDivider_[3] = {};
    int timerStage_[3] = {};
    int echoPos_ = 0;
    int noise_ = 0x4000;
    std::int64_t cycleAccum_ = 0;
    std::int64_t outputAccum_ = 0;
    bool booting_ = true;
    bool transferring_ = false;
    bool halted_ = false;
};

}  // namespace iris::systems::snes
