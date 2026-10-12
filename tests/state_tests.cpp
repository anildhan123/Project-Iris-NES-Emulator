#include "iris/nes/demo_rom.hpp"
#include "iris/nes/nes_system.hpp"
#include "iris/paths.hpp"
#include "iris/sha256.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> copyFrame(const iris::systems::nes::NesSystem& nes) {
    const iris::FrameView frame = nes.frame();
    return std::vector<std::uint8_t>(frame.rgba, frame.rgba + static_cast<std::size_t>(frame.width * frame.height * 4));
}

std::vector<std::uint8_t> tinyRom(int flag6) {
    std::vector<std::uint8_t> bytes(16 + 16384 + 8192, 0);
    bytes[0] = 'N';
    bytes[1] = 'E';
    bytes[2] = 'S';
    bytes[3] = 0x1A;
    bytes[4] = 1;
    bytes[5] = 1;
    bytes[6] = static_cast<std::uint8_t>(flag6);
    bytes[16] = 0xA9;
    bytes[17] = 0xAB;
    bytes[18] = 0x8D;
    bytes[19] = 0x00;
    bytes[20] = 0x60;
    bytes[21] = 0x4C;
    bytes[22] = 0x00;
    bytes[23] = 0x80;
    bytes[16 + 16384 - 4] = 0x00;
    bytes[16 + 16384 - 3] = 0x80;
    bytes[16 + 16384 - 2] = 0x00;
    bytes[16 + 16384 - 1] = 0x80;
    return bytes;
}

}  // namespace

TEST_CASE("sha256 matches the empty-string digest") {
    const std::string digest = iris::sha256Hex(nullptr, 0);
    REQUIRE(digest == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("a save state restores the picture and rejects a damaged file") {
    const auto rom = iris::systems::nes::makeIrisDemoRom();
    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "iris_demo", error));
    for (int frame = 0; frame < 20; ++frame) {
        nes.runFrame();
    }
    nes.setPaused(true);
    const auto savedPicture = copyFrame(nes);
    std::vector<std::uint8_t> state;
    REQUIRE(nes.saveState(state, "checkpoint", error));
    REQUIRE_FALSE(nes.contentId().empty());

    iris::StatePreview preview;
    nes.inspectState(state.data(), state.size(), preview);
    REQUIRE(preview.readable);
    REQUIRE(preview.sameGame);
    REQUIRE(preview.description == "checkpoint");
    REQUIRE(preview.thumbnailWidth == 64);
    REQUIRE(preview.thumbnailHeight == 60);
    REQUIRE(preview.thumbnailRgba.size() == 64u * 60u * 4u);

    nes.setPaused(false);
    nes.setController(0, iris::kButtonRight);
    for (int frame = 0; frame < 15; ++frame) {
        nes.runFrame();
    }
    REQUIRE(copyFrame(nes) != savedPicture);

    REQUIRE(nes.loadState(state.data(), state.size(), error));
    REQUIRE(nes.paused());
    REQUIRE(copyFrame(nes) == savedPicture);

    std::vector<std::uint8_t> damaged = state;
    damaged.back() ^= 0xFF;
    REQUIRE_FALSE(nes.loadState(damaged.data(), damaged.size(), error));
    REQUIRE(error.find("damaged") != std::string::npos);
    REQUIRE(copyFrame(nes) == savedPicture);

    damaged = state;
    damaged.resize(12);
    REQUIRE_FALSE(nes.loadState(damaged.data(), damaged.size(), error));
    REQUIRE(copyFrame(nes) == savedPicture);
}

TEST_CASE("a state from one cartridge is refused by another") {
    const auto demo = iris::systems::nes::makeIrisDemoRom();
    const auto other = tinyRom(0);
    iris::systems::nes::NesSystem source;
    iris::systems::nes::NesSystem destination;
    std::string error;
    REQUIRE(source.loadBytes(demo.data(), demo.size(), "demo", error));
    source.runFrame();
    std::vector<std::uint8_t> state;
    REQUIRE(source.saveState(state, "", error));
    REQUIRE(destination.loadBytes(other.data(), other.size(), "other", error));
    destination.runFrame();
    REQUIRE_FALSE(destination.loadState(state.data(), state.size(), error));
    REQUIRE(error.find("different game") != std::string::npos);
    REQUIRE(destination.debugRead(0x6000) == 0xAB);
}

TEST_CASE("battery RAM round-trips without being confused with a state") {
    const auto rom = tinyRom(0x02);
    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "battery", error));
    REQUIRE(nes.info().batteryBacked);
    nes.runFrame();
    REQUIRE(nes.debugRead(0x6000) == 0xAB);
    std::vector<std::uint8_t> battery;
    REQUIRE(nes.copyBattery(battery));
    REQUIRE(battery.size() == 8192);
    REQUIRE(battery[0] == 0xAB);
    battery[0] = 0xCD;
    REQUIRE(nes.restoreBattery(battery.data(), battery.size(), error));
    REQUIRE(nes.debugRead(0x6000) == 0xCD);
    REQUIRE_FALSE(nes.restoreBattery(battery.data(), 4, error));
    REQUIRE(nes.debugRead(0x6000) == 0xCD);
}

TEST_CASE("an MMC1 bank switch survives a state and a reset") {
    std::vector<std::uint8_t> rom(16 + 32768 + 8192, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 2;
    rom[5] = 1;
    rom[6] = 0x10;
    rom[16 + 0x100] = 0xA5;
    rom[16 + 0x4100] = 0x5A;
    const std::uint8_t program[] = {0xAD, 0x00, 0x81, 0x85, 0x11, 0xA9, 0x01, 0x8D, 0x00, 0xE0, 0xA9, 0x00, 0x8D,
                                    0x00, 0xE0, 0x8D, 0x00, 0xE0, 0x8D, 0x00, 0xE0, 0x8D, 0x00, 0xE0, 0xAD, 0x00,
                                    0x81, 0x85, 0x12, 0x4C, 0x1D, 0xC0};
    for (std::size_t index = 0; index < sizeof(program); ++index) {
        rom[16 + 0x4000 + index] = program[index];
    }
    rom[16 + 32768 - 4] = 0x00;
    rom[16 + 32768 - 3] = 0xC0;

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "mmc1", error));
    nes.runFrame();
    REQUIRE(nes.debugRead(0x8100) == 0x5A);
    std::vector<std::uint8_t> state;
    REQUIRE(nes.saveState(state, "", error));
    nes.reset();
    REQUIRE(nes.debugRead(0x8100) == 0xA5);
    REQUIRE(nes.loadState(state.data(), state.size(), error));
    REQUIRE(nes.debugRead(0x8100) == 0x5A);
}

TEST_CASE("binary files are replaced atomically") {
    iris::test::TemporaryDirectory directory;
    const auto destination = directory.path() / "slot.irstate";
    const std::uint8_t first[] = {1, 2, 3, 4};
    REQUIRE(iris::writeBinaryFileAtomically(destination, first, sizeof(first)));
    const std::uint8_t second[] = {9, 8, 7};
    REQUIRE(iris::writeBinaryFileAtomically(destination, second, sizeof(second)));
    std::ifstream input(destination, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    REQUIRE(bytes.size() == 3);
    REQUIRE(bytes[0] == 9);
    REQUIRE(bytes[1] == 8);
    REQUIRE(bytes[2] == 7);
}
