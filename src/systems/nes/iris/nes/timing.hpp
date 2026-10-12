#pragma once

#include <cstdint>

namespace iris::systems::nes {

enum class Region {
    Ntsc,
    Pal,
    Dendy,
};

struct RegionTiming {
    Region region = Region::Ntsc;
    int cpuClock = 1789773;
    int ppuNumerator = 3;
    int ppuDenominator = 1;
    int scanlines = 262;
    bool oddFrameSkip = true;
};

// NTSC keeps the half-dot-skipped frame length used by the shell since Milestone 2.
inline constexpr std::int64_t kNtscFrameNs = 16639266;

[[nodiscard]] inline const RegionTiming& regionTiming(Region region) {
    static constexpr RegionTiming kNtsc{Region::Ntsc, 1789773, 3, 1, 262, true};
    static constexpr RegionTiming kPal{Region::Pal, 1662607, 16, 5, 312, false};
    static constexpr RegionTiming kDendy{Region::Dendy, 1773448, 3, 1, 312, false};
    switch (region) {
    case Region::Pal:
        return kPal;
    case Region::Dendy:
        return kDendy;
    case Region::Ntsc:
        return kNtsc;
    }
    return kNtsc;
}

[[nodiscard]] inline const char* regionLabel(Region region) {
    switch (region) {
    case Region::Pal:
        return "PAL";
    case Region::Dendy:
        return "Dendy";
    case Region::Ntsc:
        return "NTSC";
    }
    return "NTSC";
}

// Whole frame, in nanoseconds. NTSC uses the established constant; PAL and Dendy follow their clocks.
[[nodiscard]] inline std::int64_t framePeriodNs(Region region) {
    if (region == Region::Ntsc) {
        return kNtscFrameNs;
    }
    const RegionTiming& timing = regionTiming(region);
    const std::int64_t dots = static_cast<std::int64_t>(timing.scanlines) * 341;
    const std::int64_t numerator = dots * timing.ppuDenominator * 1000000000LL;
    const std::int64_t denominator = static_cast<std::int64_t>(timing.ppuNumerator) * timing.cpuClock;
    return numerator / denominator;
}

}  // namespace iris::systems::nes
