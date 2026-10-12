#include "iris/nes/bus.hpp"
#include "iris/nes/demo_rom.hpp"
#include "iris/nes/nes_system.hpp"
#include "iris/nes/palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

namespace {

int minXOf(const iris::FrameView& frame, iris::systems::nes::Rgb color) {
    int found = frame.width;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::uint8_t* pixel = frame.rgba + (static_cast<std::size_t>(y) * frame.width + x) * 4;
            if (pixel[0] == color.red && pixel[1] == color.green && pixel[2] == color.blue && found > x) {
                found = x;
            }
        }
    }
    return found;
}

int countColor(const iris::FrameView& frame, iris::systems::nes::Rgb color) {
    int count = 0;
    const std::size_t pixels = static_cast<std::size_t>(frame.width * frame.height);
    for (std::size_t index = 0; index < pixels; ++index) {
        const std::uint8_t* pixel = frame.rgba + index * 4;
        if (pixel[0] == color.red && pixel[1] == color.green && pixel[2] == color.blue) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST_CASE("CPU RAM and controller registers are wired through the bus") {
    iris::systems::nes::Ppu ppu;
    iris::systems::nes::Apu apu;
    iris::systems::nes::Controller first;
    iris::systems::nes::Controller second;
    iris::systems::nes::Bus bus(ppu, apu, first, second);
    bus.power();
    bus.write(0x0000, 0x5A);
    REQUIRE(bus.read(0x0800) == 0x5A);
    REQUIRE(bus.read(0x1800) == 0x5A);

    first.setButtons(iris::kButtonA | iris::kButtonRight);
    bus.write(0x4016, 0x01);
    bus.write(0x4016, 0x00);
    REQUIRE((bus.read(0x4016) & 0x01) == 1);
    REQUIRE((bus.read(0x4016) & 0x01) == 0);
    REQUIRE((bus.read(0x4016) & 0x01) == 0);
    REQUIRE((bus.read(0x4016) & 0x01) == 0);
    REQUIRE((bus.read(0x4016) & 0x01) == 0);
    REQUIRE((bus.read(0x4016) & 0x01) == 0);
    REQUIRE((bus.read(0x4016) & 0x01) == 0);
    REQUIRE((bus.read(0x4016) & 0x01) == 1);
}

TEST_CASE("the original demo draws IRIS and moves a sprite from port 1") {
    const auto rom = iris::systems::nes::makeIrisDemoRom();
    REQUIRE(rom.size() == 16 + 32768 + 8192);
    REQUIRE(rom[0] == 'N');
    REQUIRE(rom[16] == 0x78);

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "Iris Demo", error));
    REQUIRE(nes.info().title == "Iris Demo");
    REQUIRE(nes.info().detail == "NROM, NTSC");

    for (int frame = 0; frame < 30; ++frame) {
        nes.runFrame();
    }
    const auto white = iris::systems::nes::paletteColor(0x30);
    const auto yellow = iris::systems::nes::paletteColor(0x28);
    const auto blue = iris::systems::nes::paletteColor(0x02);
    REQUIRE(countColor(nes.frame(), white) > 100);
    REQUIRE(countColor(nes.frame(), blue) > 100);
    REQUIRE(countColor(nes.frame(), yellow) > 8);
    const int origin = minXOf(nes.frame(), yellow);

    float loudest = 0.0f;
    for (int frame = 0; frame < 10; ++frame) {
        nes.runFrame();
        const auto audio = nes.audio();
        REQUIRE(audio.sampleRate == 48000);
        for (int sample = 0; sample < audio.count; ++sample) {
            loudest = std::max(loudest, std::fabs(audio.samples[sample]));
        }
    }
    REQUIRE(loudest < 0.001f);

    nes.setController(0, iris::kButtonRight);
    for (int frame = 0; frame < 20; ++frame) {
        nes.runFrame();
    }
    REQUIRE(minXOf(nes.frame(), yellow) >= origin + 8);

    nes.setController(0, iris::kButtonA);
    bool heard = false;
    for (int frame = 0; frame < 20 && !heard; ++frame) {
        nes.runFrame();
        const auto audio = nes.audio();
        for (int sample = 0; sample < audio.count; ++sample) {
            if (std::fabs(audio.samples[sample]) > 0.01f) {
                heard = true;
                break;
            }
        }
    }
    REQUIRE(heard);

    const int moved = minXOf(nes.frame(), yellow);
    nes.setPaused(true);
    const auto before = nes.frame().rgba[0];
    nes.runFrame();
    REQUIRE(nes.frame().rgba[0] == before);
    REQUIRE(minXOf(nes.frame(), yellow) == moved);

    nes.reset();
    for (int frame = 0; frame < 30; ++frame) {
        nes.runFrame();
    }
    REQUIRE(minXOf(nes.frame(), yellow) <= origin + 2);
}
