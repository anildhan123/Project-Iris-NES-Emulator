#include "iris.h"
#include "iris_bridge.h"

#include "iris/nes/demo_rom.hpp"
#include "iris/sha256.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

TEST_CASE("a null core handle is ignored") {
    iris_destroy(nullptr);
    iris_unload(nullptr);
    iris_reset(nullptr);
    iris_set_paused(nullptr, 1);
    iris_run_frame(nullptr);
    iris_set_controller(nullptr, 0, 0x80);

    char error[64] = "unchanged";
    REQUIRE(iris_load_bytes(nullptr, nullptr, 0, nullptr, error, sizeof(error)) == 0);
    REQUIRE(iris_load_file(nullptr, nullptr, error, sizeof(error)) == 0);
    REQUIRE(iris_loaded(nullptr) == 0);
    REQUIRE(iris_frame(nullptr, nullptr, nullptr) == nullptr);
    REQUIRE(iris_audio(nullptr, nullptr, nullptr) == nullptr);
    REQUIRE(iris_frame_period_ns(nullptr) == 0);
    REQUIRE(iris_content_id(nullptr, error, sizeof(error)) == 0);
    REQUIRE(std::string(iris_version()) == "0.9.0");
}

TEST_CASE("the C core reproduces the pinned demo picture") {
    const auto rom = iris::systems::nes::makeIrisDemoRom();
    IrisSystem* system = iris_create();
    REQUIRE(system != nullptr);

    char error[256] = {};
    REQUIRE(iris_load_bytes(system, rom.data(), rom.size(), "Iris Demo", error, sizeof(error)) == 1);
    REQUIRE(iris_loaded(system) == 1);
    REQUIRE(error[0] == '\0');

    char contentId[65] = {};
    REQUIRE(iris_content_id(system, contentId, sizeof(contentId)) == 1);
    REQUIRE(std::string(contentId).size() == 64);
    REQUIRE(iris_content_id(system, contentId, 64) == 0);

    for (int frame = 0; frame < 90; ++frame) {
        iris_set_controller(system, 0, 0);
        iris_run_frame(system);
    }
    int width = 0;
    int height = 0;
    const std::uint8_t* rgba = iris_frame(system, &width, &height);
    REQUIRE(rgba != nullptr);
    REQUIRE(width == 256);
    REQUIRE(height == 240);
    const std::string hash = iris::sha256Hex(rgba, static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    REQUIRE(hash == "ef26962c33f1a46db91ef4b16eb74ddbb6f66e807ea9cb82d6dd82219b744ba1");

    int sampleCount = 0;
    int sampleRate = 0;
    REQUIRE(iris_audio(system, &sampleCount, &sampleRate) != nullptr);
    REQUIRE(sampleCount > 0);
    REQUIRE(sampleRate > 0);
    REQUIRE(iris_frame_period_ns(system) > 0);

    size_t batterySize = 0;
    REQUIRE(iris_copy_battery(system, nullptr, &batterySize, error, sizeof(error)) == 0);

    size_t stateSize = 0;
    REQUIRE(iris_save_state(system, nullptr, &stateSize, "slot", error, sizeof(error)) == 1);
    REQUIRE(stateSize > 0);
    std::vector<std::uint8_t> state(stateSize);
    REQUIRE(iris_save_state(system, state.data(), &stateSize, "slot", error, sizeof(error)) == 1);
    iris_reset(system);
    REQUIRE(iris_load_state(system, state.data(), stateSize, error, sizeof(error)) == 1);

    iris_unload(system);
    REQUIRE(iris_loaded(system) == 0);
    iris_destroy(system);
}
