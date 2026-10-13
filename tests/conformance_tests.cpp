#include "iris/nes/cartridge.hpp"
#include "iris/nes/cnrom.hpp"
#include "iris/nes/mapper_factory.hpp"
#include "iris/nes/mmc1.hpp"
#include "iris/nes/nes_system.hpp"
#include "iris/nes/timing.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> makeRom(int mapper, int submapper, int prg16k, int chr8k, int region, bool nes20) {
    const std::size_t prg = static_cast<std::size_t>(prg16k) * 16384;
    const std::size_t chr = static_cast<std::size_t>(chr8k) * 8192;
    std::vector<std::uint8_t> bytes(16 + prg + chr, 0);
    bytes[0] = 'N';
    bytes[1] = 'E';
    bytes[2] = 'S';
    bytes[3] = 0x1A;
    bytes[4] = static_cast<std::uint8_t>(prg16k);
    bytes[5] = static_cast<std::uint8_t>(chr8k);
    bytes[6] = static_cast<std::uint8_t>((mapper & 0x0F) << 4);
    if (nes20) {
        bytes[7] = static_cast<std::uint8_t>(0x08 | (mapper & 0xF0));
        bytes[8] = static_cast<std::uint8_t>((submapper & 0x0F) << 4);
        bytes[12] = static_cast<std::uint8_t>(region & 0x03);
    } else {
        bytes[7] = static_cast<std::uint8_t>(mapper & 0xF0);
    }
    return bytes;
}

void putPrg(std::vector<std::uint8_t>& rom, std::size_t offset, std::initializer_list<std::uint8_t> bytes) {
    std::size_t index = 16 + offset;
    for (const std::uint8_t byte : bytes) {
        rom[index++] = byte;
    }
}

void putVector(std::vector<std::uint8_t>& rom, std::size_t prgBytes, std::uint16_t reset, std::uint16_t irq) {
    const std::size_t base = 16 + prgBytes - 4;
    rom[base] = static_cast<std::uint8_t>(reset & 0xFF);
    rom[base + 1] = static_cast<std::uint8_t>(reset >> 8);
    rom[base + 2] = static_cast<std::uint8_t>(irq & 0xFF);
    rom[base + 3] = static_cast<std::uint8_t>(irq >> 8);
}

float peak(const iris::AudioView& audio) {
    float value = 0.0f;
    for (int index = 0; index < audio.count; ++index) {
        const float sample = audio.samples[index] < 0.0f ? -audio.samples[index] : audio.samples[index];
        if (sample > value) {
            value = sample;
        }
    }
    return value;
}

}  // namespace

TEST_CASE("UxROM switches the low PRG bank and leaves the last bank fixed") {
    auto rom = makeRom(2, 1, 2, 1, 0, true);
    rom[16 + 0x100] = 0x11;
    rom[16 + 0x4100] = 0x22;
    putPrg(rom, 0, {0xAD, 0x00, 0x81, 0x85, 0x10, 0xA9, 0x01, 0x8D, 0x00, 0x80, 0x4C, 0x00, 0xC0});
    putPrg(rom, 0x4000, {0xAD, 0x00, 0x81, 0x85, 0x11, 0x4C, 0x00, 0xC0});
    putVector(rom, 32768, 0x8000, 0x8000);

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "uxrom", error));
    REQUIRE(nes.info().detail == "UxROM, NTSC");
    nes.runFrame();
    REQUIRE(nes.debugRead(0x10) == 0x11);
    REQUIRE(nes.debugRead(0x11) == 0x22);
    REQUIRE(nes.debugRead(0xC000) == 0xAD);
}

TEST_CASE("AxROM selects a 32 KiB PRG bank and one nametable") {
    auto rom = makeRom(7, 0, 4, 0, 0, true);
    rom[11] = 7;
    rom[16 + 32] = 0x11;
    rom[16 + 32768 + 32] = 0x22;
    putPrg(rom, 0, {0xAD, 0x20, 0x80, 0x85, 0x10, 0xA9, 0x11, 0x8D, 0x00, 0x80});
    putPrg(rom, 32768 + 10, {0xAD, 0x20, 0x80, 0x85, 0x11, 0x4C, 0x0A, 0x80});
    putVector(rom, 32768, 0x8000, 0x8000);
    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "axrom", error));
    REQUIRE(nes.info().detail == "AxROM, NTSC");
    nes.runFrame();
    REQUIRE(nes.debugRead(0x10) == 0x11);
    REQUIRE(nes.debugRead(0x11) == 0x22);
}

TEST_CASE("CNROM bank selects 8 KiB CHR and submapper 0 has a bus conflict") {
    auto rom = makeRom(3, 0, 1, 2, 0, true);
    auto parsed = iris::systems::nes::Cartridge::fromBytes(rom.data(), rom.size(), "cnrom");
    REQUIRE(parsed.ok());
    iris::systems::nes::Cartridge cart = parsed.value();
    cart.chr()[0] = 0x11;
    cart.chr()[8192] = 0x22;
    iris::systems::nes::Cnrom mapper(cart);
    REQUIRE(mapper.readChr(0) == 0x11);
    mapper.writeCpu(0x8000, 0x01);
    REQUIRE(mapper.readChr(0) == 0x11);

    cart.prg()[0] = 0xFF;
    mapper.writeCpu(0x8000, 0x01);
    REQUIRE(mapper.readChr(0) == 0x22);
}

TEST_CASE("MMC1 loads a PRG bank from five register writes") {
    auto rom = makeRom(1, 0, 2, 1, 0, false);
    rom[16 + 0x100] = 0xA5;
    rom[16 + 0x4100] = 0x5A;
    putPrg(rom, 0x4000,
           {0xAD, 0x00, 0x81, 0x85, 0x11, 0xA9, 0x01, 0x8D, 0x00, 0xE0, 0xA9, 0x00, 0x8D, 0x00, 0xE0, 0x8D, 0x00, 0xE0,
            0x8D, 0x00, 0xE0, 0x8D, 0x00, 0xE0, 0xAD, 0x00, 0x81, 0x85, 0x12, 0x4C, 0x1D, 0xC0});
    putVector(rom, 32768, 0xC000, 0xC000);

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "mmc1", error));
    REQUIRE(nes.info().detail == "MMC1, NTSC");
    nes.runFrame();
    REQUIRE(nes.debugRead(0x11) == 0xA5);
    REQUIRE(nes.debugRead(0x12) == 0x5A);
}

TEST_CASE("a second MMC1 write in the same instruction is ignored") {
    auto rom = makeRom(1, 0, 4, 1, 0, false);
    auto parsed = iris::systems::nes::Cartridge::fromBytes(rom.data(), rom.size(), "mmc1");
    REQUIRE(parsed.ok());
    iris::systems::nes::Cartridge cart = parsed.value();
    cart.prg()[1 * 16384] = 0x11;
    cart.prg()[3 * 16384] = 0x33;
    iris::systems::nes::Mmc1 mapper(cart);
    mapper.writeCpu(0xE000, 0x01);
    mapper.writeCpu(0xE000, 0x01);
    mapper.cpuCycle();
    mapper.writeCpu(0xE000, 0x00);
    mapper.cpuCycle();
    mapper.writeCpu(0xE000, 0x00);
    mapper.cpuCycle();
    mapper.writeCpu(0xE000, 0x00);
    mapper.cpuCycle();
    mapper.writeCpu(0xE000, 0x00);
    mapper.cpuCycle();
    REQUIRE(mapper.readCpu(0x8000, 0) == 0x11);
}

TEST_CASE("MMC3 raises an IRQ after a filtered A12 rise") {
    auto rom = makeRom(4, 0, 2, 1, 0, false);
    putPrg(rom, 0,
           {0x78, 0xA9, 0x08, 0x8D, 0x00, 0x20, 0xA9, 0x18, 0x8D, 0x01, 0x20, 0xA9, 0x00, 0x8D, 0x00, 0xC0, 0x8D, 0x01,
            0xC0, 0x8D, 0x01, 0xE0, 0x58, 0x4C, 0x17, 0x80});
    putPrg(rom, 0x100, {0xE6, 0x10, 0xA9, 0x00, 0x8D, 0x00, 0xE0, 0x40});
    putVector(rom, 32768, 0x8000, 0x8100);

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "mmc3", error));
    REQUIRE(nes.info().detail == "MMC3, NTSC");
    nes.runFrame();
    REQUIRE(nes.debugRead(0x10) > 0);
}

TEST_CASE("the frame counter raises an IRQ in four-step mode") {
    auto rom = makeRom(0, 0, 2, 1, 0, false);
    putPrg(rom, 0, {0x78, 0xD8, 0xA9, 0x00, 0x8D, 0x17, 0x40, 0x58, 0x4C, 0x08, 0x80});
    putPrg(rom, 0x100, {0xAD, 0x15, 0x40, 0xE6, 0x10, 0x40});
    putVector(rom, 32768, 0x8000, 0x8100);

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "irq", error));
    nes.runFrame();
    nes.runFrame();
    nes.runFrame();
    REQUIRE(nes.debugRead(0x10) > 0);
}

TEST_CASE("DMC playback fetches a sample and moves the mixer") {
    auto rom = makeRom(0, 0, 2, 1, 0, false);
    putPrg(rom, 0,
           {0x78, 0xA9, 0x00, 0x8D, 0x10, 0x40, 0x8D, 0x11, 0x40, 0x8D, 0x12, 0x40, 0x8D, 0x13, 0x40, 0xA9, 0x10, 0x8D,
            0x15, 0x40, 0x4C, 0x14, 0x80});
    rom[16 + 0x4000] = 0xFF;
    putVector(rom, 32768, 0x8000, 0x8000);

    iris::systems::nes::NesSystem nes;
    std::string error;
    REQUIRE(nes.loadBytes(rom.data(), rom.size(), "dmc", error));
    nes.runFrame();
    nes.runFrame();
    REQUIRE(peak(nes.audio()) > 0.01f);
}

TEST_CASE("PAL and Dendy headers select their own frame length") {
    auto ntscRom = makeRom(0, 0, 1, 1, 0, true);
    auto palRom = makeRom(0, 0, 1, 1, 1, true);
    auto dendyRom = makeRom(0, 0, 1, 1, 3, true);
    putPrg(ntscRom, 0, {0x78, 0x4C, 0x00, 0x80});
    putPrg(palRom, 0, {0x78, 0x4C, 0x00, 0x80});
    putVector(ntscRom, 16384, 0x8000, 0x8000);
    putVector(palRom, 16384, 0x8000, 0x8000);

    iris::systems::nes::NesSystem ntsc;
    iris::systems::nes::NesSystem pal;
    iris::systems::nes::NesSystem dendy;
    std::string error;
    REQUIRE(ntsc.loadBytes(ntscRom.data(), ntscRom.size(), "ntsc", error));
    REQUIRE(pal.loadBytes(palRom.data(), palRom.size(), "pal", error));
    REQUIRE(dendy.loadBytes(dendyRom.data(), dendyRom.size(), "dendy", error));
    REQUIRE(ntsc.info().detail == "NROM, NTSC");
    REQUIRE(pal.info().detail == "NROM, PAL");
    REQUIRE(dendy.info().detail == "NROM, Dendy");
    REQUIRE(pal.framePeriodNs() > ntsc.framePeriodNs());
    REQUIRE(dendy.framePeriodNs() != pal.framePeriodNs());
    REQUIRE(ntsc.framePeriodNs() == iris::systems::nes::kNtscFrameNs);

    ntsc.runFrame();
    pal.runFrame();
    REQUIRE(pal.audio().count > ntsc.audio().count + 50);
}
