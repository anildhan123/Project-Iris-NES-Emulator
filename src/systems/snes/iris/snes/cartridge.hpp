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
    SuperFx,
    Sa1,
};

struct Cartridge {
    std::vector<std::uint8_t> rom;
    std::string title;
    TvStandard standard = TvStandard::Ntsc;
    bool hiRom = false;
    bool exHiRom = false;
    Chip chip = Chip::None;
    std::size_t sramBytes = 0;
};

// LoROM, HiROM, ExHiROM, and the DSP-1, Super FX, and SA-1 boards, with or without a 512-byte copier header.
[[nodiscard]] Result<Cartridge> parseCartridge(const std::uint8_t* data, std::size_t size);
[[nodiscard]] std::string boardText(const Cartridge& cartridge);

}  // namespace iris::systems::snes
