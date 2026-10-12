#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace iris {

// Host-side presentation. These choices never write the emulated framebuffer.

enum class ScaleMode {
    Fit,
    Integer,
};

enum class AspectMode {
    Pixel,
    Television,
};

enum class PictureFilter {
    Nearest,
    Smooth,
    Sharp,
    Crt,
};

struct ParameterRange {
    const char* id;
    const char* name;
    float minimum;
    float maximum;
    float defaultValue;
};

struct ShaderDescription {
    const char* id;
    const char* name;
    int version;
    const char* stage;
    const char* cost;
    const ParameterRange* parameters;
    int parameterCount;
};

inline constexpr ParameterRange kBrightness{"brightness", "Brightness", -0.5f, 0.5f, 0.0f};
inline constexpr ParameterRange kContrast{"contrast", "Contrast", 0.5f, 2.0f, 1.0f};
inline constexpr ParameterRange kSaturation{"saturation", "Saturation", 0.0f, 2.0f, 1.0f};
inline constexpr ParameterRange kGamma{"gamma", "Gamma", 0.4f, 2.5f, 1.0f};
inline constexpr ParameterRange kSharpness{"sharpness", "Sharpness", 1.0f, 4.0f, 2.0f};
inline constexpr ParameterRange kScanline{"scanline", "Scanlines", 0.0f, 1.0f, 0.5f};

inline constexpr ParameterRange kColorParameters[] = {kBrightness, kContrast, kSaturation, kGamma};

// Sampling runs first. Color runs after it. That order is fixed because later
// stages read the sampled color and do not feed back into the sampler.
inline constexpr ShaderDescription kPresentationShaders[] = {
    {"nearest", "Nearest", 1, "sampling", "low", nullptr, 0},
    {"smooth", "Smooth", 1, "sampling", "low", nullptr, 0},
    {"sharp", "Sharp", 1, "sampling", "low", &kSharpness, 1},
    {"crt", "CRT", 1, "sampling", "low", &kScanline, 1},
    {"color", "Color", 1, "color", "low", kColorParameters, 4},
};

inline constexpr int kMinimumIntegerScale = 1;
inline constexpr int kMaximumIntegerScale = 6;

struct PicturePlacement {
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    int appliedScale = 1;
    bool fellBack = false;
    bool fitted = false;
};

[[nodiscard]] inline const char* scaleModeId(ScaleMode mode) {
    return mode == ScaleMode::Integer ? "integer" : "fit";
}

[[nodiscard]] inline const char* aspectModeId(AspectMode mode) {
    return mode == AspectMode::Television ? "4:3" : "pixel";
}

[[nodiscard]] inline const char* pictureFilterId(PictureFilter filter) {
    switch (filter) {
    case PictureFilter::Smooth:
        return "smooth";
    case PictureFilter::Sharp:
        return "sharp";
    case PictureFilter::Crt:
        return "crt";
    case PictureFilter::Nearest:
        return "nearest";
    }
    return "nearest";
}

[[nodiscard]] inline const char* pictureFilterName(PictureFilter filter) {
    switch (filter) {
    case PictureFilter::Smooth:
        return "Smooth";
    case PictureFilter::Sharp:
        return "Sharp";
    case PictureFilter::Crt:
        return "CRT";
    case PictureFilter::Nearest:
        return "Nearest";
    }
    return "Nearest";
}

[[nodiscard]] inline std::optional<ScaleMode> scaleModeFromId(std::string_view id) {
    if (id == "fit") {
        return ScaleMode::Fit;
    }
    if (id == "integer") {
        return ScaleMode::Integer;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<AspectMode> aspectModeFromId(std::string_view id) {
    if (id == "pixel") {
        return AspectMode::Pixel;
    }
    if (id == "4:3") {
        return AspectMode::Television;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<PictureFilter> pictureFilterFromId(std::string_view id) {
    if (id == "nearest") {
        return PictureFilter::Nearest;
    }
    if (id == "smooth") {
        return PictureFilter::Smooth;
    }
    if (id == "sharp") {
        return PictureFilter::Sharp;
    }
    if (id == "crt") {
        return PictureFilter::Crt;
    }
    return std::nullopt;
}

[[nodiscard]] inline bool acceptsParameter(const ParameterRange& parameter, double value) {
    if (!std::isfinite(value)) {
        return false;
    }
    const double minimum = static_cast<double>(parameter.minimum) - 0.0001;
    const double maximum = static_cast<double>(parameter.maximum) + 0.0001;
    return value >= minimum && value <= maximum;
}

[[nodiscard]] inline float clampParameter(const ParameterRange& parameter, double value) {
    const double minimum = static_cast<double>(parameter.minimum);
    const double maximum = static_cast<double>(parameter.maximum);
    if (value < minimum) {
        value = minimum;
    }
    if (value > maximum) {
        value = maximum;
    }
    return static_cast<float>(value);
}

[[nodiscard]] inline bool matchesPreset(
    PictureFilter active,
    PictureFilter preset,
    float brightness,
    float contrast,
    float saturation,
    float gamma,
    float sharpness,
    float scanline) {
    if (active != preset || brightness != kBrightness.defaultValue || contrast != kContrast.defaultValue ||
        saturation != kSaturation.defaultValue || gamma != kGamma.defaultValue) {
        return false;
    }
    if (preset == PictureFilter::Sharp && sharpness != kSharpness.defaultValue) {
        return false;
    }
    if (preset == PictureFilter::Crt && scanline != kScanline.defaultValue) {
        return false;
    }
    return true;
}

[[nodiscard]] inline int pictureUnitHeight(int sourceWidth, int sourceHeight, AspectMode aspect) {
    if (aspect == AspectMode::Television) {
        return std::max(1, sourceWidth * 3 / 4);
    }
    return std::max(1, sourceHeight);
}

// Places the picture inside the available pixel rectangle. Integer scales that
// do not fit step down to the largest integer that does. If even 1x does not
// fit, the picture is fitted uniformly instead of stretched on one axis.
[[nodiscard]] inline PicturePlacement placePicture(
    int availableWidth,
    int availableHeight,
    int sourceWidth,
    int sourceHeight,
    ScaleMode scaleMode,
    int integerScale,
    AspectMode aspect) {
    PicturePlacement placement;
    if (availableWidth <= 0 || availableHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0) {
        placement.appliedScale = 0;
        placement.fellBack = true;
        placement.fitted = true;
        return placement;
    }

    const int unitWidth = std::max(1, sourceWidth);
    const int unitHeight = pictureUnitHeight(sourceWidth, sourceHeight, aspect);
    const int fittedScale = std::min(availableWidth / unitWidth, availableHeight / unitHeight);

    const auto placeInteger = [&](int scale, bool fallback) {
        placement.width = unitWidth * scale;
        placement.height = unitHeight * scale;
        placement.x = (availableWidth - placement.width) / 2;
        placement.y = (availableHeight - placement.height) / 2;
        placement.appliedScale = scale;
        placement.fellBack = fallback;
        placement.fitted = false;
    };
    const auto placeFitted = [&]() {
        const double scale = std::min(
            static_cast<double>(availableWidth) / static_cast<double>(unitWidth),
            static_cast<double>(availableHeight) / static_cast<double>(unitHeight));
        placement.width = std::max(1, static_cast<int>(std::floor(static_cast<double>(unitWidth) * scale)));
        placement.height = std::max(1, static_cast<int>(std::floor(static_cast<double>(unitHeight) * scale)));
        if (placement.width > availableWidth) {
            placement.width = availableWidth;
        }
        if (placement.height > availableHeight) {
            placement.height = availableHeight;
        }
        placement.x = (availableWidth - placement.width) / 2;
        placement.y = (availableHeight - placement.height) / 2;
        placement.appliedScale = 0;
        placement.fellBack = true;
        placement.fitted = true;
    };

    if (scaleMode == ScaleMode::Fit) {
        if (fittedScale >= 1) {
            placeInteger(fittedScale, false);
        } else {
            placeFitted();
        }
        return placement;
    }

    int requested = integerScale;
    if (requested < kMinimumIntegerScale) {
        requested = kMinimumIntegerScale;
    }
    if (requested > kMaximumIntegerScale) {
        requested = kMaximumIntegerScale;
    }
    if (requested <= fittedScale) {
        placeInteger(requested, false);
    } else if (fittedScale >= 1) {
        placeInteger(fittedScale, true);
    } else {
        placeFitted();
    }
    return placement;
}

[[nodiscard]] inline std::string placementLabel(const PicturePlacement& placement, AspectMode aspect) {
    std::string label = placement.fitted ? std::string("Fitted") : std::to_string(placement.appliedScale) + "x";
    if (aspect == AspectMode::Television) {
        label += " 4:3";
    }
    return label;
}

[[nodiscard]] inline std::string placementNote(ScaleMode mode, int requestedScale, const PicturePlacement& placement) {
    if (!placement.fellBack) {
        return {};
    }
    if (placement.fitted || mode == ScaleMode::Fit) {
        return "Fitted to the window";
    }
    return std::to_string(requestedScale) + "x does not fit; showing " + std::to_string(placement.appliedScale) + "x";
}

}  // namespace iris
