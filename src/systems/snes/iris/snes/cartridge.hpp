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

struct Cartridge {
    std::vector<std::uint8_t> rom;
    std::string title;
    TvStandard standard = TvStandard::Ntsc;
    bool hiRom = false;
    std::size_t sramBytes = 0;
};

// LoROM and HiROM images, with or without a 512-byte copier header.
[[nodiscard]] Result<Cartridge> parseCartridge(const std::uint8_t* data, std::size_t size);

}  // namespace iris::systems::snes
