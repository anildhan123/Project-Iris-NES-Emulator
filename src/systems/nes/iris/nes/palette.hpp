#pragma once

#include <algorithm>
#include <cstdint>

namespace iris::systems::nes {

// Documented 2C03 RGB DAC levels. Each channel is 255 * level / 7.
// This is a display palette for the NTSC core, not a decoded composite signal.
inline constexpr std::uint16_t kPaletteDac[64] = {
    0x333, 0x014, 0x006, 0x326, 0x403, 0x503, 0x510, 0x420, 0x320, 0x120, 0x031, 0x040, 0x022, 0x000, 0x000, 0x000,
    0x555, 0x036, 0x027, 0x407, 0x507, 0x704, 0x700, 0x630, 0x430, 0x140, 0x040, 0x053, 0x044, 0x000, 0x000, 0x000,
    0x777, 0x357, 0x447, 0x637, 0x707, 0x737, 0x740, 0x750, 0x660, 0x360, 0x070, 0x276, 0x077, 0x000, 0x000, 0x000,
    0x777, 0x567, 0x657, 0x757, 0x747, 0x755, 0x764, 0x772, 0x773, 0x572, 0x473, 0x276, 0x467, 0x000, 0x000, 0x000,
};

struct Rgb {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

[[nodiscard]] inline Rgb paletteColor(std::uint8_t index, bool pal = false) {
    const std::uint16_t packed = kPaletteDac[index & 0x3F];
    const auto channel = [](std::uint16_t value, int shift) {
        const int level = (value >> shift) & 0xF;
        return static_cast<std::uint8_t>((level * 255) / 7);
    };
    Rgb color{channel(packed, 8), channel(packed, 4), channel(packed, 0)};
    if (pal) {
        // Slight cooler bias for the 2C07 display path; not a decoded PAL composite.
        color.red = static_cast<std::uint8_t>(color.red * 15 / 16);
        color.blue = static_cast<std::uint8_t>(std::min(255, color.blue * 17 / 16));
    }
    return color;
}

}  // namespace iris::systems::nes
