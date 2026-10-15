#pragma once

#include <cstdint>
#include <vector>

namespace iris {
class ByteReader;
class ByteWriter;
}

namespace iris::systems::snes {

// Super FX (GSU). The program runs when the GO flag is set. Plot writes one
// byte per pixel at SCBR * 1024 + Y * 256 + X. The pixel cache, packed 2/4bpp
// screens, and the remaining opcodes are not in this build.
class Gsu {
public:
    void power();
    void halt();
    void bind(const std::vector<std::uint8_t>* rom);
    void run(int budget);
    [[nodiscard]] bool running() const { return (sfr_ & 0x20) != 0; }
    [[nodiscard]] bool cpuRead(std::uint8_t bank, std::uint16_t address, std::uint8_t& value) const;
    [[nodiscard]] bool cpuWrite(std::uint8_t bank, std::uint16_t address, std::uint8_t value);
    void saveState(iris::ByteWriter& out) const;
    [[nodiscard]] bool loadState(iris::ByteReader& in);

private:
    [[nodiscard]] std::uint8_t fetch();
    [[nodiscard]] std::uint8_t memory(std::uint8_t bank, std::uint16_t address) const;
    void memoryWrite(std::uint8_t bank, std::uint16_t address, std::uint8_t value);
    void ramWrite(std::uint32_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t ramRead(std::uint32_t address) const;
    void setAddFlags(std::uint16_t left, std::uint16_t right, std::uint32_t sum);
    void finishPrefix();

    const std::vector<std::uint8_t>* rom_ = nullptr;
    std::vector<std::uint8_t> ram_;
    std::uint16_t r_[16] = {};
    std::uint16_t sfr_ = 0;
    std::uint8_t pbr_ = 0;
    std::uint8_t rombr_ = 0;
    std::uint8_t ramb_ = 0;
    std::uint8_t scbr_ = 0;
    std::uint8_t color_ = 0;
    std::uint8_t source_ = 0;
    std::uint8_t dest_ = 0;
    bool alt1_ = false;
    bool useDest_ = false;
    bool useSource_ = false;
};

}  // namespace iris::systems::snes
