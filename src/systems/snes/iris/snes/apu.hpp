#pragma once

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

// SPC700 and S-DSP. The upload protocol is implemented in software by default.
// A 64-byte IPL image the user supplies can be mapped at $FFC0 and executed.
// Nintendo's IPL dump is not shipped with Iris.
class Apu {
public:
    static constexpr int kOutputRate = 48000;
    static constexpr int kIplSize = 64;

    void power();
    void beginFrame();
    void catchUp(int masterCycle, int masterClock);
    void finishAudio(int masterCycles, int masterClock);
    // Run the software IPL handshake after a CPU instruction so 16-bit port stores
    // are visible as a pair. Nintendo firmware is not required for this path.
    void pollBoot();

    // Install a 64-byte IPL image. Empty clears the image and returns to the
    // software protocol. Nintendo firmware must not be redistributed with Iris.
    [[nodiscard]] bool installIpl(const std::uint8_t* data, std::size_t size);
    [[nodiscard]] bool hasIpl() const { return hasIpl_; }

    [[nodiscard]] std::uint8_t readPort(int index) const;
    void writePort(int index, std::uint8_t value);
    [[nodiscard]] const float* samples() const { return samples_.data(); }
    // Number of host frames. Each frame has channels() interleaved floats.
    [[nodiscard]] int sampleCount() const { return static_cast<int>(samples_.size()) / channels(); }
    [[nodiscard]] int channels() const { return 2; }
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

private:
    enum class EnvState : std::uint8_t {
        Attack,
        Decay,
        Sustain,
        Release,
    };

    struct Voice {
        int pointer = 0;
        int loop = 0;
        int phase = 0;
        int older = 0;
        int previous = 0;
        int envelope = 0;
        int output = 0;
        int rateCounter = 0;
        int brrPos = 0;
        int konDelay = 0;
        int history[4] = {};
        EnvState envState = EnvState::Attack;
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
    void serviceIrq();
    void renderSample(float& left, float& right);
    void keyOn(int voice);
    void tickEnvelope(Voice& voice, int index);
    [[nodiscard]] bool envelopeClock(Voice& voice, int rate);
    [[nodiscard]] bool rateFires(int& counter, int rate);
    void decodeBlock(Voice& voice);
    [[nodiscard]] int nextBrrSample(Voice& voice, int index);
    [[nodiscard]] int interpolate(const Voice& voice) const;
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
    std::vector<float> rawLeft_;
    std::vector<float> rawRight_;
    std::vector<float> samples_;
    std::vector<int> echoLeft_;
    std::vector<int> echoRight_;
    std::uint8_t ipl_[kIplSize] = {};
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
    int echoDelay_ = 1;
    int noise_ = 0x4000;
    int noiseCounter_ = 0;
    std::int64_t cycleAccum_ = 0;
    std::int64_t outputAccum_ = 0;
    bool hasIpl_ = false;
    bool booting_ = true;
    bool transferring_ = false;
    bool halted_ = false;
    bool sleeping_ = false;
    bool irqPending_ = false;
};

}  // namespace iris::systems::snes
