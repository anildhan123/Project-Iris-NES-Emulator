#include "iris/snes/demo_rom.hpp"
#include "iris/snes/snes_system.hpp"
#include "iris/system_kind.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
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
    std::uint8_t mode, std::uint8_t attribute, bool window, std::uint8_t colorMath, std::uint8_t fixedColor,
    std::uint8_t tileBits = 0xFF, std::uint8_t mosaic = 0) {
    std::vector<std::uint8_t> program;
    store(program, 0x80, 0x15, 0x21);
    store(program, 0x00, 0x16, 0x21);
    store(program, 0x10, 0x17, 0x21);
    store(program, tileBits, 0x18, 0x21);
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
    if (mosaic != 0) {
        store(program, mosaic, 0x06, 0x21);
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

    std::vector<std::uint8_t> upper(64 * 1024, 0);
    const std::uint8_t upperCode[] = {0xA9, 0x5A, 0x8F, 0x10, 0x00, 0x7E, 0xAF, 0x10, 0x00, 0xFE, 0x85, 0x20, 0x4C, 0x0C, 0x80};
    for (std::size_t index = 0; index < sizeof(upperCode); ++index) {
        upper[0x8000 + index] = upperCode[index];
    }
    upper[0xFFD5] = 0x21;
    upper[0xFFD9] = 0x01;
    upper[0xFFDC] = 0xFF;
    upper[0xFFDD] = 0xFF;
    upper[0xFFFC] = 0x00;
    upper[0xFFFD] = 0x80;
    REQUIRE(system.loadBytes(upper.data(), upper.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0020) == 0x00);
}

TEST_CASE("a Super NES controller shifts B first and then the direction pad") {
    // Strobe, release, then sixteen reads. B is first. Right is the eighth bit.
    const std::vector<std::uint8_t> program = {
        0x78, 0xA9, 0x01, 0x8D, 0x16, 0x40, 0x9C, 0x16, 0x40, 0x64, 0x20, 0x64, 0x21, 0xA2, 0x10, 0xAD, 0x16,
        0x40, 0x4A, 0x26, 0x21, 0x26, 0x20, 0xCA, 0xD0, 0xF5, 0xDB,
    };
    const std::vector<std::uint8_t> rom = loRom(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.setController(0, static_cast<std::uint8_t>(iris::kButtonB | iris::kButtonRight));
    system.runFrame();
    REQUIRE(system.debugRead(0x0020) == 0x81);
    REQUIRE(system.debugRead(0x0021) == 0x00);
}

TEST_CASE("automatic joypad read is busy at vblank and then reports the direction pad") {
    const std::vector<std::uint8_t> program = {
        0x78, 0xA9, 0x01, 0x8D, 0x00, 0x42, 0x64, 0x10, 0x64, 0x11, 0x64, 0x12, 0xAD, 0x12, 0x42, 0x10, 0xFB,
        0xAD, 0x12, 0x42, 0x29, 0x01, 0xF0, 0x04, 0xA9, 0x01, 0x85, 0x10, 0xAD, 0x12, 0x42, 0x29, 0x01, 0xD0,
        0xF9, 0xAD, 0x18, 0x42, 0x85, 0x11, 0xAD, 0x19, 0x42, 0x85, 0x12, 0xDB,
    };
    const std::vector<std::uint8_t> rom = loRom(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.setController(0, static_cast<std::uint8_t>(iris::kButtonB | iris::kButtonUp));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x01);
    REQUIRE(system.debugRead(0x0011) == 0x00);
    REQUIRE(system.debugRead(0x0012) == 0x88);
}

TEST_CASE("port 2 auto-read and manual shift report a second controller") {
    // Enable auto-read, wait for busy to clear, copy JOY2 into WRAM.
    const std::vector<std::uint8_t> program = {
        0x78, 0xA9, 0x01, 0x8D, 0x00, 0x42, 0xAD, 0x12, 0x42, 0x10, 0xFB, 0xAD, 0x12, 0x42, 0x29, 0x01,
        0xD0, 0xF9, 0xAD, 0x1A, 0x42, 0x85, 0x20, 0xAD, 0x1B, 0x42, 0x85, 0x21, 0xAD, 0x1C, 0x42, 0x85,
        0x22, 0xDB,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(program);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.setController(1, static_cast<std::uint8_t>(iris::kButtonStart | iris::kButtonA));
    system.runFrame();
    REQUIRE(system.debugRead(0x0020) == 0x80);  // A on JOY2L
    REQUIRE(system.debugRead(0x0021) == 0x50);  // Y+Start on JOY2H (A also holds Y)
    REQUIRE(system.debugRead(0x0022) == 0x00);  // JOY3 stays 0 without a multitap

    // Manual $4017 shift: strobe, release, first bit is B.
    const std::vector<std::uint8_t> manual = {
        0x78, 0xA9, 0x01, 0x8D, 0x16, 0x40, 0x9C, 0x16, 0x40, 0xAD, 0x17, 0x40, 0x85, 0x30, 0xDB,
    };
    REQUIRE(system.loadBytes(loRom(manual).data(), loRom(manual).size(), error));
    system.setController(1, iris::kButtonB);
    system.runFrame();
    REQUIRE((system.debugRead(0x0030) & 0x01) == 0x01);
    REQUIRE((system.debugRead(0x0030) & 0x1C) == 0x1C);
}

TEST_CASE("horizontal blank is reported and then clears on the next scanline") {
    // Wait until $4212 bit 6 is set, then until it clears. A game that writes
    // video memory outside vblank, including Final Fantasy VI, spins on this bit.
    const std::vector<std::uint8_t> program = {
        0x78, 0x64, 0x10, 0xAD, 0x12, 0x42, 0x29, 0x40, 0xF0, 0xF9, 0xAD, 0x12, 0x42, 0x29, 0x40, 0xD0,
        0xF9, 0xA9, 0x01, 0x85, 0x10, 0xDB,
    };
    const std::vector<std::uint8_t> rom = loRom(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x01);
}

TEST_CASE("overscan delays the $4212 vblank flag") {
    const auto countUntilVblank = [](bool overscan) {
        std::vector<std::uint8_t> program;
        if (overscan) {
            store(program, 0x04, 0x33, 0x21);
        }
        // 16-bit counter in $10/$11 until $4212 bit 7.
        program.insert(program.end(),
                       {0xA9, 0x00, 0x85, 0x10, 0x85, 0x11, 0xAD, 0x12, 0x42, 0x30, 0x08, 0xE6, 0x10, 0xD0, 0xF7, 0xE6,
                        0x11, 0x80, 0xF3, 0xDB});
        iris::systems::snes::SnesSystem system;
        std::string error;
        const std::vector<std::uint8_t> rom = loRom(program);
        REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
        system.runFrame();
        return system.debugRead(0x0010) | (system.debugRead(0x0011) << 8);
    };
    const int normal = countUntilVblank(false);
    const int tall = countUntilVblank(true);
    REQUIRE(normal > 1000);
    REQUIRE(tall > normal);
}

TEST_CASE("a Super NES controller reports Start on the high byte and A on the low byte") {
    iris::systems::snes::SnesSystem system;
    system.setController(0, static_cast<std::uint8_t>(iris::kButtonStart | iris::kButtonA | iris::kButtonRight));
    // A is the low byte. The same key also holds Y, which is how a game runs or rolls.
    REQUIRE(system.debugRead(0x4219) == 0x51);
    REQUIRE(system.debugRead(0x4218) == 0x80);
    system.setController(0, iris::kButtonA);
    REQUIRE(system.debugRead(0x4219) == 0x40);
    REQUIRE(system.debugRead(0x4218) == 0x80);
    system.setController(0, iris::kButtonB);
    REQUIRE(system.debugRead(0x4219) == 0x80);
    REQUIRE(system.debugRead(0x4218) == 0x00);
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

TEST_CASE("Mode 7 M7SEL flip, EXTBG, and M7 multiply results") {
    // Pixel $81 (EXTBG high + color 1), identity matrix, CGRAM[1]=red.
    auto mode7Setup = [](std::uint8_t pixel, std::uint8_t tm, std::uint8_t setini, std::uint8_t m7sel) {
        std::vector<std::uint8_t> program;
        store(program, 0x00, 0x16, 0x21);
        store(program, 0x00, 0x17, 0x21);
        store(program, 0x80, 0x15, 0x21);
        store(program, 0x00, 0x18, 0x21);
        store(program, pixel, 0x19, 0x21);
        store(program, 0x00, 0x21, 0x21);
        store(program, 0x00, 0x22, 0x21);
        store(program, 0x00, 0x22, 0x21);
        store(program, 0x01, 0x21, 0x21);
        store(program, 0x1F, 0x22, 0x21);
        store(program, 0x00, 0x22, 0x21);
        store(program, 0x00, 0x1B, 0x21);
        store(program, 0x01, 0x1B, 0x21);
        store(program, 0x00, 0x1E, 0x21);
        store(program, 0x01, 0x1E, 0x21);
        store(program, m7sel, 0x1A, 0x21);
        store(program, 0x07, 0x05, 0x21);
        store(program, tm, 0x2C, 0x21);
        store(program, setini, 0x33, 0x21);
        store(program, 0x0F, 0x00, 0x21);
        const auto loop = static_cast<std::uint8_t>(program.size());
        program.insert(program.end(), {0x4C, loop, 0x80});
        return program;
    };

    iris::systems::snes::SnesSystem system;
    std::string error;

    const auto run = [&](const std::vector<std::uint8_t>& program) {
        const std::vector<std::uint8_t> rom = loRom(program);
        REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
        system.runFrame();
        REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
        return system.frame();
    };

    // EXTBG high pixel shows on BG2 only.
    {
        const iris::FrameView frame = run(mode7Setup(0x81, 0x02, 0x40, 0x00));
        REQUIRE(frame.rgba[0] == 255);
        REQUIRE(frame.rgba[1] == 0);
        REQUIRE(frame.rgba[2] == 0);
    }
    // Same high pixel is hidden when only BG1 is enabled.
    {
        const iris::FrameView frame = run(mode7Setup(0x81, 0x01, 0x40, 0x00));
        REQUIRE(frame.rgba[0] == 0);
        REQUIRE(frame.rgba[1] == 0);
        REQUIRE(frame.rgba[2] == 0);
    }

    // H-flip moves the (0,0) texel to screen x=255.
    {
        const iris::FrameView frame = run(mode7Setup(0x01, 0x01, 0x00, 0x01));
        REQUIRE(frame.rgba[0] == 0);
        REQUIRE(frame.rgba[1] == 0);
        REQUIRE(frame.rgba[2] == 0);
        const int right = 255 * 4;
        REQUIRE(frame.rgba[right] == 255);
        REQUIRE(frame.rgba[right + 1] == 0);
        REQUIRE(frame.rgba[right + 2] == 0);
    }

    // M7A=$0100 then M7B write $40 -> MPY = $4000.
    std::vector<std::uint8_t> multiply = {
        0xA9, 0x00, 0x8D, 0x1B, 0x21, 0xA9, 0x01, 0x8D, 0x1B, 0x21, 0xA9, 0x40, 0x8D, 0x1C, 0x21, 0xAD, 0x34, 0x21,
        0x85, 0x10, 0xAD, 0x35, 0x21, 0x85, 0x11, 0xAD, 0x36, 0x21, 0x85, 0x12,
    };
    const auto idle = static_cast<std::uint8_t>(multiply.size());
    multiply.insert(multiply.end(), {0x4C, idle, 0x80});
    run(multiply);
    REQUIRE(system.debugRead(0x0010) == 0x00);
    REQUIRE(system.debugRead(0x0011) == 0x40);
    REQUIRE(system.debugRead(0x0012) == 0x00);
}

TEST_CASE("DMA, HDMA, and IRQs take cycles on the scanline") {
    const std::vector<std::uint8_t> plain = {
        0xA9, 0x00, 0x85, 0x10, 0x85, 0x11, 0xAD, 0x12, 0x42, 0x30, 0x08, 0xE6, 0x10, 0xD0, 0xF7, 0xE6, 0x11, 0x80, 0xF3,
        0x4C, 0x13, 0x80,
    };
    const std::vector<std::uint8_t> paced = {
        0xA9, 0x00, 0x8D, 0x00, 0x43, 0x8D, 0x01, 0x43, 0xA9, 0x20, 0x8D, 0x02, 0x43, 0xA9, 0x00, 0x8D, 0x03, 0x43, 0x8D,
        0x04, 0x43, 0xA9, 0xFF, 0x85, 0x20, 0xA9, 0x00, 0x85, 0x21, 0x85, 0x22, 0xA9, 0x01, 0x8D, 0x0C, 0x42, 0xA9, 0x00,
        0x85, 0x10, 0x85, 0x11, 0xAD, 0x12, 0x42, 0x30, 0x08, 0xE6, 0x10, 0xD0, 0xF7, 0xE6, 0x11, 0x80, 0xF3, 0x4C, 0x37,
        0x80,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> plainRom = loRom(plain);
    REQUIRE(system.loadBytes(plainRom.data(), plainRom.size(), error));
    system.runFrame();
    const int plainCount = system.debugRead(0x0010) | (system.debugRead(0x0011) << 8);
    const std::vector<std::uint8_t> pacedRom = loRom(paced);
    REQUIRE(system.loadBytes(pacedRom.data(), pacedRom.size(), error));
    system.runFrame();
    const int pacedCount = system.debugRead(0x0010) | (system.debugRead(0x0011) << 8);
    REQUIRE(plainCount > 1000);
    REQUIRE(pacedCount > 1000);
    REQUIRE(pacedCount < plainCount);

    const std::vector<std::uint8_t> heavy = {
        0xA9, 0x00, 0x8D, 0x00, 0x43, 0xA9, 0x18, 0x8D, 0x01, 0x43, 0xA9, 0x00, 0x8D, 0x02, 0x43, 0x8D, 0x03, 0x43, 0x8D,
        0x05, 0x43, 0x8D, 0x06, 0x43, 0xA9, 0x7E, 0x8D, 0x04, 0x43, 0xA9, 0x01, 0x8D, 0x0B, 0x42, 0xA9, 0x5A, 0x85, 0x10,
        0x4C, 0x26, 0x80,
    };
    const std::vector<std::uint8_t> light = {
        0xA9, 0x00, 0x8D, 0x00, 0x43, 0xA9, 0x18, 0x8D, 0x01, 0x43, 0xA9, 0x00, 0x8D, 0x02, 0x43, 0x8D, 0x03, 0x43, 0xA9,
        0x01, 0x8D, 0x05, 0x43, 0xA9, 0x00, 0x8D, 0x06, 0x43, 0xA9, 0x7E, 0x8D, 0x04, 0x43, 0xA9, 0x01, 0x8D, 0x0B, 0x42,
        0xA9, 0x5A, 0x85, 0x10, 0x4C, 0x2A, 0x80,
    };
    REQUIRE(system.loadBytes(loRom(heavy).data(), loRom(heavy).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x00);
    REQUIRE(system.loadBytes(loRom(light).data(), loRom(light).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x5A);

    std::vector<std::uint8_t> irq = {
        0x58, 0xA9, 0x20, 0x8D, 0x00, 0x42, 0xA9, 0x0A, 0x8D, 0x09, 0x42, 0xA9, 0x00, 0x8D, 0x0A, 0x42, 0x4C, 0x00, 0x80,
        0xA9, 0xA5, 0x85, 0x12, 0x40,
    };
    std::vector<std::uint8_t> irqRom = loRom(irq);
    irqRom[0x7FFE] = 0x13;
    irqRom[0x7FFF] = 0x80;
    REQUIRE(system.loadBytes(irqRom.data(), irqRom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0012) == 0xA5);

    std::vector<std::uint8_t> horizontal = {
        0x58, 0xA9, 0x10, 0x8D, 0x00, 0x42, 0xA9, 0x64, 0x8D, 0x07, 0x42, 0xA9, 0x00, 0x8D, 0x08, 0x42, 0x4C, 0x00, 0x80,
        0xA9, 0x5A, 0x85, 0x13, 0x40,
    };
    std::vector<std::uint8_t> horizontalRom = loRom(horizontal);
    horizontalRom[0x7FFE] = 0x13;
    horizontalRom[0x7FFF] = 0x80;
    REQUIRE(system.loadBytes(horizontalRom.data(), horizontalRom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0013) == 0x5A);
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
    REQUIRE(mode5.width == 512);
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

TEST_CASE("mosaic repeats a background pixel and background 2 shows behind it") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> plain = loRom(backgroundProgram(0, 0, false, 0, 0, 0x80, 0));
    REQUIRE(system.loadBytes(plain.data(), plain.size(), error));
    system.runFrame();
    const iris::FrameView open = system.frame();
    REQUIRE(open.rgba[0] == 0);
    REQUIRE(open.rgba[2] == 255);
    REQUIRE(open.rgba[4] == 255);
    REQUIRE(open.rgba[6] == 0);

    const std::vector<std::uint8_t> blocked = loRom(backgroundProgram(0, 0, false, 0, 0, 0x80, 0x11));
    REQUIRE(system.loadBytes(blocked.data(), blocked.size(), error));
    system.runFrame();
    const iris::FrameView repeated = system.frame();
    REQUIRE(repeated.rgba[4] == 0);
    REQUIRE(repeated.rgba[6] == 255);

    std::vector<std::uint8_t> second;
    store(second, 0x80, 0x15, 0x21);
    store(second, 0x00, 0x16, 0x21);
    store(second, 0x20, 0x17, 0x21);
    store(second, 0xFF, 0x18, 0x21);
    store(second, 0x00, 0x19, 0x21);
    store(second, 0x00, 0x16, 0x21);
    store(second, 0x04, 0x17, 0x21);
    store(second, 0x00, 0x18, 0x21);
    store(second, 0x00, 0x19, 0x21);
    store(second, 0x01, 0x21, 0x21);
    store(second, 0x00, 0x22, 0x21);
    store(second, 0x7C, 0x22, 0x21);
    store(second, 0x20, 0x0B, 0x21);
    store(second, 0x04, 0x08, 0x21);
    store(second, 0x02, 0x2C, 0x21);
    store(second, 0x0F, 0x00, 0x21);
    const auto loop = static_cast<std::uint8_t>(second.size());
    second.insert(second.end(), {0x4C, loop, 0x80});
    const std::vector<std::uint8_t> secondRom = loRom(second);
    REQUIRE(system.loadBytes(secondRom.data(), secondRom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    const iris::FrameView back = system.frame();
    REQUIRE(back.rgba[0] == 0);
    REQUIRE(back.rgba[2] == 255);
}

TEST_CASE("background 3, priority, scroll, offset, and subscreen math draw the front pixel") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    const auto show = [&](const std::vector<std::uint8_t>& program) {
        const std::vector<std::uint8_t> rom = loRom(program);
        REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
        system.runFrame();
        REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
        return system.frame();
    };

    std::vector<std::uint8_t> third;
    store(third, 0x80, 0x15, 0x21);
    store(third, 0x00, 0x16, 0x21);
    store(third, 0x10, 0x17, 0x21);
    store(third, 0xFF, 0x18, 0x21);
    store(third, 0x00, 0x19, 0x21);
    store(third, 0x00, 0x16, 0x21);
    store(third, 0x04, 0x17, 0x21);
    store(third, 0x00, 0x18, 0x21);
    store(third, 0x00, 0x19, 0x21);
    store(third, 0x01, 0x21, 0x21);
    store(third, 0x00, 0x22, 0x21);
    store(third, 0x7C, 0x22, 0x21);
    store(third, 0x01, 0x0C, 0x21);
    store(third, 0x04, 0x09, 0x21);
    store(third, 0x01, 0x05, 0x21);
    store(third, 0x04, 0x2C, 0x21);
    store(third, 0x0F, 0x00, 0x21);
    const auto thirdLoop = static_cast<std::uint8_t>(third.size());
    third.insert(third.end(), {0x4C, thirdLoop, 0x80});
    const iris::FrameView bg3 = show(third);
    REQUIRE(bg3.rgba[0] == 0);
    REQUIRE(bg3.rgba[2] == 255);

    std::vector<std::uint8_t> priority;
    store(priority, 0x80, 0x15, 0x21);
    store(priority, 0x00, 0x16, 0x21);
    store(priority, 0x10, 0x17, 0x21);
    store(priority, 0xFF, 0x18, 0x21);
    store(priority, 0x00, 0x19, 0x21);
    store(priority, 0x00, 0x16, 0x21);
    store(priority, 0x20, 0x17, 0x21);
    store(priority, 0xFF, 0x18, 0x21);
    store(priority, 0x00, 0x19, 0x21);
    store(priority, 0x00, 0x16, 0x21);
    store(priority, 0x00, 0x17, 0x21);
    store(priority, 0x00, 0x18, 0x21);
    store(priority, 0x00, 0x19, 0x21);
    store(priority, 0x00, 0x16, 0x21);
    store(priority, 0x04, 0x17, 0x21);
    store(priority, 0x00, 0x18, 0x21);
    store(priority, 0x24, 0x19, 0x21);
    store(priority, 0x01, 0x21, 0x21);
    store(priority, 0x00, 0x22, 0x21);
    store(priority, 0x7C, 0x22, 0x21);
    store(priority, 0x05, 0x21, 0x21);
    store(priority, 0xE0, 0x22, 0x21);
    store(priority, 0x03, 0x22, 0x21);
    store(priority, 0x21, 0x0B, 0x21);
    store(priority, 0x04, 0x08, 0x21);
    store(priority, 0x03, 0x2C, 0x21);
    store(priority, 0x0F, 0x00, 0x21);
    const auto priorityLoop = static_cast<std::uint8_t>(priority.size());
    priority.insert(priority.end(), {0x4C, priorityLoop, 0x80});
    const iris::FrameView front = show(priority);
    REQUIRE(front.rgba[0] == 0);
    REQUIRE(front.rgba[1] == 255);
    REQUIRE(front.rgba[2] == 0);

    std::vector<std::uint8_t> scrolled = backgroundProgram(0, 0, false, 0, 0, 0x80, 0);
    scrolled.pop_back();
    scrolled.pop_back();
    scrolled.pop_back();
    store(scrolled, 0x01, 0x0D, 0x21);
    store(scrolled, 0x00, 0x0D, 0x21);
    const auto scrollLoop = static_cast<std::uint8_t>(scrolled.size());
    scrolled.insert(scrolled.end(), {0x4C, scrollLoop, 0x80});
    const iris::FrameView shifted = show(scrolled);
    REQUIRE(shifted.rgba[0] == 255);
    REQUIRE(shifted.rgba[2] == 0);

    std::vector<std::uint8_t> behind;
    store(behind, 0x80, 0x15, 0x21);
    store(behind, 0x00, 0x16, 0x21);
    store(behind, 0x10, 0x17, 0x21);
    store(behind, 0xFF, 0x18, 0x21);
    store(behind, 0x00, 0x19, 0x21);
    store(behind, 0x00, 0x16, 0x21);
    store(behind, 0x20, 0x17, 0x21);
    store(behind, 0xFF, 0x18, 0x21);
    store(behind, 0x00, 0x19, 0x21);
    store(behind, 0x00, 0x16, 0x21);
    store(behind, 0x00, 0x17, 0x21);
    store(behind, 0x00, 0x18, 0x21);
    store(behind, 0x00, 0x19, 0x21);
    store(behind, 0x00, 0x16, 0x21);
    store(behind, 0x04, 0x17, 0x21);
    store(behind, 0x00, 0x18, 0x21);
    store(behind, 0x04, 0x19, 0x21);
    store(behind, 0x01, 0x21, 0x21);
    store(behind, 0x00, 0x22, 0x21);
    store(behind, 0x7C, 0x22, 0x21);
    store(behind, 0x05, 0x21, 0x21);
    store(behind, 0xE0, 0x22, 0x21);
    store(behind, 0x03, 0x22, 0x21);
    store(behind, 0x21, 0x0B, 0x21);
    store(behind, 0x04, 0x08, 0x21);
    store(behind, 0x02, 0x30, 0x21);
    store(behind, 0x01, 0x31, 0x21);
    store(behind, 0x01, 0x2C, 0x21);
    store(behind, 0x02, 0x2D, 0x21);
    store(behind, 0x0F, 0x00, 0x21);
    const auto behindLoop = static_cast<std::uint8_t>(behind.size());
    behind.insert(behind.end(), {0x4C, behindLoop, 0x80});
    const iris::FrameView added = show(behind);
    REQUIRE(added.rgba[0] == 0);
    REQUIRE(added.rgba[1] == 255);
    REQUIRE(added.rgba[2] == 255);
}

TEST_CASE("Mode 2 OPT, subtract math, and color window edges") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    const auto run = [&](const std::vector<std::uint8_t>& program, int frames = 1) {
        const std::vector<std::uint8_t> rom = loRom(program);
        REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
        for (int frame = 0; frame < frames; ++frame) {
            system.runFrame();
        }
        REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
        return system.frame();
    };

    // Compact Mode 2 setup (same char/map style as backgroundProgram) plus OPT V on column 1.
    std::vector<std::uint8_t> opt;
    store(opt, 0x80, 0x15, 0x21);
    store(opt, 0x00, 0x16, 0x21);
    store(opt, 0x10, 0x17, 0x21);
    store(opt, 0xFF, 0x18, 0x21);
    store(opt, 0x00, 0x19, 0x21);
    // Tile 1 at word $1010 (16 words after tile 0): row0 = color 2.
    store(opt, 0x10, 0x16, 0x21);
    store(opt, 0x10, 0x17, 0x21);
    store(opt, 0x00, 0x18, 0x21);
    store(opt, 0xFF, 0x19, 0x21);
    // map (0,0)=tile0 pal1; map (1,1)=tile1 pal1 (x=8 → tile column 1 after OPT V).
    store(opt, 0x00, 0x16, 0x21);
    store(opt, 0x00, 0x17, 0x21);
    store(opt, 0x00, 0x18, 0x21);
    store(opt, 0x04, 0x19, 0x21);
    store(opt, 0x21, 0x16, 0x21);
    store(opt, 0x00, 0x17, 0x21);
    store(opt, 0x01, 0x18, 0x21);
    store(opt, 0x04, 0x19, 0x21);
    // BG3SC=$08, V-offset entry for OPT column 0 at word $0820 = $2008 (BG1, V=8).
    store(opt, 0x20, 0x16, 0x21);
    store(opt, 0x08, 0x17, 0x21);
    store(opt, 0x08, 0x18, 0x21);
    store(opt, 0x20, 0x19, 0x21);
    store(opt, 0x00, 0x21, 0x21);
    store(opt, 0x1F, 0x22, 0x21);
    store(opt, 0x00, 0x22, 0x21);
    store(opt, 0x11, 0x21, 0x21);
    store(opt, 0x1F, 0x22, 0x21);
    store(opt, 0x00, 0x22, 0x21);
    store(opt, 0x12, 0x21, 0x21);
    store(opt, 0xE0, 0x22, 0x21);
    store(opt, 0x03, 0x22, 0x21);
    store(opt, 0x01, 0x0B, 0x21);
    store(opt, 0x08, 0x09, 0x21);
    store(opt, 0x02, 0x05, 0x21);
    store(opt, 0x01, 0x2C, 0x21);
    store(opt, 0x0F, 0x00, 0x21);
    const auto optLoop = static_cast<std::uint8_t>(opt.size());
    opt.insert(opt.end(), {0x4C, optLoop, 0x80});
    {
        const iris::FrameView frame = run(opt);
        REQUIRE(frame.rgba[0] == 255);
        REQUIRE(frame.rgba[1] == 0);
        REQUIRE(frame.rgba[2] == 0);
        // Column 1 (x=8) samples map row1 via OPT → green at CGRAM $12.
        REQUIRE(frame.rgba[8 * 4] == 0);
        REQUIRE(frame.rgba[8 * 4 + 1] == 255);
        REQUIRE(frame.rgba[8 * 4 + 2] == 0);
    }

    // Color window: math only inside WH0..WH1 (CGWSEL %01xxxx); outside matches no-math.
    const iris::FrameView plain = run(backgroundProgram(0, 0, false, 0, 0));
    std::vector<std::uint8_t> clipped = backgroundProgram(0, 0, false, 0x01, 0x5F);
    clipped.pop_back();
    clipped.pop_back();
    clipped.pop_back();
    store(clipped, 0x00, 0x26, 0x21);
    store(clipped, 0x07, 0x27, 0x21);
    store(clipped, 0x20, 0x25, 0x21);  // color window 1 enable
    store(clipped, 0x10, 0x30, 0x21);  // math inside window, fixed color
    store(clipped, 0x01, 0x31, 0x21);
    store(clipped, 0x5F, 0x32, 0x21);
    const auto clipLoop = static_cast<std::uint8_t>(clipped.size());
    clipped.insert(clipped.end(), {0x4C, clipLoop, 0x80});
    {
        const iris::FrameView frame = run(clipped);
        const iris::FrameView added = run(backgroundProgram(0, 0, false, 0x01, 0x5F));
        // Inside window matches full-screen add.
        REQUIRE(frame.rgba[0] == added.rgba[0]);
        REQUIRE(frame.rgba[1] == added.rgba[1]);
        REQUIRE(frame.rgba[2] == added.rgba[2]);
        // Outside window matches the no-math picture.
        REQUIRE(frame.rgba[16] == plain.rgba[16]);
        REQUIRE(frame.rgba[17] == plain.rgba[17]);
        REQUIRE(frame.rgba[18] == plain.rgba[18]);
    }
}

void waitPort(std::vector<std::uint8_t>& code, std::uint16_t port, std::uint8_t value) {
    code.insert(code.end(), {0xAD, static_cast<std::uint8_t>(port), static_cast<std::uint8_t>(port >> 8), 0xC9, value, 0xD0, 0xF9});
}

void loadPort(std::vector<std::uint8_t>& code, std::uint8_t value, std::uint16_t port) {
    code.insert(code.end(), {0xA9, value, 0x8D, static_cast<std::uint8_t>(port), static_cast<std::uint8_t>(port >> 8)});
}

void sendBlock(std::vector<std::uint8_t>& code, std::vector<std::pair<int, std::vector<std::uint8_t>>>& blocks,
               std::uint16_t address, const std::vector<std::uint8_t>& payload, bool first) {
    if (first) {
        waitPort(code, 0x2140, 0xAA);
        waitPort(code, 0x2141, 0xBB);
        loadPort(code, static_cast<std::uint8_t>(address), 0x2142);
        loadPort(code, static_cast<std::uint8_t>(address >> 8), 0x2143);
        loadPort(code, 0x01, 0x2141);
        loadPort(code, 0xCC, 0x2140);
        waitPort(code, 0x2140, 0xCC);
    } else {
        loadPort(code, 0x01, 0x2141);
        loadPort(code, static_cast<std::uint8_t>(address), 0x2142);
        loadPort(code, static_cast<std::uint8_t>(address >> 8), 0x2143);
        code.insert(code.end(), {0xAD, 0x40, 0x21, 0x18, 0x69, 0x02, 0xD0, 0x01, 0x1A, 0x8D, 0x40, 0x21, 0xCD, 0x40, 0x21, 0xD0, 0xFB});
    }
    code.insert(code.end(), {0xA2, 0x00, 0xBD, 0x00, 0x00, 0x8D, 0x41, 0x21, 0x8A, 0x8D, 0x40, 0x21, 0xCD, 0x40, 0x21, 0xD0, 0xFB, 0xE8, 0xE0,
                             static_cast<std::uint8_t>(payload.size()), 0xD0, 0xEC});
    blocks.push_back({static_cast<int>(code.size()) - 19, payload});
}

std::vector<std::uint8_t> transferProgram(std::uint16_t entry,
                                          const std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>>& parts,
                                          const std::vector<std::uint8_t>& afterStart = {}) {
    std::vector<std::uint8_t> code;
    std::vector<std::pair<int, std::vector<std::uint8_t>>> blocks;
    bool first = true;
    for (const auto& part : parts) {
        sendBlock(code, blocks, part.first, part.second, first);
        first = false;
    }
    loadPort(code, static_cast<std::uint8_t>(entry), 0x2142);
    loadPort(code, static_cast<std::uint8_t>(entry >> 8), 0x2143);
    loadPort(code, 0x00, 0x2141);
    code.insert(code.end(), {0xAD, 0x40, 0x21, 0x18, 0x69, 0x02, 0xD0, 0x01, 0x1A, 0x8D, 0x40, 0x21});
    if (!afterStart.empty()) {
        code.insert(code.end(), afterStart.begin(), afterStart.end());
    }
    const auto idle = static_cast<std::uint16_t>(0x8000 + code.size());
    code.insert(code.end(), {0x4C, static_cast<std::uint8_t>(idle), static_cast<std::uint8_t>(idle >> 8)});
    for (const auto& block : blocks) {
        const auto data = static_cast<std::uint16_t>(0x8000 + code.size());
        code[static_cast<std::size_t>(block.first)] = static_cast<std::uint8_t>(data);
        code[static_cast<std::size_t>(block.first + 1)] = static_cast<std::uint8_t>(data >> 8);
        code.insert(code.end(), block.second.begin(), block.second.end());
    }
    return code;
}

TEST_CASE("after boot the SPC answers an in-game port command") {
    // Snapshot the IPL kick, signal ready, wait until the CPU changes port 0, then
    // echo the command to port 1 and ack with $80 (engines must not treat the kick as a command).
    const std::vector<std::uint8_t> spc = {0xE4, 0xF4, 0x8F, 0x00, 0xF4, 0x64, 0xF4, 0xF0, 0xFC, 0xE4, 0xF4, 0xC4,
                                           0xF5, 0x8F, 0x80, 0xF4, 0x2F, 0xFE};
    // Wait ready, send $42, wait ack, check $2144 mirror, store markers in WRAM.
    const std::vector<std::uint8_t> after = {
        0xAD, 0x40, 0x21, 0xD0, 0xFB, 0xA9, 0x42, 0x8D, 0x40, 0x21, 0xAD, 0x40, 0x21, 0xC9, 0x80, 0xD0, 0xF9,
        0xAD, 0x41, 0x21, 0xC9, 0x42, 0xD0, 0xF2, 0xA9, 0x5A, 0x85, 0x10, 0xAD, 0x44, 0x21, 0xC9, 0x80, 0xD0, 0x02,
        0xA9, 0xA5, 0x85, 0x11};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom =
        loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), spc}}, after));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x0010) == 0x5A);
    REQUIRE(system.debugRead(0x0011) == 0xA5);
    REQUIRE(system.debugRead(0x2140) == 0x80);
    REQUIRE(system.debugRead(0x2141) == 0x42);
}

TEST_CASE("Akao-style APU traffic keeps kick≠command and multi-port params") {
    // Publish kick on $F5, ready on $F4=0, wait port0 != kick, echo $F5–$F7, ack $80;
    // $F1 clears inputs; wait next command; ack $81.
    const std::vector<std::uint8_t> spc = {
        0xE4, 0xF4, 0xC4, 0xF5, 0x8F, 0x00, 0xF4, 0x64, 0xF4, 0xF0, 0xFC, 0xE4, 0xF5, 0xC4, 0xF5, 0xE4,
        0xF6, 0xC4, 0xF6, 0xE4, 0xF7, 0xC4, 0xF7, 0x8F, 0x80, 0xF4, 0x8F, 0x10, 0xF1, 0xE4, 0xF4, 0x64,
        0xF4, 0xF0, 0xFC, 0x8F, 0x81, 0xF4, 0x2F, 0xFE};
    // Wait ready; rewrite kick from $2141 (must stay non-ack); params then $42; second cmd $43.
    const std::vector<std::uint8_t> after = {
        0xAD, 0x40, 0x21, 0xD0, 0xFB, 0xAD, 0x41, 0x21, 0x8D, 0x40, 0x21, 0xAD, 0x40, 0x21, 0xC9, 0x80,
        0xD0, 0x02, 0x80, 0xFE, 0xA9, 0x11, 0x8D, 0x41, 0x21, 0xA9, 0x22, 0x8D, 0x42, 0x21, 0xA9, 0x33,
        0x8D, 0x43, 0x21, 0xA9, 0x42, 0x8D, 0x40, 0x21, 0xAD, 0x40, 0x21, 0xC9, 0x80, 0xD0, 0xF9, 0xAD,
        0x41, 0x21, 0xC9, 0x11, 0xD0, 0xF2, 0xAD, 0x42, 0x21, 0xC9, 0x22, 0xD0, 0xEB, 0xAD, 0x43, 0x21,
        0xC9, 0x33, 0xD0, 0xE4, 0xA9, 0x43, 0x8D, 0x40, 0x21, 0xAD, 0x40, 0x21, 0xC9, 0x81, 0xD0, 0xF9,
        0xA9, 0x5A, 0x85, 0x10, 0xAD, 0x47, 0x21, 0xC9, 0x33, 0xD0, 0x02, 0xA9, 0xA5, 0x85, 0x11};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom =
        loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), spc}}, after));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x0010) == 0x5A);
    REQUIRE(system.debugRead(0x0011) == 0xA5);
    REQUIRE(system.debugRead(0x2140) == 0x81);
    REQUIRE(system.debugRead(0x2141) == 0x11);
    REQUIRE(system.debugRead(0x2142) == 0x22);
    REQUIRE(system.debugRead(0x2143) == 0x33);
}

TEST_CASE("a 16-bit in-game APU command commits both port bytes before the SPC acks") {
    // Ready, then REP #$20 / LDA #$2211 / STA $2140 writes cmd $11 and param $22 together.
    const std::vector<std::uint8_t> spc = {0xE4, 0xF4, 0x8F, 0x00, 0xF4, 0x64, 0xF4, 0xF0, 0xFC, 0xE4, 0xF4, 0xC4,
                                           0xF5, 0xE4, 0xF5, 0xC4, 0xF6, 0x8F, 0x80, 0xF4, 0x2F, 0xFE};
    const std::vector<std::uint8_t> after = {
        0xAD, 0x40, 0x21, 0xD0, 0xFB, 0x18, 0xFB, 0xC2, 0x20, 0xA9, 0x11, 0x22, 0x8D, 0x40, 0x21, 0xE2,
        0x20, 0xAD, 0x40, 0x21, 0xC9, 0x80, 0xD0, 0xF9, 0xAD, 0x41, 0x21, 0xC9, 0x11, 0xD0, 0xF2, 0xAD,
        0x42, 0x21, 0xC9, 0x22, 0xD0, 0xEB, 0xA9, 0x5A, 0x85, 0x10};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom =
        loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), spc}}, after));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x5A);
    REQUIRE(system.debugRead(0x2140) == 0x80);
    REQUIRE(system.debugRead(0x2141) == 0x11);
    REQUIRE(system.debugRead(0x2142) == 0x22);
}

TEST_CASE("an SPC timer IRQ wakes SLEEP and answers the CPU") {
    // Timer 0 target 1, enable, EI, SLEEP. IRQ at $0300 writes $5A to port 0 and RETI.
    const std::vector<std::uint8_t> main = {0x8F, 0x01, 0xFA, 0x8F, 0x01, 0xF1, 0xA0, 0xEF, 0x2F, 0xFE};
    const std::vector<std::uint8_t> irq = {0x8F, 0x5A, 0xF4, 0x7F};
    const std::vector<std::uint8_t> vectors = {0x00, 0x03};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(transferProgram(
        0x0200, {{static_cast<std::uint16_t>(0x0200), main},
                 {static_cast<std::uint16_t>(0x0300), irq},
                 {static_cast<std::uint16_t>(0xFFDE), vectors}}));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x2140) == 0x5A);
}

TEST_CASE("SPC700 ALU and bit opcodes do not halt and produce expected results") {
    // OR/AND/ADC A,#imm and SET1/BBS — early-dispatch groups commercial engines use.
    const std::vector<std::uint8_t> spc = {
        0xE8, 0x0F,        // MOV A, #$0F
        0x08, 0xF0,        // OR A, #$F0 -> $FF
        0x28, 0x0F,        // AND A, #$0F -> $0F
        0x80,              // SETC
        0x88, 0x01,        // ADC A, #$01 -> $11
        0xC4, 0x00,        // MOV $00, A
        0x8F, 0x00, 0x11,  // MOV $11, #$00
        0x02, 0x11,        // SET1 $11.0
        0x03, 0x11, 0x02,  // BBS $11.0, skip MOV A,#$00
        0xE8, 0x00,        // cleared only if BBS fails
        0xE4, 0x00,        // MOV A, $00
        0xC4, 0xF4,        // MOV $F4, A
        0x2F, 0xFE,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom =
        loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), spc}}));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x2140) == 0x11);
}

std::vector<std::uint8_t> hiRomProgram(const std::vector<std::uint8_t>& program) {
    std::vector<std::uint8_t> rom(64 * 1024, 0);
    for (std::size_t index = 0; index < program.size() && index < 0x7FC0; ++index) {
        rom[0x8000 + index] = program[index];
    }
    rom[0xFFD5] = 0x21;
    rom[0xFFD9] = 0x01;
    rom[0xFFDC] = 0xFF;
    rom[0xFFDD] = 0xFF;
    rom[0xFFFC] = 0x00;
    rom[0xFFFD] = 0x80;
    return rom;
}

TEST_CASE("multi-block IPL and HiROM still reach an in-game APU ready state") {
    // Second block carries a data byte; the entry program publishes $5A and that byte.
    const std::vector<std::uint8_t> payload = {0xA5};
    const std::vector<std::uint8_t> spc = {0x8F, 0x5A, 0x00, 0xE4, 0x00, 0xC4, 0xF4, 0xE5, 0x00, 0x03, 0xC4, 0xF5,
                                           0x2F, 0xFE};
    const std::vector<std::uint8_t> after = {0xAD, 0x40, 0x21, 0xC9, 0x5A, 0xD0, 0xF9, 0xAD, 0x41, 0x21, 0xC9, 0xA5,
                                             0xD0, 0xF2, 0xA9, 0x5A, 0x85, 0x10};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const auto program = transferProgram(0x0200,
                                         {{static_cast<std::uint16_t>(0x0200), spc},
                                          {static_cast<std::uint16_t>(0x0300), payload}},
                                         after);
    const std::vector<std::uint8_t> lo = loRom(program);
    REQUIRE(system.loadBytes(lo.data(), lo.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x5A);
    REQUIRE(system.debugRead(0x2140) == 0x5A);
    REQUIRE(system.debugRead(0x2141) == 0xA5);

    const std::vector<std::uint8_t> hi = hiRomProgram(program);
    REQUIRE(system.loadBytes(hi.data(), hi.size(), error));
    REQUIRE(system.info().detail == "HiROM, NTSC");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x5A);
    REQUIRE(system.debugRead(0x2140) == 0x5A);
    REQUIRE(system.debugRead(0x2141) == 0xA5);
}

TEST_CASE("horizontal blank edges stay line-relative through the NTSC short scanline") {
    // Sixteen set→clear edges of $4212 bit 6 (same polarity as the FF6 wait test).
    // Enough edges to pass NTSC line 240; a drifted cycle%1364 HBlank never clears again.
    const std::vector<std::uint8_t> program = {
        0xA2, 0x00, 0xAD, 0x12, 0x42, 0x29, 0x40, 0xF0, 0xF9, 0xAD, 0x12, 0x42, 0x29, 0x40, 0xD0, 0xF9,
        0xE8, 0xE0, 0x10, 0xD0, 0xED, 0x86, 0x10, 0xDB,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(program);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    for (int frame = 0; frame < 4; ++frame) {
        system.runFrame();
        if (system.debugRead(0x0010) == 0x10) {
            break;
        }
    }
    REQUIRE(system.debugRead(0x0010) == 0x10);
}

TEST_CASE("the SPC700 answers a transfer and the DSP plays a sample") {
    const std::vector<std::uint8_t> marker = {0x8F, 0x5A, 0x00, 0xE4, 0x00, 0xC4, 0xF4, 0x2F, 0xFE};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> markerRom =
        loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), marker}}));
    REQUIRE(system.loadBytes(markerRom.data(), markerRom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x2140) == 0x5A);
    const iris::AudioView silent = system.audio();
    REQUIRE(silent.samples != nullptr);
    REQUIRE(silent.count > 0);
    REQUIRE(silent.sampleRate == 48000);

    std::vector<std::uint8_t> tone;
    const auto page = [&](std::uint8_t value, std::uint8_t address) {
        tone.insert(tone.end(), {0x8F, value, address});
    };
    page(0x00, 0xF2);
    page(0x7F, 0xF3);
    page(0x01, 0xF2);
    page(0x7F, 0xF3);
    page(0x02, 0xF2);
    page(0x00, 0xF3);
    page(0x03, 0xF2);
    page(0x10, 0xF3);
    page(0x04, 0xF2);
    page(0x00, 0xF3);
    page(0x07, 0xF2);
    page(0x7F, 0xF3);
    page(0x5D, 0xF2);
    page(0x03, 0xF3);
    page(0x0C, 0xF2);
    page(0x7F, 0xF3);
    page(0x1C, 0xF2);
    page(0x7F, 0xF3);
    page(0x6C, 0xF2);
    page(0x20, 0xF3);  // echo write protect; EDL=0 would otherwise overwrite ESA
    page(0x4C, 0xF2);
    page(0x01, 0xF3);
    tone.insert(tone.end(), {0x2F, 0xFE});
    std::vector<std::uint8_t> sample = {0x10, 0x03, 0x10, 0x03};
    sample.resize(0x10, 0);
    sample.insert(sample.end(), {0xC3, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77});
    const std::vector<std::uint8_t> toneRom =
        loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), tone}, {static_cast<std::uint16_t>(0x0300), sample}}));
    REQUIRE(system.loadBytes(toneRom.data(), toneRom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    const iris::AudioView played = system.audio();
    REQUIRE(played.count > 100);
    REQUIRE(played.channels == 2);
    float peak = 0.0f;
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    for (int index = 0; index < played.count; ++index) {
        const float left = played.samples[index * 2];
        const float right = played.samples[index * 2 + 1];
        const float leftAbs = left < 0.0f ? -left : left;
        const float rightAbs = right < 0.0f ? -right : right;
        if (leftAbs > leftPeak) {
            leftPeak = leftAbs;
        }
        if (rightAbs > rightPeak) {
            rightPeak = rightAbs;
        }
        if (leftAbs > peak) {
            peak = leftAbs;
        }
        if (rightAbs > peak) {
            peak = rightAbs;
        }
    }
    REQUIRE(peak > 0.05f);
    REQUIRE(leftPeak > 0.05f);
    REQUIRE(rightPeak > 0.05f);
}

TEST_CASE("FLG mute keeps the host quiet while ENVX still advances") {
    std::vector<std::uint8_t> program;
    const auto dsp = [&](std::uint8_t address, std::uint8_t value) {
        program.insert(program.end(), {0x8F, address, 0xF2, 0x8F, value, 0xF3});
    };
    dsp(0x00, 0x7F);
    dsp(0x01, 0x7F);
    dsp(0x02, 0x00);
    dsp(0x03, 0x10);
    dsp(0x04, 0x00);
    dsp(0x05, 0x8F);
    dsp(0x06, 0xE0);
    dsp(0x5D, 0x03);
    dsp(0x0C, 0x7F);
    dsp(0x1C, 0x7F);
    // Mute only (no soft reset): DAC silent, envelopes still run.
    dsp(0x6C, 0x40);
    dsp(0x4C, 0x01);
    program.insert(program.end(), {0x8F, 0x08, 0xF2, 0xE4, 0xF3, 0xC4, 0xF4, 0x2F, 0xF5});
    std::vector<std::uint8_t> directory = {0x04, 0x03, 0x04, 0x03};
    std::vector<std::uint8_t> sample = {0xC3, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(transferProgram(
        0x0200, {{static_cast<std::uint16_t>(0x0200), program},
                 {static_cast<std::uint16_t>(0x0300), directory},
                 {static_cast<std::uint16_t>(0x0304), sample}}));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    const iris::AudioView muted = system.audio();
    float peak = 0.0f;
    for (int index = 0; index < muted.count; ++index) {
        const float left = muted.samples[index * 2];
        const float abs = left < 0.0f ? -left : left;
        if (abs > peak) {
            peak = abs;
        }
    }
    REQUIRE(peak < 0.01f);
    REQUIRE(system.debugRead(0x2140) >= 0x70);
}

TEST_CASE("pitch modulation is masked when the modulator voice uses noise") {
    std::vector<std::uint8_t> program;
    const auto dsp = [&](std::uint8_t address, std::uint8_t value) {
        program.insert(program.end(), {0x8F, address, 0xF2, 0x8F, value, 0xF3});
    };
    // Voice 0 noise, voice 1 tone with PMON from voice 0 — NON must disable the mod.
    dsp(0x00, 0x7F);
    dsp(0x01, 0x7F);
    dsp(0x02, 0x00);
    dsp(0x03, 0x10);
    dsp(0x04, 0x00);
    dsp(0x05, 0x8F);
    dsp(0x06, 0xE0);
    dsp(0x10, 0x7F);
    dsp(0x11, 0x7F);
    dsp(0x12, 0x00);
    dsp(0x13, 0x08);
    dsp(0x14, 0x00);
    dsp(0x15, 0x8F);
    dsp(0x16, 0xE0);
    dsp(0x5D, 0x03);
    dsp(0x0C, 0x7F);
    dsp(0x1C, 0x7F);
    dsp(0x3D, 0x01);  // NON voice 0
    dsp(0x2D, 0x02);  // PMON voice 1
    dsp(0x6C, 0x00);
    dsp(0x4C, 0x03);
    // Publish voice 1 ENVX — tone + attack still run when PMON is masked by NON.
    program.insert(program.end(), {0x8F, 0x18, 0xF2, 0xE4, 0xF3, 0xC4, 0xF4, 0x2F, 0xF5});
    std::vector<std::uint8_t> directory = {0x04, 0x03, 0x04, 0x03};
    std::vector<std::uint8_t> sample = {0xC3, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(transferProgram(
        0x0200, {{static_cast<std::uint16_t>(0x0200), program},
                 {static_cast<std::uint16_t>(0x0300), directory},
                 {static_cast<std::uint16_t>(0x0304), sample}}));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    const iris::AudioView played = system.audio();
    float peak = 0.0f;
    for (int index = 0; index < played.count; ++index) {
        const float left = played.samples[index * 2];
        const float abs = left < 0.0f ? -left : left;
        if (abs > peak) {
            peak = abs;
        }
    }
    REQUIRE(peak > 0.02f);
    REQUIRE(system.debugRead(0x2140) >= 0x70);
}

TEST_CASE("ADSR attack peaks and EDL zero still writes echo RAM") {
    std::vector<std::uint8_t> program;
    const auto dsp = [&](std::uint8_t address, std::uint8_t value) {
        program.insert(program.end(), {0x8F, address, 0xF2, 0x8F, value, 0xF3});
    };
    // Voice 0: volumes, pitch $1000, source 0, ADSR AR=$F / SL=7 / SR=0.
    dsp(0x00, 0x7F);
    dsp(0x01, 0x7F);
    dsp(0x02, 0x00);
    dsp(0x03, 0x10);
    dsp(0x04, 0x00);
    dsp(0x05, 0x8F);
    dsp(0x06, 0xE0);
    dsp(0x5D, 0x03);  // DIR at $0300 (uploaded with the BRR block)
    dsp(0x0C, 0x7F);
    dsp(0x1C, 0x7F);
    // Echo at $0400, EDL=0 (one stereo sample), FIR0=0x7F, EON voice 0.
    dsp(0x6D, 0x04);
    dsp(0x7D, 0x00);
    dsp(0x0F, 0x7F);
    dsp(0x2C, 0x40);
    dsp(0x3C, 0x40);
    dsp(0x4D, 0x01);
    dsp(0x0D, 0x00);
    dsp(0x6C, 0x00);
    dsp(0x4C, 0x01);
    // Copy ENVX ($08) to port 0 and echo RAM at ESA to port 1.
    program.insert(program.end(),
                   {0x8F, 0x08, 0xF2, 0xE4, 0xF3, 0xC4, 0xF4, 0xE5, 0x00, 0x04, 0xC4, 0xF5, 0x2F, 0xF2});
    std::vector<std::uint8_t> directory = {0x04, 0x03, 0x04, 0x03};
    std::vector<std::uint8_t> sample = {0xC3, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(transferProgram(
        0x0200, {{static_cast<std::uint16_t>(0x0200), program},
                 {static_cast<std::uint16_t>(0x0300), directory},
                 {static_cast<std::uint16_t>(0x0304), sample}}));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
    REQUIRE(system.debugRead(0x2140) >= 0x70);
    // EDL=0 still produces echo writes at ESA ($0400).
    REQUIRE(system.debugRead(0x2141) != 0);
    const iris::AudioView played = system.audio();
    float peak = 0.0f;
    for (int index = 0; index < played.count; ++index) {
        const float left = played.samples[index * 2];
        const float abs = left < 0.0f ? -left : left;
        if (abs > peak) {
            peak = abs;
        }
    }
    REQUIRE(peak > 0.02f);
}

void staLong(std::vector<std::uint8_t>& code, std::uint8_t value, std::uint32_t address) {
    code.insert(code.end(), {0xA9, value, 0x8F, static_cast<std::uint8_t>(address), static_cast<std::uint8_t>(address >> 8),
                             static_cast<std::uint8_t>(address >> 16)});
}

void ldaLongSta(std::vector<std::uint8_t>& code, std::uint32_t source, std::uint16_t dest) {
    code.insert(code.end(), {0xAF, static_cast<std::uint8_t>(source), static_cast<std::uint8_t>(source >> 8),
                             static_cast<std::uint8_t>(source >> 16), 0x8D, static_cast<std::uint8_t>(dest),
                             static_cast<std::uint8_t>(dest >> 8)});
}

void finishIdle(std::vector<std::uint8_t>& code) {
    const auto idle = static_cast<std::uint16_t>(0x8000 + code.size());
    code.insert(code.end(), {0x4C, static_cast<std::uint8_t>(idle), static_cast<std::uint8_t>(idle >> 8)});
}

std::vector<std::uint8_t> boardRom(const std::vector<std::uint8_t>& program, std::uint8_t mapMode, std::uint8_t type) {
    std::vector<std::uint8_t> rom = loRom(program);
    rom[0x7FD5] = mapMode;
    rom[0x7FD6] = type;
    return rom;
}

TEST_CASE("a user IPL ROM maps at $FFC0 when CONTROL bit 7 is set") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    std::uint8_t ipl[64] = {};
    // Original Iris test IPL: MOV $F4,#$A5 / MOV $F5,#$5A / loop. Not a Nintendo dump.
    ipl[0] = 0x8F;
    ipl[1] = 0xA5;
    ipl[2] = 0xF4;
    ipl[3] = 0x8F;
    ipl[4] = 0x5A;
    ipl[5] = 0xF5;
    ipl[6] = 0x2F;
    ipl[7] = 0xFE;
    const auto path = std::filesystem::temp_directory_path() / "iris_test_ipl.rom";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(ipl), sizeof(ipl));
        REQUIRE(out);
    }
    REQUIRE(system.loadIplFile(path, error));
    REQUIRE(system.hasIpl());
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    std::vector<std::uint8_t> program;
    finishIdle(program);
    const std::vector<std::uint8_t> rom = loRom(program);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x2140) == 0xA5);
    REQUIRE(system.debugRead(0x2141) == 0x5A);
}

TEST_CASE("DSP-1, Super FX, and SA-1 answer the cartridge") {
    std::vector<std::uint8_t> dsp;
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    ldaLongSta(dsp, 0x308000, 0x0010);
    ldaLongSta(dsp, 0x308000, 0x0011);
    staLong(dsp, 0x10, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    ldaLongSta(dsp, 0x308000, 0x0012);
    ldaLongSta(dsp, 0x308000, 0x0013);
    staLong(dsp, 0x04, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x01, 0x308000);
    ldaLongSta(dsp, 0x308000, 0x0014);
    ldaLongSta(dsp, 0x308000, 0x0015);
    ldaLongSta(dsp, 0x308000, 0x0016);
    ldaLongSta(dsp, 0x308000, 0x0017);
    ldaLongSta(dsp, 0x30C000, 0x0018);
    finishIdle(dsp);

    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> dspRom = boardRom(dsp, 0x20, 0x03);
    REQUIRE(system.loadBytes(dspRom.data(), dspRom.size(), error));
    REQUIRE(system.info().detail == "LoROM, NTSC, DSP-1");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x00);
    REQUIRE(system.debugRead(0x0011) == 0x20);
    REQUIRE(system.debugRead(0x0012) == 0xFF);
    REQUIRE(system.debugRead(0x0013) == 0x7F);
    REQUIRE(system.debugRead(0x0014) == 0x00);
    REQUIRE(system.debugRead(0x0015) == 0x00);
    REQUIRE(system.debugRead(0x0016) == 0x00);
    REQUIRE(system.debugRead(0x0017) == 0x01);
    REQUIRE((system.debugRead(0x0018) & 0x80) != 0);

    const std::uint8_t gsuCode[] = {0xF0, 0xA5, 0x00, 0xF1, 0x10, 0x00, 0x3D, 0x31, 0x4E, 0xF1, 0x02, 0x00,
                                    0xF2, 0x03, 0x00, 0x4C, 0x00};
    std::vector<std::uint8_t> fx;
    for (std::size_t index = 0; index < sizeof(gsuCode); ++index) {
        staLong(fx, gsuCode[index], static_cast<std::uint32_t>(0x700000 + index));
    }
    store(fx, 0x70, 0x34, 0x30);
    store(fx, 0x00, 0x1E, 0x30);
    store(fx, 0x00, 0x1F, 0x30);
    store(fx, 0x20, 0x30, 0x30);
    finishIdle(fx);
    const std::vector<std::uint8_t> fxRom = boardRom(fx, 0x20, 0x14);
    REQUIRE(system.loadBytes(fxRom.data(), fxRom.size(), error));
    REQUIRE(system.info().detail == "LoROM, NTSC, Super FX");
    system.runFrame();
    REQUIRE(system.debugRead(0x700010) == 0xA5);
    REQUIRE(system.debugRead(0x700302) == 0xA5);

    const std::uint8_t sa1Code[] = {0xA9, 0x5A, 0x8D, 0x20, 0x30, 0x80, 0xFE};
    std::vector<std::uint8_t> sa1;
    for (std::size_t index = 0; index < sizeof(sa1Code); ++index) {
        store(sa1, sa1Code[index], static_cast<std::uint8_t>(index), 0x30);
    }
    store(sa1, 0x00, 0x03, 0x22);
    store(sa1, 0x00, 0x04, 0x22);
    store(sa1, 0x20, 0x00, 0x22);
    finishIdle(sa1);
    const std::vector<std::uint8_t> sa1Rom = boardRom(sa1, 0x23, 0x35);
    REQUIRE(system.loadBytes(sa1Rom.data(), sa1Rom.size(), error));
    REQUIRE(system.info().detail == "SA-1, NTSC");
    system.runFrame();
    REQUIRE(system.debugRead(0x003020) == 0x5A);

    std::vector<std::uint8_t> exHi = loRom({});
    exHi[0x7FD5] = 0x25;
    REQUIRE_FALSE(system.loadBytes(exHi.data(), exHi.size(), error));
    REQUIRE(error.find("not supported") != std::string::npos);

    std::vector<std::uint8_t> exImage(0x410000, 0);
    exImage[0x000000] = 0xA5;
    exImage[0x400000] = 0x5A;
    const std::uint8_t exCode[] = {0xAF, 0x00, 0x00, 0xC0, 0x8D, 0x10, 0x00, 0xAF, 0x00, 0x00, 0x40, 0x8D, 0x11, 0x00, 0x4C, 0x0E, 0x80};
    for (std::size_t index = 0; index < sizeof(exCode); ++index) {
        exImage[0x408000 + index] = exCode[index];
    }
    const std::string exTitle = "EXHIROM TEST";
    for (std::size_t index = 0; index < exTitle.size(); ++index) {
        exImage[0x40FFC0 + index] = static_cast<std::uint8_t>(exTitle[index]);
    }
    exImage[0x40FFD5] = 0x25;
    exImage[0x40FFD9] = 0x01;
    exImage[0x40FFDC] = 0xFF;
    exImage[0x40FFDD] = 0xFF;
    exImage[0x40FFFC] = 0x00;
    exImage[0x40FFFD] = 0x80;
    REQUIRE(system.loadBytes(exImage.data(), exImage.size(), error));
    REQUIRE(system.info().title == "EXHIROM TEST");
    REQUIRE(system.info().detail == "ExHiROM, NTSC");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0xA5);
    REQUIRE(system.debugRead(0x0011) == 0x5A);
}

TEST_CASE("an eight-bit index sign-extends into the accumulator and an indexed write crosses the bank") {
    std::vector<std::uint8_t> code = {0x18, 0xFB, 0xC2, 0x30, 0xA2, 0x80, 0x12, 0xE2, 0x10, 0x8A, 0x8F, 0x00, 0x20, 0x7E,
                                      0xC2, 0x30, 0xA9, 0x00, 0x02, 0x1B, 0xA9, 0x5A, 0x01, 0xE2, 0x20, 0x1B, 0x48,
                                      0xA9, 0x7E, 0x48, 0xAB, 0xC2, 0x30, 0xA2, 0x10, 0x00, 0xA9, 0x5A, 0x00, 0x9D, 0xF0, 0xFF};
    finishIdle(code);
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(code);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x7E2000) == 0x80);
    REQUIRE(system.debugRead(0x7E2001) == 0xFF);
    REQUIRE(system.debugRead(0x00015A) == 0x5A);
    REQUIRE(system.debugRead(0x7F0000) == 0x5A);
    REQUIRE(system.debugRead(0x7E0000) == 0x00);
}

TEST_CASE("a 16-bit port store can finish an SPC transfer") {
    std::vector<std::uint8_t> code = {0x18, 0xFB, 0xC2, 0x20, 0xAD, 0x40, 0x21, 0xC9, 0xAA, 0xBB, 0xD0, 0xF8, 0xA9, 0x00, 0x02, 0x8D, 0x42, 0x21,
                                      0xA9, 0xCC, 0x01, 0x8D, 0x40, 0x21, 0xE2, 0x20, 0xAD, 0x40, 0x21, 0xC9, 0xCC, 0xD0, 0xF9, 0xA2, 0x00};
    const std::uint8_t payload[] = {0x8F, 0x5A, 0xF4, 0x2F, 0xFE};
    for (const std::uint8_t byte : payload) {
        code.insert(code.end(), {0xA9, byte, 0x8D, 0x41, 0x21, 0x8E, 0x40, 0x21, 0xEC, 0x40, 0x21, 0xD0, 0xFB, 0xE8});
    }
    code.insert(code.end(), {0xC2, 0x20, 0xA9, 0x07, 0x00, 0x8D, 0x40, 0x21});
    finishIdle(code);
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(code);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x2140) == 0x5A);
}

TEST_CASE("vblank sets the NMI flag while NMI is disabled") {
    const std::uint8_t program[] = {0xA9, 0x00, 0x8D, 0x00, 0x42, 0xAD, 0x10, 0x42, 0x10, 0xFB, 0xA9, 0x5A, 0x85, 0x10, 0x4C, 0x0E, 0x80};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(std::vector<std::uint8_t>(program, program + sizeof(program)));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x5A);
    REQUIRE(system.info().detail.find("Stopped") == std::string::npos);
}

TEST_CASE("a Super NES save state restores work RAM and refuses another game") {
    std::vector<std::uint8_t> code = {0x18, 0xFB, 0xC2, 0x20, 0xA9, 0x5A, 0x00, 0x8F, 0x10, 0x00, 0x7E};
    finishIdle(code);
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(code);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x7E0010) == 0x5A);
    std::vector<std::uint8_t> bytes;
    REQUIRE(system.saveState(bytes, "checkpoint", error));
    iris::StatePreview preview;
    system.inspectState(bytes.data(), bytes.size(), preview);
    REQUIRE(preview.readable);
    REQUIRE(preview.sameGame);
    REQUIRE(preview.description == "checkpoint");
    REQUIRE(preview.thumbnailWidth == 64);
    REQUIRE(preview.thumbnailHeight == 56);
    REQUIRE(preview.thumbnailRgba.size() == 64 * 56 * 4);
    system.powerCycle();
    REQUIRE(system.debugRead(0x7E0010) == 0x00);
    REQUIRE(system.loadState(bytes.data(), bytes.size(), error));
    REQUIRE(system.debugRead(0x7E0010) == 0x5A);
    std::vector<std::uint8_t> damaged = bytes;
    damaged.back() ^= 0xFF;
    REQUIRE_FALSE(system.loadState(damaged.data(), damaged.size(), error));
    REQUIRE(system.debugRead(0x7E0010) == 0x5A);
    std::vector<std::uint8_t> otherCode = code;
    otherCode[5] = 0x11;
    iris::systems::snes::SnesSystem other;
    const std::vector<std::uint8_t> otherRom = loRom(otherCode);
    REQUIRE(other.loadBytes(otherRom.data(), otherRom.size(), error));
    other.runFrame();
    REQUIRE(other.debugRead(0x7E0010) == 0x11);
    REQUIRE_FALSE(other.loadState(bytes.data(), bytes.size(), error));
    REQUIRE(other.debugRead(0x7E0010) == 0x11);
}

TEST_CASE("the DSP address keeps the high bit that a driver compares") {
    const std::vector<std::uint8_t> program = {0x8F, 0x7F, 0xF2, 0x98, 0x10, 0xF2, 0xE4, 0xF2, 0xC4, 0xF4, 0x2F, 0xFE};
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(transferProgram(0x0200, {{static_cast<std::uint16_t>(0x0200), program}}));
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x2140) == 0x8F);
}

TEST_CASE("a Super NES path selects that system and a NES path does not") {
    REQUIRE(iris::systemKindForPath("game.sfc") == iris::SystemKind::Snes);
    REQUIRE(iris::systemKindForPath("game.smc") == iris::SystemKind::Snes);
    REQUIRE(iris::systemKindForPath("game.nes") == iris::SystemKind::Nes);
    REQUIRE(std::string(iris::systemKindName(iris::SystemKind::Snes)) == "Super NES");
}

TEST_CASE("vblank reloads the sprite address before the next object list") {
    std::vector<std::uint8_t> program = {
        0x78, 0x9C, 0x02, 0x21, 0x9C, 0x03, 0x21, 0xA9, 0x01, 0x8D, 0x04, 0x21, 0xA9, 0x02, 0x8D, 0x04, 0x21,
        0xA9, 0x03, 0x8D, 0x04, 0x21, 0xA9, 0x04, 0x8D, 0x04, 0x21, 0xA9, 0x80, 0x8D, 0x00, 0x42, 0xCB, 0xA9,
        0x10, 0x8D, 0x04, 0x21, 0xA9, 0x20, 0x8D, 0x04, 0x21, 0xA9, 0x30, 0x8D, 0x04, 0x21, 0xA9, 0x40, 0x8D,
        0x04, 0x21, 0x9C, 0x02, 0x21, 0xDB, 0x40,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    std::vector<std::uint8_t> rom = loRom(program);
    const auto handler = static_cast<std::uint16_t>(0x8000 + program.size() - 1);
    rom[0x7FFA] = static_cast<std::uint8_t>(handler);
    rom[0x7FFB] = static_cast<std::uint8_t>(handler >> 8);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x2138) == 0x10);
    REQUIRE(system.debugRead(0x2138) == 0x20);
    REQUIRE(system.debugRead(0x2138) == 0x30);
    REQUIRE(system.debugRead(0x2138) == 0x40);
}

TEST_CASE("SETINI overscan grows the picture and exotic map modes stay refused") {
    std::vector<std::uint8_t> program;
    store(program, 0x04, 0x33, 0x21);
    store(program, 0x0F, 0x00, 0x21);
    finishIdle(program);
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> rom = loRom(program);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    system.runFrame();
    REQUIRE(system.frame().height == 239);

    std::vector<std::uint8_t> exotic = loRom({});
    exotic[0x7FD5] = 0x26;  // map mode 6 stays refused
    REQUIRE_FALSE(system.loadBytes(exotic.data(), exotic.size(), error));
    REQUIRE(error.find("not supported") != std::string::npos);
}

TEST_CASE("Cx4 and SA-1 bitstream slices answer the cartridge") {
    std::vector<std::uint8_t> cx4;
    store(cx4, 0xA5, 0x00, 0x60);
    cx4.insert(cx4.end(), {0xAD, 0x00, 0x60, 0x8D, 0x10, 0x00});
    finishIdle(cx4);
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> cx4Rom = boardRom(cx4, 0x20, 0xF3);
    REQUIRE(system.loadBytes(cx4Rom.data(), cx4Rom.size(), error));
    REQUIRE(system.info().detail == "LoROM, NTSC, Cx4");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0xA5);

    std::vector<std::uint8_t> bits;
    store(bits, 0xA5, 0x58, 0x22);
    store(bits, 0x5A, 0x59, 0x22);
    store(bits, 0x00, 0x5A, 0x22);
    store(bits, 0x08, 0x5B, 0x22);
    bits.insert(bits.end(), {0xAD, 0x0E, 0x23, 0x8D, 0x10, 0x00});
    finishIdle(bits);
    const std::vector<std::uint8_t> sa1Rom = boardRom(bits, 0x23, 0x35);
    REQUIRE(system.loadBytes(sa1Rom.data(), sa1Rom.size(), error));
    REQUIRE(system.info().detail == "SA-1, NTSC");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0xA5);

    std::vector<std::uint8_t> sdd1;
    finishIdle(sdd1);
    const std::vector<std::uint8_t> sdd1Rom = boardRom(sdd1, 0x20, 0x43);
    REQUIRE(system.loadBytes(sdd1Rom.data(), sdd1Rom.size(), error));
    REQUIRE(system.info().detail == "LoROM, NTSC, S-DD1");
}

TEST_CASE("SA-1 arithmetic, character conversion, and Cx4 multiply deepen chip paths") {
    std::vector<std::uint8_t> arith;
    store(arith, 0x00, 0x50, 0x22);
    store(arith, 0x07, 0x51, 0x22);
    store(arith, 0x00, 0x52, 0x22);
    store(arith, 0x06, 0x53, 0x22);
    store(arith, 0x00, 0x54, 0x22);
    arith.insert(arith.end(), {0xAD, 0x06, 0x23, 0x8D, 0x10, 0x00});
    arith.insert(arith.end(), {0xAD, 0x07, 0x23, 0x8D, 0x11, 0x00});
    store(arith, 0x01, 0x50, 0x22);
    store(arith, 100, 0x51, 0x22);
    store(arith, 0x00, 0x52, 0x22);
    store(arith, 0x07, 0x53, 0x22);
    store(arith, 0x00, 0x54, 0x22);
    arith.insert(arith.end(), {0xAD, 0x06, 0x23, 0x8D, 0x12, 0x00});
    arith.insert(arith.end(), {0xAD, 0x08, 0x23, 0x8D, 0x13, 0x00});
    finishIdle(arith);
    iris::systems::snes::SnesSystem system;
    std::string error;
    const std::vector<std::uint8_t> arithRom = boardRom(arith, 0x23, 0x35);
    REQUIRE(system.loadBytes(arithRom.data(), arithRom.size(), error));
    REQUIRE(system.info().detail == "SA-1, NTSC");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 42);
    REQUIRE(system.debugRead(0x0011) == 0);
    REQUIRE(system.debugRead(0x0012) == 14);
    REQUIRE(system.debugRead(0x0013) == 2);

    std::vector<std::uint8_t> cc2;
    store(cc2, 0x00, 0x35, 0x22);
    store(cc2, 0x00, 0x36, 0x22);
    store(cc2, 0x00, 0x37, 0x22);
    store(cc2, 0x02, 0x31, 0x22);
    store(cc2, 0xB0, 0x30, 0x22);
    for (std::uint8_t index = 0; index < 8; ++index) {
        store(cc2, 0x01, static_cast<std::uint8_t>(0x40 + index), 0x22);
    }
    for (std::uint8_t index = 8; index < 15; ++index) {
        store(cc2, 0x00, static_cast<std::uint8_t>(0x40 + index), 0x22);
    }
    store(cc2, 0x00, 0x4F, 0x22);
    cc2.insert(cc2.end(), {0xAD, 0x00, 0x30, 0x8D, 0x10, 0x00});
    cc2.insert(cc2.end(), {0xAD, 0x01, 0x30, 0x8D, 0x11, 0x00});
    finishIdle(cc2);
    const std::vector<std::uint8_t> cc2Rom = boardRom(cc2, 0x23, 0x35);
    REQUIRE(system.loadBytes(cc2Rom.data(), cc2Rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0xFF);
    REQUIRE(system.debugRead(0x0011) == 0x00);

    std::vector<std::uint8_t> cx4;
    store(cx4, 0x03, 0x40, 0x7F);
    store(cx4, 0x00, 0x41, 0x7F);
    store(cx4, 0x05, 0x42, 0x7F);
    store(cx4, 0x00, 0x43, 0x7F);
    store(cx4, 0x05, 0x4F, 0x7F);
    cx4.insert(cx4.end(), {0xAD, 0x50, 0x7F, 0x8D, 0x10, 0x00});
    cx4.insert(cx4.end(), {0xAD, 0x51, 0x7F, 0x8D, 0x11, 0x00});
    cx4.insert(cx4.end(), {0xAD, 0x52, 0x7F, 0x8D, 0x12, 0x00});
    cx4.insert(cx4.end(), {0xAD, 0x53, 0x7F, 0x8D, 0x13, 0x00});
    finishIdle(cx4);
    const std::vector<std::uint8_t> cx4Rom = boardRom(cx4, 0x20, 0xF3);
    REQUIRE(system.loadBytes(cx4Rom.data(), cx4Rom.size(), error));
    REQUIRE(system.info().detail == "LoROM, NTSC, Cx4");
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 15);
    REQUIRE(system.debugRead(0x0011) == 0);
    REQUIRE(system.debugRead(0x0012) == 0);
    REQUIRE(system.debugRead(0x0013) == 0);
}

TEST_CASE("Mode 5 interlace fields sample alternate source rows") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    std::vector<std::uint8_t> program;
    store(program, 0x80, 0x15, 0x21);
    store(program, 0x00, 0x16, 0x21);
    store(program, 0x10, 0x17, 0x21);
    // Tile 0 row0 = color 1, row1 = color 2 (4bpp Mode 5).
    store(program, 0xFF, 0x18, 0x21);
    store(program, 0x00, 0x19, 0x21);
    store(program, 0x00, 0x18, 0x21);
    store(program, 0xFF, 0x19, 0x21);
    store(program, 0x00, 0x16, 0x21);
    store(program, 0x00, 0x17, 0x21);
    store(program, 0x00, 0x18, 0x21);
    store(program, 0x04, 0x19, 0x21);
    store(program, 0x00, 0x21, 0x21);
    store(program, 0x1F, 0x22, 0x21);
    store(program, 0x00, 0x22, 0x21);
    store(program, 0x11, 0x21, 0x21);
    store(program, 0x1F, 0x22, 0x21);
    store(program, 0x00, 0x22, 0x21);
    store(program, 0x12, 0x21, 0x21);
    store(program, 0xE0, 0x22, 0x21);
    store(program, 0x03, 0x22, 0x21);
    store(program, 0x01, 0x0B, 0x21);
    store(program, 0x05, 0x05, 0x21);
    store(program, 0x01, 0x2C, 0x21);
    store(program, 0x01, 0x33, 0x21);  // SETINI interlace
    store(program, 0x0F, 0x00, 0x21);
    const auto loop = static_cast<std::uint8_t>(program.size());
    program.insert(program.end(), {0x4C, loop, 0x80});
    const std::vector<std::uint8_t> rom = loRom(program);
    REQUIRE(system.loadBytes(rom.data(), rom.size(), error));
    // First frame writes SETINI after the line-0 field toggle; warm up so the
    // next beginLine sees interlace enabled.
    system.runFrame();
    REQUIRE(system.frame().width == 512);
    system.runFrame();
    // Odd field samples fine row 1 → green at CGRAM $12.
    REQUIRE(system.frame().rgba[0] == 0);
    REQUIRE(system.frame().rgba[1] == 255);
    REQUIRE(system.frame().rgba[2] == 0);
    system.runFrame();
    // Even field samples fine row 0 → red at CGRAM $11.
    REQUIRE(system.frame().rgba[0] == 255);
    REQUIRE(system.frame().rgba[1] == 0);
    REQUIRE(system.frame().rgba[2] == 0);
}

TEST_CASE("SLHV latches OPHCT/OPVCT and PPU open bus retains the last read") {
    // Wait for $4212 vblank (same pattern as the auto-joy tests), then latch HV.
    const std::vector<std::uint8_t> program = {
        0x78, 0xAD, 0x12, 0x42, 0x10, 0xFB, 0xAD, 0x37, 0x21, 0xAD, 0x3D, 0x21, 0x85, 0x10, 0xAD, 0x3D,
        0x21, 0x85, 0x11, 0xAD, 0x3F, 0x21, 0x85, 0x12, 0xAD, 0x3B, 0x21, 0x85, 0x13, 0xDB,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(loRom(program).data(), loRom(program).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) >= 225);
    REQUIRE(system.debugRead(0x0012) == system.debugRead(0x0013));
}

TEST_CASE("NTSC short line and PAL line count fix the master clock budget") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    std::vector<std::uint8_t> program;
    finishIdle(program);
    REQUIRE(system.loadBytes(loRom(program).data(), loRom(program).size(), error));
    system.runFrame();
    REQUIRE(system.lastFrameMasterClocks() == 262 * 1364 - 4);
    system.setRegionChoice(iris::RegionChoice::Pal);
    system.runFrame();
    REQUIRE(system.lastFrameMasterClocks() == 312 * 1364);
    REQUIRE(system.framePeriodNs() == 1364LL * 312 * 1000000000LL / 21281370LL);
}

TEST_CASE("unaligned 16-bit WRAM stores survive page-cross cycle billing") {
    // Native mode, 16-bit A, store $1234 at $00FF/$0100, then stop.
    const std::vector<std::uint8_t> program = {
        0x18, 0xFB, 0xC2, 0x20, 0xA9, 0x34, 0x12, 0x8D, 0xFF, 0x00, 0xE2, 0x20, 0xAD, 0xFF, 0x00, 0x85,
        0x10, 0xAD, 0x00, 0x01, 0x85, 0x11, 0xDB,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(loRom(program).data(), loRom(program).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x34);
    REQUIRE(system.debugRead(0x0011) == 0x12);
}

TEST_CASE("multitap, mouse, and Super Scope report through auto-read and HV latch") {
    const std::vector<std::uint8_t> program = {
        0x78, 0xA9, 0x01, 0x8D, 0x00, 0x42, 0xAD, 0x12, 0x42, 0x10, 0xFB, 0xAD, 0x12, 0x42, 0x29, 0x01,
        0xD0, 0xF9, 0xAD, 0x18, 0x42, 0x85, 0x20, 0xAD, 0x19, 0x42, 0x85, 0x21, 0xAD, 0x1C, 0x42, 0x85,
        0x22, 0xAD, 0x1D, 0x42, 0x85, 0x23, 0xDB,
    };
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(loRom(program).data(), loRom(program).size(), error));
    system.setInputDevice(0, iris::systems::snes::Bus::InputDevice::Multitap);
    system.setController(0, iris::kButtonB);
    system.setController(2, iris::kButtonStart);
    system.runFrame();
    REQUIRE(system.debugRead(0x0021) == 0x80);  // B on JOY1H
    REQUIRE(system.debugRead(0x0023) == 0x10);  // Start on JOY3H

    std::vector<std::uint8_t> mouse;
    finishIdle(mouse);
    REQUIRE(system.loadBytes(loRom(mouse).data(), loRom(mouse).size(), error));
    system.setInputDevice(0, iris::systems::snes::Bus::InputDevice::Mouse);
    system.setPointer(5, 9);
    system.setController(0, iris::kButtonA);
    system.runFrame();
    REQUIRE((system.debugRead(0x4219) & 0x01) != 0);

    // Fire latches HV at the pointer; the program must not rewrite SLHV.
    const std::vector<std::uint8_t> gun = {
        0x78, 0xAD, 0x3C, 0x21, 0x85, 0x30, 0xAD, 0x3D, 0x21, 0x85, 0x31, 0xDB,
    };
    REQUIRE(system.loadBytes(loRom(gun).data(), loRom(gun).size(), error));
    system.setInputDevice(0, iris::systems::snes::Bus::InputDevice::SuperScope);
    system.setPointer(40, 60);
    system.setController(0, iris::kButtonA);
    system.runFrame();
    REQUIRE(system.debugRead(0x0030) == 40);
    REQUIRE(system.debugRead(0x0031) == 60);
}

TEST_CASE("DSP-1 scale and DSP-4 projection commands answer with host arithmetic") {
    std::vector<std::uint8_t> dsp;
    staLong(dsp, 0x03, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    ldaLongSta(dsp, 0x308000, 0x0010);
    ldaLongSta(dsp, 0x308000, 0x0011);
    finishIdle(dsp);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(boardRom(dsp, 0x20, 0x03).data(), boardRom(dsp, 0x20, 0x03).size(), error));
    system.runFrame();
    // $4000 * $4000 >> 15 = $2000
    REQUIRE(system.debugRead(0x0010) == 0x00);
    REQUIRE(system.debugRead(0x0011) == 0x20);

    std::vector<std::uint8_t> dsp4;
    staLong(dsp4, 0x00, 0x308000);
    staLong(dsp4, 0x00, 0x308000);
    staLong(dsp4, 0x10, 0x308000);
    staLong(dsp4, 0x00, 0x308000);
    staLong(dsp4, 0x00, 0x308000);
    staLong(dsp4, 0x00, 0x308000);
    staLong(dsp4, 0x00, 0x308000);
    staLong(dsp4, 0x40, 0x308000);
    ldaLongSta(dsp4, 0x308000, 0x0020);
    ldaLongSta(dsp4, 0x308000, 0x0021);
    finishIdle(dsp4);
    REQUIRE(system.loadBytes(boardRom(dsp4, 0x20, 0x45).data(), boardRom(dsp4, 0x20, 0x45).size(), error));
    REQUIRE(system.info().detail.find("DSP-4") != std::string::npos);
    system.runFrame();
    // horizon $4000 / distance $0010 = $0400
    REQUIRE(system.debugRead(0x0020) == 0x00);
    REQUIRE(system.debugRead(0x0021) == 0x04);
}

TEST_CASE("Super FX WITH/MULT/IBT and SA-1 type-1 character conversion") {
    // IBT R1,#3 / WITH R1 / MULT R1 (3*3) / STOP — result in R1.
    const std::uint8_t gsuCode[] = {0xA1, 0x03, 0x21, 0x81, 0x00};
    std::vector<std::uint8_t> fx;
    for (std::size_t index = 0; index < sizeof(gsuCode); ++index) {
        staLong(fx, gsuCode[index], static_cast<std::uint32_t>(0x700000 + index));
    }
    store(fx, 0x70, 0x34, 0x30);
    store(fx, 0x00, 0x1E, 0x30);
    store(fx, 0x00, 0x1F, 0x30);
    store(fx, 0x28, 0x3A, 0x30);  // SCMR high-speed bit
    store(fx, 0x20, 0x30, 0x30);
    finishIdle(fx);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(boardRom(fx, 0x20, 0x14).data(), boardRom(fx, 0x20, 0x14).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x3002) == 9);
    REQUIRE(system.debugRead(0x3003) == 0);

    // Type-1 char conv: 2bpp tile of color-1 pixels in BW-RAM → I-RAM plane 0 = $FF.
    std::vector<std::uint8_t> sa1;
    for (int index = 0; index < 64; ++index) {
        staLong(sa1, 0x01, static_cast<std::uint32_t>(0x400000 + index));
    }
    store(sa1, 0x00, 0x32, 0x22);
    store(sa1, 0x00, 0x33, 0x22);
    store(sa1, 0x00, 0x34, 0x22);
    store(sa1, 0x00, 0x35, 0x22);
    store(sa1, 0x00, 0x36, 0x22);
    store(sa1, 0x00, 0x37, 0x22);
    store(sa1, 0x02, 0x31, 0x22);  // CDMA 2bpp
    store(sa1, 0xA0, 0x30, 0x22);  // DMAEN+CDEN type-1
    sa1.insert(sa1.end(), {0xAD, 0x00, 0x30, 0x8D, 0x10, 0x00});
    finishIdle(sa1);
    REQUIRE(system.loadBytes(boardRom(sa1, 0x23, 0x35).data(), boardRom(sa1, 0x23, 0x35).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0xFF);
}

TEST_CASE("Cx4 distance, S-DD1 stream, SPC7110 port, OBC-1 remap, and map mode 4") {
    std::vector<std::uint8_t> cx4;
    store(cx4, 0x03, 0x40, 0x7F);
    store(cx4, 0x00, 0x41, 0x7F);
    store(cx4, 0x04, 0x42, 0x7F);
    store(cx4, 0x00, 0x43, 0x7F);
    store(cx4, 0x02, 0x4F, 0x7F);  // distance
    cx4.insert(cx4.end(), {0xAD, 0x50, 0x7F, 0x8D, 0x10, 0x00});
    finishIdle(cx4);
    iris::systems::snes::SnesSystem system;
    std::string error;
    REQUIRE(system.loadBytes(boardRom(cx4, 0x20, 0xF3).data(), boardRom(cx4, 0x20, 0xF3).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 5);  // sqrt(3^2+4^2)

    std::vector<std::uint8_t> sdd1;
    // Point decompress window at ROM $0100 (away from the program at $8000).
    store(sdd1, 0x00, 0x02, 0x48);
    store(sdd1, 0x01, 0x03, 0x48);
    store(sdd1, 0x00, 0x04, 0x48);
    store(sdd1, 0x01, 0x01, 0x48);
    ldaLongSta(sdd1, 0x700000, 0x0011);
    finishIdle(sdd1);
    std::vector<std::uint8_t> sdd1Rom = boardRom(sdd1, 0x20, 0x43);
    sdd1Rom[0x100] = 0x5A;
    REQUIRE(system.loadBytes(sdd1Rom.data(), sdd1Rom.size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0011) == 0x5A);

    std::vector<std::uint8_t> spc;
    store(spc, 0x00, 0x05, 0x48);
    store(spc, 0x02, 0x06, 0x48);
    store(spc, 0x00, 0x07, 0x48);
    store(spc, 0x01, 0x04, 0x48);
    spc.insert(spc.end(), {0xAD, 0x00, 0x48, 0x8D, 0x12, 0x00});
    finishIdle(spc);
    std::vector<std::uint8_t> spcRom = boardRom(spc, 0x20, 0xF5);
    spcRom[0x200] = 0xA5;
    REQUIRE(system.loadBytes(spcRom.data(), spcRom.size(), error));
    REQUIRE(system.info().detail.find("SPC7110") != std::string::npos);
    system.runFrame();
    REQUIRE(system.debugRead(0x0012) == 0xA5);

    std::vector<std::uint8_t> obc;
    store(obc, 0x80, 0xF0, 0x7F);
    store(obc, 0x00, 0xF1, 0x7F);
    store(obc, 0x3C, 0x00, 0x60);  // write through remap base $80
    obc.insert(obc.end(), {0xAD, 0x00, 0x60, 0x8D, 0x13, 0x00});
    finishIdle(obc);
    REQUIRE(system.loadBytes(boardRom(obc, 0x20, 0x65).data(), boardRom(obc, 0x20, 0x65).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0013) == 0x3C);

    std::vector<std::uint8_t> map4 = loRom({});
    map4[0x7FD5] = 0x24;  // uncommon LoROM variant
    REQUIRE(system.loadBytes(map4.data(), map4.size(), error));
    REQUIRE(system.info().detail.find("LoROM*") != std::string::npos);
}

TEST_CASE("SNES hardware acceptance suite freeze covers timing APU PPU and chips") {
    iris::systems::snes::SnesSystem system;
    std::string error;
    // Timing: NTSC master clocks and DRAM/short-line budget.
    std::vector<std::uint8_t> idle;
    finishIdle(idle);
    REQUIRE(system.loadBytes(loRom(idle).data(), loRom(idle).size(), error));
    system.runFrame();
    REQUIRE(system.lastFrameMasterClocks() == 262 * 1364 - 4);
    // PPU: Mode 5 width.
    const std::vector<std::uint8_t> mode5Rom = loRom(backgroundProgram(5, 0x04, false, 0, 0));
    REQUIRE(system.loadBytes(mode5Rom.data(), mode5Rom.size(), error));
    system.runFrame();
    REQUIRE(system.frame().width == 512);
    // APU: software IPL still answers AA/BB before a transfer.
    REQUIRE(system.debugRead(0x2140) == 0xAA);
    REQUIRE(system.debugRead(0x2141) == 0xBB);
    // Chip: DSP-1 still multiplies.
    std::vector<std::uint8_t> dsp;
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    staLong(dsp, 0x00, 0x308000);
    staLong(dsp, 0x40, 0x308000);
    ldaLongSta(dsp, 0x308000, 0x0010);
    ldaLongSta(dsp, 0x308000, 0x0011);
    finishIdle(dsp);
    REQUIRE(system.loadBytes(boardRom(dsp, 0x20, 0x03).data(), boardRom(dsp, 0x20, 0x03).size(), error));
    system.runFrame();
    REQUIRE(system.debugRead(0x0010) == 0x00);
    REQUIRE(system.debugRead(0x0011) == 0x20);
}
