#pragma once

#include "iris/presentation.hpp"

#include <cstdint>
#include <memory>

namespace iris {

struct PictureAdjustments {
    PictureFilter filter = PictureFilter::Nearest;
    float brightness = kBrightness.defaultValue;
    float contrast = kContrast.defaultValue;
    float saturation = kSaturation.defaultValue;
    float gamma = kGamma.defaultValue;
    float sharpness = kSharpness.defaultValue;
    float scanline = kScanline.defaultValue;
};

// Samples the emulated texture into a presentation texture. The source texture is not modified.
class PicturePass {
public:
    PicturePass();
    ~PicturePass();

    PicturePass(const PicturePass&) = delete;
    PicturePass& operator=(const PicturePass&) = delete;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool ready() const;
    [[nodiscard]] std::uint32_t texture() const;
    [[nodiscard]] bool draw(
        std::uint32_t source,
        int sourceWidth,
        int sourceHeight,
        int outputWidth,
        int outputHeight,
        const PictureAdjustments& adjustments);

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace iris
