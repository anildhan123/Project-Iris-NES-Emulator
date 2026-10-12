#include "iris/nes/demo_rom.hpp"
#include "iris/nes/nes_system.hpp"
#include "iris/sha256.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace {

std::string pictureHash(int frames, std::uint8_t buttons) {
    const auto rom = iris::systems::nes::makeIrisDemoRom();
    iris::systems::nes::NesSystem nes;
    std::string error;
    if (!nes.loadBytes(rom.data(), rom.size(), "Iris Demo", error)) {
        return {};
    }
    for (int frame = 0; frame < frames; ++frame) {
        nes.setController(0, buttons);
        nes.runFrame();
    }
    const iris::FrameView frame = nes.frame();
    if (frame.rgba == nullptr || frame.width <= 0 || frame.height <= 0) {
        return {};
    }
    const std::size_t bytes = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4;
    return iris::sha256Hex(frame.rgba, bytes);
}

std::string audioHash(int frames) {
    const auto rom = iris::systems::nes::makeIrisDemoRom();
    iris::systems::nes::NesSystem nes;
    std::string error;
    if (!nes.loadBytes(rom.data(), rom.size(), "Iris Demo", error)) {
        return {};
    }
    const iris::AudioView audio = [&] {
        for (int frame = 0; frame < frames; ++frame) {
            nes.runFrame();
        }
        return nes.audio();
    }();
    if (audio.samples == nullptr || audio.count <= 0) {
        return {};
    }
    return iris::sha256Hex(reinterpret_cast<const std::uint8_t*>(audio.samples), static_cast<std::size_t>(audio.count) * sizeof(float));
}

}  // namespace

TEST_CASE("the original demo picture and audio stay deterministic") {
    const std::string still = pictureHash(90, 0);
    REQUIRE(still == "ef26962c33f1a46db91ef4b16eb74ddbb6f66e807ea9cb82d6dd82219b744ba1");
    REQUIRE(pictureHash(90, 0) == still);
    REQUIRE(pictureHash(90, iris::kButtonRight) != still);

    const std::string audio = audioHash(90);
    REQUIRE(audio.size() == 64);
    REQUIRE(audioHash(90) == audio);
}

TEST_CASE("the original demo stays inside the frame budget") {
    const auto rom = iris::systems::nes::makeIrisDemoRom();
    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "Iris Demo", error));
    constexpr int kFrames = 180;
    const auto started = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kFrames; ++frame) {
        nes.runFrame();
    }
    const double averageMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count() / kFrames;
    REQUIRE(averageMs < 100.0);
}
