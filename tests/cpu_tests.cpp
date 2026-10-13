#include "iris/nes/cpu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace {

class FlatMemory final : public iris::systems::nes::CpuMemory {
public:
    std::array<std::uint8_t, 65536> data{};

    std::uint8_t read(std::uint16_t address) override { return data[address]; }
    void write(std::uint16_t address, std::uint8_t value) override { data[address] = value; }

    void program(std::uint16_t origin, std::initializer_list<std::uint8_t> bytes) {
        std::uint16_t address = origin;
        for (const std::uint8_t byte : bytes) {
            data[address++] = byte;
        }
        data[0xFFFC] = static_cast<std::uint8_t>(origin & 0xFF);
        data[0xFFFD] = static_cast<std::uint8_t>(origin >> 8);
    }
};

}  // namespace

TEST_CASE("official opcodes are implemented") {
    constexpr std::uint8_t kOfficial[] = {
        0x00, 0x01, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0D, 0x0E, 0x10, 0x11, 0x15, 0x16, 0x18, 0x19, 0x1D, 0x1E, 0x20, 0x21,
        0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0x2C, 0x2D, 0x2E, 0x30, 0x31, 0x35, 0x36, 0x38, 0x39, 0x3D, 0x3E, 0x40, 0x41,
        0x45, 0x46, 0x48, 0x49, 0x4A, 0x4C, 0x4D, 0x4E, 0x50, 0x51, 0x55, 0x56, 0x58, 0x59, 0x5D, 0x5E, 0x60, 0x61, 0x65,
        0x66, 0x68, 0x69, 0x6A, 0x6C, 0x6D, 0x6E, 0x70, 0x71, 0x75, 0x76, 0x78, 0x79, 0x7D, 0x7E, 0x81, 0x84, 0x85, 0x86,
        0x88, 0x8A, 0x8C, 0x8D, 0x8E, 0x90, 0x91, 0x94, 0x95, 0x96, 0x98, 0x99, 0x9A, 0x9D, 0xA0, 0xA1, 0xA2, 0xA4, 0xA5,
        0xA6, 0xA8, 0xA9, 0xAA, 0xAC, 0xAD, 0xAE, 0xB0, 0xB1, 0xB4, 0xB5, 0xB6, 0xB8, 0xB9, 0xBA, 0xBC, 0xBD, 0xBE, 0xC0,
        0xC1, 0xC4, 0xC5, 0xC6, 0xC8, 0xC9, 0xCA, 0xCC, 0xCD, 0xCE, 0xD0, 0xD1, 0xD5, 0xD6, 0xD8, 0xD9, 0xDD, 0xDE, 0xE0,
        0xE1, 0xE4, 0xE5, 0xE6, 0xE8, 0xE9, 0xEA, 0xEC, 0xED, 0xEE, 0xF0, 0xF1, 0xF5, 0xF6, 0xF8, 0xF9, 0xFD, 0xFE,
    };
    REQUIRE(sizeof(kOfficial) == 151);
    FlatMemory memory;
    iris::systems::nes::Cpu cpu;
    for (const std::uint8_t opcode : kOfficial) {
        memory.data.fill(0);
        memory.data[0x8000] = opcode;
        memory.data[0xFFFC] = 0x00;
        memory.data[0xFFFD] = 0x80;
        cpu.reset(memory);
        cpu.step(memory);
    }
    REQUIRE(cpu.unknownOpcodes() == 0);
}

TEST_CASE("LDA ADC and STA update the accumulator and flags") {
    FlatMemory memory;
    memory.program(0x8000, {0xA9, 0x50, 0x69, 0x50, 0x8D, 0x00, 0x02});
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    REQUIRE(cpu.step(memory) == 2);
    REQUIRE(cpu.step(memory) == 2);
    const auto state = cpu.state();
    REQUIRE(state.a == 0xA0);
    REQUIRE((state.p & 0x40) != 0);
    REQUIRE((state.p & 0x80) != 0);
    REQUIRE((state.p & 0x01) == 0);
    cpu.step(memory);
    REQUIRE(memory.data[0x0200] == 0xA0);
}

TEST_CASE("decimal mode does not change ADC") {
    FlatMemory memory;
    memory.program(0x8000, {0xF8, 0xA9, 0x09, 0x69, 0x01});
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    cpu.step(memory);
    cpu.step(memory);
    cpu.step(memory);
    REQUIRE(cpu.state().a == 0x0A);
    REQUIRE((cpu.state().p & 0x08) != 0);
}

TEST_CASE("a taken branch across a page costs four cycles") {
    FlatMemory memory;
    memory.program(0x80F0, {0xA9, 0x00, 0xF0, 0x0C});
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    REQUIRE(cpu.step(memory) == 2);
    REQUIRE(cpu.step(memory) == 4);
    REQUIRE(cpu.state().pc == 0x8100);
}

TEST_CASE("JSR and RTS return to the following instruction") {
    FlatMemory memory;
    memory.program(0x8000, {0x20, 0x00, 0x90, 0x8D, 0x20, 0x00});
    memory.data[0x9000] = 0xA9;
    memory.data[0x9001] = 0x42;
    memory.data[0x9002] = 0x60;
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    cpu.step(memory);
    cpu.step(memory);
    cpu.step(memory);
    cpu.step(memory);
    REQUIRE(memory.data[0x0020] == 0x42);
    REQUIRE(cpu.state().a == 0x42);
}

TEST_CASE("indirect JMP wraps inside the pointer page") {
    FlatMemory memory;
    memory.program(0x8000, {0x6C, 0xFF, 0x10});
    memory.data[0x10FF] = 0x34;
    memory.data[0x1000] = 0x12;
    memory.data[0x1100] = 0x99;
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    cpu.step(memory);
    REQUIRE(cpu.state().pc == 0x1234);
}

TEST_CASE("NMI runs before the next instruction and RTI returns") {
    FlatMemory memory;
    memory.program(0x8000, {0xE6, 0x10, 0x4C, 0x00, 0x80});
    memory.data[0x9000] = 0xE6;
    memory.data[0x9001] = 0x11;
    memory.data[0x9002] = 0x40;
    memory.data[0xFFFA] = 0x00;
    memory.data[0xFFFB] = 0x90;
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    cpu.signalNmi();
    cpu.step(memory);
    cpu.step(memory);
    cpu.step(memory);
    REQUIRE(memory.data[0x0011] == 1);
    REQUIRE(memory.data[0x0010] == 0);
    REQUIRE(cpu.state().pc == 0x8000);
}

TEST_CASE("IRQ waits until the instruction after CLI") {
    FlatMemory memory;
    memory.program(0x8000, {0x58, 0xEA, 0xE6, 0x10});
    memory.data[0x9000] = 0xE6;
    memory.data[0x9001] = 0x11;
    memory.data[0x9002] = 0x40;
    memory.data[0xFFFE] = 0x00;
    memory.data[0xFFFF] = 0x90;
    iris::systems::nes::Cpu cpu;
    cpu.reset(memory);
    cpu.setIrqLine(true);
    cpu.step(memory);
    cpu.step(memory);
    REQUIRE(cpu.state().pc == 0x8002);
    cpu.step(memory);
    REQUIRE(cpu.state().pc == 0x9000);
    REQUIRE(memory.data[0x0010] == 0);
}

TEST_CASE("unofficial opcodes are implemented") {
    FlatMemory memory;
    iris::systems::nes::Cpu cpu;
    for (int opcode = 0; opcode < 256; ++opcode) {
        memory.data.fill(0);
        memory.data[0x8000] = static_cast<std::uint8_t>(opcode);
        memory.data[0xFFFC] = 0x00;
        memory.data[0xFFFD] = 0x80;
        cpu.reset(memory);
        cpu.step(memory);
    }
    REQUIRE(cpu.unknownOpcodes() == 0);

    memory.program(0x8000, {0xA7, 0x10});
    memory.data[0x0010] = 0x3C;
    cpu.reset(memory);
    cpu.step(memory);
    REQUIRE(cpu.state().a == 0x3C);
    REQUIRE(cpu.state().x == 0x3C);

    memory.program(0x8000, {0xA9, 0x0F, 0xA2, 0xF0, 0x87, 0x10});
    cpu.reset(memory);
    cpu.step(memory);
    cpu.step(memory);
    cpu.step(memory);
    REQUIRE(memory.data[0x0010] == 0x00);
}
