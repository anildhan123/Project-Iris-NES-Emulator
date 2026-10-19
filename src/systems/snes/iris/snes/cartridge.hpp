#pragma once

#include "iris/error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace iris::systems::snes {

enum class TvStandard {
    Ntsc,
    Pal,
};

enum class Chip {
    None,
    Dsp1,
    Dsp2,
    Dsp3,
    Dsp4,
    SuperFx,
    Sa1,
    Cx4,
    Sdd1,
    Spc7110,
    Obc1,
    St010,
    St011,
    St018,
    Bsx,
};

struct Cartridge {
    std::vector<std::uint8_t> rom;
    std::string title;
    TvStandard standard = TvStandard::Ntsc;
    bool hiRom = false;
    bool exHiRom = false;
    bool fastRom = false;
    bool loRomSpecial = false;  // map mode 2 / 0xA (LoROM variants)
    Chip chip = Chip::None;
    std::size_t sramBytes = 0;
    int mapMode = 0;
};

// LoROM, HiROM, ExHiROM, FastROM, and enhancement boards, with or without a 512-byte copier header.
[[nodiscard]] Result<Cartridge> parseCartridge(const std::uint8_t* data, std::size_t size);
[[nodiscard]] std::string boardText(const Cartridge& cartridge);

}  // namespace iris::systems::snes
