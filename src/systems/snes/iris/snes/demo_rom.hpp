#pragma once

#include <cstdint>
#include <vector>

namespace iris::systems::snes {

// Original LoROM program. Stripes, one sprite, and an NMI handler that moves it.
[[nodiscard]] std::vector<std::uint8_t> makeIrisSnesDemo();

}  // namespace iris::systems::snes
