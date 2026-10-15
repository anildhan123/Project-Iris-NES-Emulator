#include "iris/snes/demo_rom.hpp"
#include "iris/snes/snes_system.hpp"
#include "iris/system_kind.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> loRom(const std::vector<std::uint8_t>& program) {
    std::vector<std::uint8_t> rom(32 * 1024, 0);
    for (std::size_t index = 0; index < program.size() && index < rom.size(); ++index) {
        rom[index] = program[index];
    }
    rom[0x7FD5] = 0x20;
    rom[0x7FD7] = 0x05;
    rom[0x7FFC] = 0x00;
    rom[0x7FFD] = 0x80;
    return rom;
}

void store(std::vector<std::uint8_t>& program, std::uint8_t value, std::uint8_t low, std::uint8_t high) {
    program.insert(program.end(), {0xA9, value, 0x8D, low, high});
}

std::vector<std::uint8_t> backgroundProgram(
    std::uint8_t mode, std::uint8_t attribute, bool window, std::uint8_t colorMath, std::uint8_t fixedColor) {
    std::vector<std::uint8_t> program;
    store(program, 0x80, 0x15, 0x21);
    store(program, 0x00, 0x16, 0x21);
    store(program, 0x08, 0x17, 0x21);
    store(program, 0xFF, 0x18, 0x21);
    store(program, 0x00, 0x19, 0x21);
    store(program, 0x00, 0x16, 0x21);
    store(program, 0x00, 0x17, 0x21);
    store(program, 0x00, 0x18, 0x21);
    store(program, attribute, 0x19, 0x21);
    store(program, 0x00, 0x18, 0x21);
    store(program, attribute, 0x19, 0x21);
    store(program, 0x00, 0x21, 0x21);
    store(program, 0x1F, 0x22, 0x21);
    store(program, 0x00, 0x22, 0x21);
    store(program, 0x01, 0x21, 0x21);
    store(program, 0x00, 0x22, 0x21);
    store(program, 0x7C, 0x22, 0x21);
    store(program, 0x11, 0x21, 0x21);
    store(program, 0x1F, 0x22, 0x21);
    store(program, 0x00, 0x22, 0x21);
    store(program, 0x05, 0x21, 0x21);
    store(program, 0xE0, 0x22, 0x21);
    store(program, 0x03, 0x22, 0x21);
    store(program, 0x01, 0x0B, 0x21);
    store(program, mode, 0x05, 0x21);
    store(program, 0x01, 0x2C, 0x21);
    if (window) {
        store(program, 0x00, 0x26, 0x21);
        store(program, 0x07, 0x27, 0x21);
        store(program, 0x02, 0x23, 0x21);
        store(program, 0x01, 0x2E, 0x21);
    }
    if (colorMath != 0) {
        store(program, colorMath, 0x31, 0x21);
        store(program, fixedColor, 0x32, 0x21);
    }
    store(program, 0x0F, 0x00, 0x21);
    const auto loop = static_cast<std::uint8_t>(program.size());
    program.insert(program.end(), {0x4C, loop, 0x80});
    return program;
}

}  // namespace

TEST_CASE("LoROM and HiROM images report their titles") {
    const std::vector<std::uint8_t> demo = iris::systems::snes::makeIrisSnesDemo();
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(demo.data(), demo.size(), error));
    REQUIRE(system.info().title == "IRIS SNES");
    REQUIRE(system.info().detail == "LoROM, NTSC");

    std::vector<std::uint8_t> headered(512, 0);
    headered.insert(headered.end(), demo.begin(), demo.end());
    REQUIRE(system.loadBytes(headered.data(), headered.size(), error));
    REQUIRE(system.info().title == "IRIS SNES");

    std::vector<std::uint8_t> hiRom = demo;
    hiRom[0x7FD5] = 0x21;
    REQUIRE_FALSE(system.loadBytes(hiRom.data(), hiRom.size(), error));
    REQUIRE(error.find("HiROM") != std::string::npos);

    std::vector<std::uint8_t> image(64 * 1024, 0);
    image[0x8000] = 0xA9;
    image[0x8001] = 0x56;
    image[0x8002] = 0x85;
    image[0x8003] = 0x20;
    image[0x8004] = 0x4C;
    image[0x8005] = 0x04;
    image[0x8006] = 0x80;
    const std::string hiTitle = "HIROM TEST";
    for (std::size_t index = 0; index < hiTitle.size(); ++index) {
        image[0xFFC0 + index] = static_cast<std::uint8_t>(hiTitle[index]);
    }
    image[0xFFD5] = 0x21;
    image[0xFFD9] = 0x01;
    image[0xFFDC] = 0xFF;
    image[0xFFDD] = 0xFF;
    image[0xFFFC] = 0x00;
    image[0xFFFD] = 0x80;
    REQUIRE(system.loadBytes(image.data(), image.size(), error));
    REQUIRE(system.info().title == "HIROM TEST");
    REQUIRE(system.info().detail == "HiROM, NTSC");
    system.runFrame();
    REQUIRE(system.debugRead(0x0020) == 0x56);
}

TEST_CASE("the Super NES demo moves its marker with the controller") {
    iris::systems::snes::SnesSystem system;
    const std::vector<std::uint8_t> demo = iris::systems::snes::makeIrisSnesDemo();
    std::string error;
    REQUIRE(system.loadBytes(demo.data(), demo.size(), error));
    system.runFrame();
    system.runFrame();
    const iris::FrameView frame = system.frame();
    REQUIRE(frame.width == 256);
    REQUIRE(frame.height == 224);
    REQUIRE(system.info().detail == "LoROM, NTSC");
    const auto at = [&](int x, int y) { return static_cast<std::size_t>((y * frame.width + x) * 4); };
    const std::size_t marker = at(124, 108);
    const std::size_t beside = at(128, 104);
    REQUIRE(frame.rgba[marker] == 255);
    REQUIRE(frame.rgba[marker + 1] == 197);
    REQUIRE(frame.rgba[marker + 2] == 32);
    REQUIRE(frame.rgba[beside] != frame.rgba[marker]);

    system.setController(0, iris::kButtonRight);
    system.runFrame();
    const iris::FrameView moved = system.frame();
    REQUIRE(moved.rgba != nullptr);
    REQUIRE(moved.rgba[beside] == frame.rgba[marker]);
    REQUIRE(moved.rgba[beside + 1] == frame.rgba[marker + 1]);
    REQUIRE(moved.rgba[beside + 2] == frame.rgba[marker + 2]);

    const auto started = std::chrono::steady_clock::now();
    for (int frameIndex = 0; frameIndex < 8; ++frameIndex) {
        system.runFrame();
    }
    REQUIRE(std::chrono::steady_clock::now() - started < std::chrono::seconds(2));
    REQUIRE(system.frame().rgba != nullptr);
    REQUIRE(system.info().detail == "LoROM, NTSC");
}

TEST_CASE("Super NES work RAM survives reset and a missing instruction stops the CPU") {
    const std::vector<std::uint8_t> program = {
        0xE6, 0x10,  // INC $10
        0x4C, 0x02, 0x80,  // JMP $8002
    };
    const std::vector<std::uint8_t> rom = loRom(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 1);
    system.reset();
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 2);
    system.powerCycle();
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 1);

    const std::vector<std::uint8_t> exchange = {
        0xFB,              // XCE
        0xC2, 0x20,        // REP #$20
        0xA9, 0x34, 0x12,  // LDA #$1234
        0x8D, 0x00, 0x00,  // STA $0000
        0x4C, 0x09, 0x80,  // JMP $8009
    };
    const std::vector<std::uint8_t> native = loRom(exchange);
    REQUIRE(system.loadBytes(native.data(), native.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0000) == 0x34);
    REQUIRE(system.debugRead(0x0001) == 0x12);
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
}

TEST_CASE("DMA copies bytes into work RAM and a block move does too") {
    const std::vector<std::uint8_t> dma = {
        0xA9, 0x11, 0x85, 0x10, 0xA9, 0x22, 0x85, 0x11, 0xA9, 0x00, 0x8D, 0x81, 0x21, 0x8D, 0x82, 0x21,
        0x8D, 0x83, 0x21, 0x8D, 0x00, 0x43, 0xA9, 0x80, 0x8D, 0x01, 0x43, 0xA9, 0x10, 0x8D, 0x02, 0x43,
        0xA9, 0x00, 0x8D, 0x03, 0x43, 0x8D, 0x04, 0x43, 0xA9, 0x02, 0x8D, 0x05, 0x43, 0xA9, 0x00, 0x8D,
        0x06, 0x43, 0xA9, 0x01, 0x8D, 0x0B, 0x42, 0x4C, 0x37, 0x80,
    };
    const std::vector<std::uint8_t> dmaRom = loRom(dma);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(dmaRom.data(), dmaRom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0000) == 0x11);
    REQUIRE(system.debugRead(0x0001) == 0x22);
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);

    const std::vector<std::uint8_t> block = {
        0xA9, 0x99, 0x85, 0x30, 0xA9, 0x00, 0xA2, 0x30, 0xA0, 0x40, 0x54, 0x00, 0x00, 0x4C, 0x0D, 0x80,
    };
    const std::vector<std::uint8_t> blockRom = loRom(block);
    REQUIRE(system.loadBytes(blockRom.data(), blockRom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0040) == 0x99);
}

TEST_CASE("mode 7 draws the transformed character pixel") {
    const std::vector<std::uint8_t> program = {
        0xA9, 0x00, 0x8D, 0x16, 0x21, 0xA9, 0x00, 0x8D, 0x17, 0x21, 0xA9, 0x80, 0x8D, 0x15, 0x21, 0xA9, 0x00, 0x8D,
        0x18, 0x21, 0xA9, 0x01, 0x8D, 0x19, 0x21, 0xA9, 0x00, 0x8D, 0x21, 0x21, 0xA9, 0x00, 0x8D, 0x22, 0x21, 0xA9,
        0x00, 0x8D, 0x22, 0x21, 0xA9, 0x01, 0x8D, 0x21, 0x21, 0xA9, 0x1F, 0x8D, 0x22, 0x21, 0xA9, 0x00, 0x8D, 0x22,
        0x21, 0xA9, 0x00, 0x8D, 0x1B, 0x21, 0xA9, 0x01, 0x8D, 0x1B, 0x21, 0xA9, 0x00, 0x8D, 0x1E, 0x21, 0xA9, 0x01,
        0x8D, 0x1E, 0x21, 0xA9, 0x07, 0x8D, 0x05, 0x21, 0xA9, 0x01, 0x8D, 0x2C, 0x21, 0xA9, 0x0F, 0x8D, 0x00, 0x21,
        0x4C, 0x00, 0x80,
    };
    const std::vector<std::uint8_t> rom = loRom(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    const iris::FrameView frame = system.frame();
    REQUIRE(frame.rgba != nullptr);
    REQUIRE(frame.rgba[0] == 255);
    REQUIRE(frame.rgba[1] == 0);
    REQUIRE(frame.rgba[2] == 0);
}

TEST_CASE("HDMA changes brightness on the next displayed line") {
    const std::vector<std::uint8_t> program = {
        0xA9, 0x00, 0x8D, 0x21, 0x21, 0xA9, 0x1F, 0x8D, 0x22, 0x21, 0xA9, 0x00, 0x8D, 0x22, 0x21, 0xA9, 0x01, 0x85,
        0x10, 0xA9, 0x0F, 0x85, 0x11, 0xA9, 0x01, 0x85, 0x12, 0xA9, 0x00, 0x85, 0x13, 0xA9, 0x00, 0x85, 0x14, 0xA9,
        0x00, 0x8D, 0x00, 0x43, 0x8D, 0x01, 0x43, 0xA9, 0x10, 0x8D, 0x02, 0x43, 0xA9, 0x00, 0x8D, 0x03, 0x43, 0x8D,
        0x04, 0x43, 0xA9, 0x01, 0x8D, 0x0C, 0x42, 0x4C, 0x3D, 0x80,
    };
    const std::vector<std::uint8_t> rom = loRom(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    const iris::FrameView frame = system.frame();
    REQUIRE(frame.rgba != nullptr);
    REQUIRE(frame.rgba[0] == 255);
    REQUIRE(frame.rgba[1] == 0);
    REQUIRE(frame.rgba[2] == 0);
    const std::size_t nextLine = static_cast<std::size_t>(256 * 4);
    REQUIRE(frame.rgba[nextLine] == 0);
    REQUIRE(frame.rgba[nextLine + 1] == 0);
    REQUIRE(frame.rgba[nextLine + 2] == 0);
}

TEST_CASE("windows, color math, and background modes 2 through 5 draw distinct pixels") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    const auto run = [&](const std::vector<std::uint8_t>& program) {
        const std::vector<std::uint8_t> rom = loRom(program);
        REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
        system.runFrame();
        REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
        return system.frame();
    };

    const iris::FrameView masked = run(backgroundProgram(0, 0, true, 0, 0));
    REQUIRE(masked.rgba[0] == 255);
    REQUIRE(masked.rgba[1] == 0);
    REQUIRE(masked.rgba[2] == 0);
    REQUIRE(masked.rgba[32] == 0);
    REQUIRE(masked.rgba[33] == 0);
    REQUIRE(masked.rgba[34] == 255);

    const iris::FrameView added = run(backgroundProgram(0, 0, false, 0x01, 0x5F));
    REQUIRE(added.rgba[0] == 0);
    REQUIRE(added.rgba[1] == 255);
    REQUIRE(added.rgba[2] == 255);

    const iris::FrameView mode2 = run(backgroundProgram(2, 0x04, false, 0, 0));
    REQUIRE(mode2.rgba[0] == 255);
    REQUIRE(mode2.rgba[1] == 0);
    REQUIRE(mode2.rgba[2] == 0);

    const iris::FrameView mode3 = run(backgroundProgram(3, 0x04, false, 0, 0));
    REQUIRE(mode3.rgba[0] == 0);
    REQUIRE(mode3.rgba[1] == 0);
    REQUIRE(mode3.rgba[2] == 255);

    const iris::FrameView mode5 = run(backgroundProgram(5, 0x04, false, 0, 0));
    REQUIRE(mode5.rgba[0] == 255);
    REQUIRE(mode5.rgba[2] == 0);
}

TEST_CASE("indexed instructions, vblank, multiply, and save RAM keep a board running") {
    const std::vector<std::uint8_t> program = {
        0xA9, 0x34, 0x85, 0x20, 0xA2, 0x20, 0xB5, 0x00, 0x85, 0x13, 0xA9, 0x06, 0x8D, 0x02, 0x42, 0xA9, 0x07, 0x8D,
        0x03, 0x42, 0xAD, 0x16, 0x42, 0x85, 0x12, 0xA9, 0xAB, 0x8F, 0x00, 0x00, 0x70, 0xAD, 0x12, 0x42, 0x10, 0xFB,
        0xA9, 0x5A, 0x85, 0x11, 0x4C, 0x26, 0x80,
    };
    std::vector<std::uint8_t> rom = loRom(program);
    rom[0x7FD8] = 1;
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    REQUIRE(system.info().batteryBacked);
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x0013) == 0x34);
    REQUIRE(system.debugRead(0x0012) == 42);
    REQUIRE(system.debugRead(0x0011) == 0x5A);
    REQUIRE(system.debugRead(0x700000) == 0xAB);
    system.reset();
    REQUIRE(system.debugRead(0x700000) == 0xAB);
}

TEST_CASE("a Super NES path selects that system and a NES path does not") {
    REQUIRE(iris::systemKindForPath("game.sfc") == iris::SystemKind::Snes);
    REQUIRE(iris::systemKindForPath("game.smc") == iris::SystemKind::Snes);
    REQUIRE(iris::systemKindForPath("game.nes") == iris::SystemKind::Nes);
    REQUIRE(std::string(iris::systemKindName(iris::SystemKind::Snes)) == "Super NES");
}
