#pragma once

#include <cstdint>
#include <vector>

namespace iris::systems::nes {

// Original NROM program written for Iris. It draws a tile grid, the word IRIS, and a sprite
// that moves from controller port 1. Holding A plays a pulse tone.
[[nodiscard]] std::vector<std::uint8_t> makeIrisDemoRom();

}  // namespace iris::systems::nes
