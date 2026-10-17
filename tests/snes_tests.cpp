#include "iris/snes/demo_rom.hpp"
#include "iris/snes/snes_system.hpp"
#include "iris/system_kind.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
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

    std::vector<std::uint8_t> offset;
    store(offset, 0x80, 0x15, 0x21);
    store(offset, 0x10, 0x16, 0x21);
    store(offset, 0x10, 0x17, 0x21);
    store(offset, 0xFF, 0x18, 0x21);
    store(offset, 0x00, 0x19, 0x21);
    store(offset, 0x00, 0x16, 0x21);
    store(offset, 0x00, 0x17, 0x21);
    store(offset, 0x00, 0x18, 0x21);
    store(offset, 0x00, 0x19, 0x21);
    store(offset, 0x01, 0x18, 0x21);
    store(offset, 0x00, 0x19, 0x21);
    store(offset, 0x00, 0x16, 0x21);
    store(offset, 0x04, 0x17, 0x21);
    store(offset, 0x08, 0x18, 0x21);
    store(offset, 0x20, 0x19, 0x21);
    store(offset, 0x00, 0x18, 0x21);
    store(offset, 0x20, 0x19, 0x21);
    store(offset, 0x00, 0x21, 0x21);
    store(offset, 0x1F, 0x22, 0x21);
    store(offset, 0x00, 0x22, 0x21);
    store(offset, 0x01, 0x21, 0x21);
    store(offset, 0x00, 0x22, 0x21);
    store(offset, 0x7C, 0x22, 0x21);
    store(offset, 0x01, 0x0B, 0x21);
    store(offset, 0x05, 0x09, 0x21);
    store(offset, 0x02, 0x05, 0x21);
    store(offset, 0x01, 0x2C, 0x21);
    store(offset, 0x0F, 0x00, 0x21);
    const auto offsetLoop = static_cast<std::uint8_t>(offset.size());
    offset.insert(offset.end(), {0x4C, offsetLoop, 0x80});
    const iris::FrameView shiftedTile = show(offset);
    REQUIRE(shiftedTile.rgba[0] == 0);
    REQUIRE(shiftedTile.rgba[2] == 255);

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
                                          const std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>>& parts) {
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
    page(0x00, 0xF3);
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
    float peak = 0.0f;
    for (int index = 0; index < played.count; ++index) {
        const float sampleValue = played.samples[index] < 0.0f ? -played.samples[index] : played.samples[index];
        if (sampleValue > peak) {
            peak = sampleValue;
        }
    }
    REQUIRE(peak > 0.05f);
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
