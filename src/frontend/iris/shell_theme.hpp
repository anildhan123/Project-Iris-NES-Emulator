#pragma once

#include "iris/config.hpp"

#include <imgui.h>

namespace iris {

struct ShellColor {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
};

struct ShellPalette {
    ShellColor face;
    ShellColor accent;
    ShellColor frame;
    ShellColor titleTop;
    ShellColor titleBottom;
    ShellColor titleText;
    ShellColor captionTop;
    ShellColor captionBottom;
    ShellColor captionMark;
    ShellColor closeTop;
    ShellColor closeBottom;
    ShellColor headerTop;
    ShellColor headerBottom;
    ShellColor headerText;
    ShellColor toolbarTop;
    ShellColor toolbarBottom;
};

struct ShellRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class CaptionCommand {
    None,
    Minimize,
    Maximize,
    Restore,
    Close,
};

struct TitleBar {
    CaptionCommand command = CaptionCommand::None;
    ShellRect buttons[3]{};
};

[[nodiscard]] ShellPalette shellPalette(Theme theme);
void applyShellTheme(Theme theme, float contentScale);
void drawShadedBand(const ImVec2& min, const ImVec2& max, const ShellColor& top, const ShellColor& bottom);
void drawSectionHeader(const char* label, const ShellPalette& palette);
void paintDialogTitle(const char* title, const ShellPalette& palette);
[[nodiscard]] TitleBar drawTitleBar(
    const ImVec2& min,
    const ImVec2& max,
    const char* title,
    bool maximized,
    const ShellPalette& palette,
    float outer);

}  // namespace iris
