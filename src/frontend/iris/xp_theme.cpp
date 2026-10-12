#include "iris/xp_theme.hpp"

#include <imgui.h>

namespace iris {
namespace {

ImVec4 color(float red, float green, float blue, float alpha = 1.0f) {
    return ImVec4(red, green, blue, alpha);
}

}  // namespace

XpPalette xpPalette(Theme theme) {
    XpPalette palette;
    palette.faceRed = 236.0f / 255.0f;
    palette.faceGreen = 233.0f / 255.0f;
    palette.faceBlue = 216.0f / 255.0f;
    switch (theme) {
    case Theme::XpOlive:
        palette.accentRed = 116.0f / 255.0f;
        palette.accentGreen = 132.0f / 255.0f;
        palette.accentBlue = 68.0f / 255.0f;
        break;
    case Theme::XpSilver:
        palette.faceRed = 226.0f / 255.0f;
        palette.faceGreen = 226.0f / 255.0f;
        palette.faceBlue = 230.0f / 255.0f;
        palette.accentRed = 112.0f / 255.0f;
        palette.accentGreen = 128.0f / 255.0f;
        palette.accentBlue = 150.0f / 255.0f;
        break;
    case Theme::XpBlue:
        palette.accentRed = 49.0f / 255.0f;
        palette.accentGreen = 106.0f / 255.0f;
        palette.accentBlue = 197.0f / 255.0f;
        break;
    }
    return palette;
}

void applyXpTheme(Theme theme, float contentScale) {
    const XpPalette palette = xpPalette(theme);
    const ImVec4 face = color(palette.faceRed, palette.faceGreen, palette.faceBlue);
    const ImVec4 accent = color(palette.accentRed, palette.accentGreen, palette.accentBlue);
    const ImVec4 selection = color(
        face.x * 0.82f + accent.x * 0.18f,
        face.y * 0.82f + accent.y * 0.18f,
        face.z * 0.82f + accent.z * 0.18f);
    const ImVec4 text = color(0.0f, 0.0f, 0.0f);
    const ImVec4 disabled = color(0.45f, 0.45f, 0.42f);
    const ImVec4 border = color(0.45f, 0.43f, 0.38f);
    const ImVec4 shadow = color(0.62f, 0.60f, 0.54f);

    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(4.0f, 4.0f);
    style.FramePadding = ImVec2(6.0f, 3.0f);
    style.ItemSpacing = ImVec2(6.0f, 3.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 2.0f);
    style.ScrollbarSize = 16.0f;
    style.FontSizeBase = 13.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = disabled;
    colors[ImGuiCol_WindowBg] = face;
    colors[ImGuiCol_ChildBg] = color(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = face;
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = color(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = color(1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = color(1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = color(1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_TitleBg] = accent;
    colors[ImGuiCol_TitleBgActive] = accent;
    colors[ImGuiCol_TitleBgCollapsed] = accent;
    colors[ImGuiCol_MenuBarBg] = face;
    colors[ImGuiCol_ScrollbarBg] = face;
    colors[ImGuiCol_ScrollbarGrab] = shadow;
    colors[ImGuiCol_ScrollbarGrabHovered] = selection;
    colors[ImGuiCol_ScrollbarGrabActive] = accent;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_Button] = face;
    colors[ImGuiCol_ButtonHovered] = selection;
    colors[ImGuiCol_ButtonActive] = selection;
    colors[ImGuiCol_Header] = selection;
    colors[ImGuiCol_HeaderHovered] = selection;
    colors[ImGuiCol_HeaderActive] = selection;
    colors[ImGuiCol_Separator] = shadow;
    colors[ImGuiCol_ResizeGrip] = shadow;
    colors[ImGuiCol_TextSelectedBg] = selection;

    // Menu highlights use Header with the global text color. Keep the highlight
    // light so black text stays readable.
    if (contentScale < 1.0f) {
        contentScale = 1.0f;
    }
    style.ScaleAllSizes(contentScale);
    style.FontScaleDpi = contentScale;
}

}  // namespace iris
