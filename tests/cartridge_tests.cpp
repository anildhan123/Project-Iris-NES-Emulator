#include "iris/nes/cartridge.hpp"
#include "iris/nes/nes_system.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

namespace {

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> nromHeader(int prgBanks, int chrBanks, int flag6 = 0, int flag7 = 0) {
    std::vector<std::uint8_t> bytes(16 + static_cast<std::size_t>(prgBanks) * 16384 + static_cast<std::size_t>(chrBanks) * 8192, 0);
    bytes[0] = 'N';
    bytes[1] = 'E';
    bytes[2] = 'S';
    bytes[3] = 0x1A;
    bytes[4] = static_cast<std::uint8_t>(prgBanks);
    bytes[5] = static_cast<std::uint8_t>(chrBanks);
    bytes[6] = static_cast<std::uint8_t>(flag6);
    bytes[7] = static_cast<std::uint8_t>(flag7);
    return bytes;
}

}  // namespace

TEST_CASE("iNES header accepts NROM and reports mirroring") {
    auto bytes = nromHeader(2, 1, 0x01);
    auto parsed = iris::systems::nes::Cartridge::fromBytes(bytes.data(), bytes.size(), "nrom");
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().mapper() == 0);
    REQUIRE(parsed.value().prg().size() == 32768);
    REQUIRE(parsed.value().chr().size() == 8192);
    REQUIRE(parsed.value().mirroring() == iris::systems::nes::Mirroring::Vertical);
    REQUIRE_FALSE(parsed.value().nes20());
}

TEST_CASE("NES 2.0 NROM header is accepted") {
    auto bytes = nromHeader(1, 1, 0, 0x08);
    auto parsed = iris::systems::nes::Cartridge::fromBytes(bytes.data(), bytes.size(), "nes2");
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().nes20());
    REQUIRE(parsed.value().mapper() == 0);
    REQUIRE(parsed.value().prg().size() == 16384);
}

TEST_CASE("malformed cartridges are rejected") {
    std::vector<std::uint8_t> empty(32, 0);
    auto missing = iris::systems::nes::Cartridge::fromBytes(empty.data(), empty.size(), "bad");
    REQUIRE_FALSE(missing.ok());
    REQUIRE(missing.error().code == iris::ErrorCode::ParseFailure);

    auto truncated = nromHeader(1, 1);
    truncated.resize(20);
    auto shortRom = iris::systems::nes::Cartridge::fromBytes(truncated.data(), truncated.size(), "short");
    REQUIRE_FALSE(shortRom.ok());

    auto extra = nromHeader(1, 1);
    extra.push_back(0);
    auto trailing = iris::systems::nes::Cartridge::fromBytes(extra.data(), extra.size(), "extra");
    REQUIRE_FALSE(trailing.ok());
    REQUIRE(trailing.error().message.find("Trailing") != std::string::npos);
}

TEST_CASE("a missing ROM file is reported") {
    iris::test::TemporaryDirectory directory;
    auto parsed = iris::systems::nes::Cartridge::fromFile(directory.path() / "missing.nes");
    REQUIRE_FALSE(parsed.ok());
    REQUIRE(parsed.error().code == iris::ErrorCode::NotFound);
}

TEST_CASE("an unsupported mapper is refused without dropping a loaded game") {
    auto playable = nromHeader(1, 1);
    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(playable.data(), playable.size(), "nrom", error));

    auto bytes = nromHeader(1, 1, 0x30, 0x60);  // mapper 99
    REQUIRE_FALSE(nes.loadBytes(bytes.data(), bytes.size(), "unsupported", error));
    REQUIRE(error.find("Mapper 99") != std::string::npos);
    REQUIRE(nes.loaded());
    REQUIRE(nes.info().detail == "NROM, NTSC");
}

TEST_CASE("NES 2.0 records the console region and rejects a non-NES console") {
    auto pal = nromHeader(1, 1, 0, 0x08);
    pal[12] = 1;
    auto parsed = iris::systems::nes::Cartridge::fromBytes(pal.data(), pal.size(), "pal");
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().region() == iris::systems::nes::Region::Pal);

    auto vs = nromHeader(1, 1, 0, 0x09);
    auto rejected = iris::systems::nes::Cartridge::fromBytes(vs.data(), vs.size(), "vs");
    REQUIRE_FALSE(rejected.ok());
    REQUIRE(rejected.error().message.find("not a standard NES") != std::string::npos);
}

TEST_CASE("CHR RAM is inferred for an iNES ROM with no CHR") {
    auto bytes = nromHeader(1, 0);
    auto parsed = iris::systems::nes::Cartridge::fromBytes(bytes.data(), bytes.size(), "chrram");
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().chrRam());
    REQUIRE(parsed.value().chr().size() == 8192);
}

TEST_CASE("a valid NROM file round-trips from disk") {
    iris::test::TemporaryDirectory directory;
    const auto path = directory.path() / "game.nes";
    const auto bytes = nromHeader(1, 1);
    writeBytes(path, bytes);
    auto parsed = iris::systems::nes::Cartridge::fromFile(path);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().name() == "game");
    REQUIRE(parsed.value().prg().size() == 16384);
}
