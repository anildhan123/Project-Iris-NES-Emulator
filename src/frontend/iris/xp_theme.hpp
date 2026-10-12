#pragma once

#include "iris/config.hpp"

namespace iris {

struct XpPalette {
    float faceRed = 0.0f;
    float faceGreen = 0.0f;
    float faceBlue = 0.0f;
    float accentRed = 0.0f;
    float accentGreen = 0.0f;
    float accentBlue = 0.0f;
};

[[nodiscard]] XpPalette xpPalette(Theme theme);
void applyXpTheme(Theme theme, float contentScale);

}  // namespace iris
