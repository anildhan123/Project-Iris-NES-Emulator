#include "iris/shell_theme.hpp"

#include <algorithm>
#include <cmath>

namespace iris {
namespace {

ShellColor rgb(int red, int green, int blue) {
    return ShellColor{
        static_cast<float>(red) / 255.0f,
        static_cast<float>(green) / 255.0f,
        static_cast<float>(blue) / 255.0f};
}

ImU32 packed(const ShellColor& color, int alpha = 255) {
    return IM_COL32(
        static_cast<int>(std::lround(std::clamp(color.red, 0.0f, 1.0f) * 255.0f)),
        static_cast<int>(std::lround(std::clamp(color.green, 0.0f, 1.0f) * 255.0f)),
        static_cast<int>(std::lround(std::clamp(color.blue, 0.0f, 1.0f) * 255.0f)),
        alpha);
}

ImVec4 vec4(const ShellColor& color) {
    return ImVec4(color.red, color.green, color.blue, 1.0f);
}

ShellColor mix(const ShellColor& color, const ShellColor& toward, float amount) {
    return ShellColor{
        color.red + (toward.red - color.red) * amount,
        color.green + (toward.green - color.green) * amount,
        color.blue + (toward.blue - color.blue) * amount};
}

void drawGlyph(ImDrawList* draw, ImVec2 min, float size, CaptionCommand glyph, ImU32 color) {
    const float inset = std::max(5.0f, size * 0.28f);
    const float left = min.x + inset;
    const float right = min.x + size - inset;
    const float top = min.y + inset;
    const float bottom = min.y + size - inset;
    const float thickness = std::max(1.0f, size * 0.08f);
    if (glyph == CaptionCommand::Minimize) {
        const float y = min.y + size * 0.62f;
        draw->AddLine(ImVec2(left, y), ImVec2(right, y), color, thickness);
        return;
    }
    if (glyph == CaptionCommand::Maximize) {
        draw->AddRect(ImVec2(left, top), ImVec2(right, bottom), color, 0.0f, 0, thickness);
        return;
    }
    if (glyph == CaptionCommand::Restore) {
        const float shift = std::max(2.0f, size * 0.12f);
        draw->AddRect(ImVec2(left + shift, top), ImVec2(right, bottom - shift), color, 0.0f, 0, thickness);
        draw->AddRect(ImVec2(left, top + shift), ImVec2(right - shift, bottom), color, 0.0f, 0, thickness);
        return;
    }
    draw->AddLine(ImVec2(left, top), ImVec2(right, bottom), color, thickness);
    draw->AddLine(ImVec2(right, top), ImVec2(left, bottom), color, thickness);
}

}  // namespace

ShellPalette shellPalette(Theme theme) {
    ShellPalette palette;
    palette.face = rgb(236, 233, 216);
    palette.toolbarTop = rgb(252, 252, 248);
    palette.toolbarBottom = rgb(214, 208, 188);
    palette.closeTop = rgb(236, 128, 96);
    palette.closeBottom = rgb(188, 42, 28);
    switch (theme) {
    case Theme::Olive:
        palette.frame = rgb(96, 112, 48);
        palette.titleTop = rgb(142, 158, 78);
        palette.titleBottom = rgb(78, 94, 36);
        palette.titleText = rgb(255, 255, 255);
        palette.captionTop = rgb(168, 182, 102);
        palette.captionBottom = rgb(104, 120, 56);
        palette.captionMark = rgb(255, 255, 255);
        palette.headerTop = rgb(176, 188, 112);
        palette.headerBottom = rgb(96, 112, 48);
        palette.headerText = rgb(255, 255, 255);
        palette.accent = rgb(214, 220, 186);
        break;
    case Theme::Silver:
        palette.face = rgb(232, 232, 236);
        palette.frame = rgb(108, 124, 148);
        palette.titleTop = rgb(236, 240, 246);
        palette.titleBottom = rgb(154, 170, 194);
        palette.titleText = rgb(0, 0, 0);
        palette.captionTop = rgb(246, 248, 252);
        palette.captionBottom = rgb(164, 176, 196);
        palette.captionMark = rgb(32, 40, 56);
        palette.headerTop = rgb(168, 186, 214);
        palette.headerBottom = rgb(90, 114, 154);
        palette.headerText = rgb(255, 255, 255);
        palette.toolbarTop = rgb(248, 248, 250);
        palette.toolbarBottom = rgb(206, 208, 216);
        palette.accent = rgb(210, 218, 230);
        break;
    case Theme::Blue:
        palette.frame = rgb(0, 84, 214);
        palette.titleTop = rgb(52, 140, 236);
        palette.titleBottom = rgb(0, 70, 186);
        palette.titleText = rgb(255, 255, 255);
        palette.captionTop = rgb(78, 158, 242);
        palette.captionBottom = rgb(12, 86, 204);
        palette.captionMark = rgb(255, 255, 255);
        palette.headerTop = rgb(126, 174, 230);
        palette.headerBottom = rgb(36, 96, 184);
        palette.headerText = rgb(255, 255, 255);
        palette.accent = rgb(196, 214, 238);
        break;
    }
    return palette;
}

void applyShellTheme(Theme theme, float contentScale) {
    const ShellPalette palette = shellPalette(theme);
    const ImVec4 face = vec4(palette.face);
    const ImVec4 accent = vec4(palette.accent);
    const ImVec4 text = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 disabled = ImVec4(0.45f, 0.45f, 0.42f, 1.0f);
    const ImVec4 border = ImVec4(0.55f, 0.53f, 0.47f, 1.0f);
    const ImVec4 shadow = ImVec4(0.62f, 0.60f, 0.54f, 1.0f);
    const ImVec4 raised = ImVec4(
        std::min(face.x + 0.04f, 1.0f), std::min(face.y + 0.04f, 1.0f), std::min(face.z + 0.03f, 1.0f), 1.0f);

    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(6.0f, 4.0f);
    style.FramePadding = ImVec2(6.0f, 3.0f);
    style.ItemSpacing = ImVec2(6.0f, 3.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 2.0f);
    style.ScrollbarSize = 16.0f;
    style.FontSizeBase = 13.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = disabled;
    colors[ImGuiCol_WindowBg] = face;
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = face;
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_TitleBg] = vec4(palette.titleBottom);
    colors[ImGuiCol_TitleBgActive] = vec4(palette.titleBottom);
    colors[ImGuiCol_TitleBgCollapsed] = vec4(palette.titleBottom);
    colors[ImGuiCol_MenuBarBg] = face;
    colors[ImGuiCol_ScrollbarBg] = face;
    colors[ImGuiCol_ScrollbarGrab] = shadow;
    colors[ImGuiCol_ScrollbarGrabHovered] = accent;
    colors[ImGuiCol_ScrollbarGrabActive] = vec4(palette.frame);
    colors[ImGuiCol_CheckMark] = vec4(palette.frame);
    colors[ImGuiCol_SliderGrab] = vec4(palette.frame);
    colors[ImGuiCol_SliderGrabActive] = vec4(palette.titleBottom);
    colors[ImGuiCol_Button] = raised;
    colors[ImGuiCol_ButtonHovered] = accent;
    colors[ImGuiCol_ButtonActive] = accent;
    colors[ImGuiCol_Header] = accent;
    colors[ImGuiCol_HeaderHovered] = accent;
    colors[ImGuiCol_HeaderActive] = accent;
    colors[ImGuiCol_Separator] = shadow;
    colors[ImGuiCol_ResizeGrip] = shadow;
    colors[ImGuiCol_TextSelectedBg] = accent;
    colors[ImGuiCol_Tab] = face;
    colors[ImGuiCol_TabHovered] = accent;
    colors[ImGuiCol_TabSelected] = raised;

    if (contentScale < 1.0f) {
        contentScale = 1.0f;
    }
    style.ScaleAllSizes(contentScale);
    style.FontScaleDpi = contentScale;
}

void drawShadedBand(const ImVec2& min, const ImVec2& max, const ShellColor& top, const ShellColor& bottom) {
    const ImU32 topColor = packed(top);
    const ImU32 bottomColor = packed(bottom);
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(min, max, topColor, topColor, bottomColor, bottomColor);
}

void drawSectionHeader(const char* label, const ShellPalette& palette) {
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFrameHeight();
    if (width < 1.0f || height < 1.0f) {
        return;
    }
    const ImVec2 max(min.x + width, min.y + height);
    drawShadedBand(min, max, palette.headerTop, palette.headerBottom);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 text = ImGui::CalcTextSize(label);
    draw->AddText(
        ImVec2(min.x + 8.0f, min.y + (height - text.y) * 0.5f), packed(palette.headerText), label);
    ImGui::Dummy(ImVec2(width, height));
}

void paintDialogTitle(const char* title, const ShellPalette& palette) {
    const ImVec2 pos = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float barHeight = ImGui::GetFrameHeight();
    const ImVec2 barMax(pos.x + width, pos.y + barHeight);
    drawShadedBand(pos, barMax, palette.titleTop, palette.titleBottom);
    const float gloss = barHeight * 0.42f;
    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(barMax.x, pos.y + gloss), IM_COL32(255, 255, 255, 36));
    const ImVec2 text = ImGui::CalcTextSize(title);
    const float pad = ImGui::GetStyle().FramePadding.x + 4.0f;
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(pos.x + pad, pos.y + (barHeight - text.y) * 0.5f), packed(palette.titleText), title);
    const ImU32 edge = packed(palette.frame);
    const ImVec2 windowMax(pos.x + width, pos.y + ImGui::GetWindowHeight());
    ImGui::GetWindowDrawList()->AddRect(pos, windowMax, edge, 0.0f, 0, 3.0f);
}

TitleBar drawTitleBar(
    const ImVec2& min,
    const ImVec2& max,
    const char* title,
    bool maximized,
    const ShellPalette& palette,
    float outer) {
    TitleBar result;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float extent = std::max(0.0f, outer);
    const ImVec2 fillMin(min.x - extent, min.y - extent);
    const ImVec2 fillMax(max.x + extent, max.y);
    drawShadedBand(fillMin, fillMax, palette.titleTop, palette.titleBottom);
    const float fillHeight = fillMax.y - fillMin.y;
    draw->AddRectFilled(fillMin, ImVec2(fillMax.x, fillMin.y + fillHeight * 0.45f), IM_COL32(255, 255, 255, 42));

    const float height = max.y - min.y;
    const float mark = std::clamp(height * 0.46f, 12.0f, 18.0f);
    const ImVec2 center(min.x + 8.0f + mark * 0.5f, (min.y + max.y) * 0.5f);
    draw->AddCircleFilled(center, mark * 0.5f, packed(palette.titleText));
    draw->AddCircleFilled(center, mark * 0.22f, packed(palette.titleBottom));

    const ImVec2 textSize = ImGui::CalcTextSize(title);
    draw->AddText(
        ImVec2(center.x + mark * 0.5f + 6.0f, min.y + (height - textSize.y) * 0.5f),
        packed(palette.titleText),
        title);

    const float button = std::clamp(height - 8.0f, 18.0f, 24.0f);
    const float gap = 2.0f;
    const float y = min.y + (height - button) * 0.5f;
    float x = max.x - 5.0f - button;
    const CaptionCommand glyphs[3] = {
        CaptionCommand::Close,
        maximized ? CaptionCommand::Restore : CaptionCommand::Maximize,
        CaptionCommand::Minimize,
    };
    ImDrawList* overlay = draw;
    for (int index = 0; index < 3; ++index) {
        const CaptionCommand glyph = glyphs[index];
        const bool close = glyph == CaptionCommand::Close;
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID(index);
        const bool pressed = ImGui::InvisibleButton("##caption", ImVec2(button, button));
        const bool held = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        ShellColor top = close ? palette.closeTop : palette.captionTop;
        ShellColor bottom = close ? palette.closeBottom : palette.captionBottom;
        if (held) {
            top = mix(top, rgb(0, 0, 0), 0.18f);
            bottom = mix(bottom, rgb(0, 0, 0), 0.18f);
        } else if (hovered) {
            top = mix(top, rgb(255, 255, 255), 0.22f);
            bottom = mix(bottom, rgb(255, 255, 255), 0.12f);
        }
        const ImVec2 buttonMin(x, y);
        const ImVec2 buttonMax(x + button, y + button);
        const float rounding = std::max(2.0f, button * 0.18f);
        overlay->AddRectFilled(buttonMin, buttonMax, packed(bottom), rounding);
        overlay->PushClipRect(buttonMin, ImVec2(buttonMax.x, buttonMin.y + button * 0.55f), true);
        overlay->AddRectFilled(buttonMin, buttonMax, packed(top), rounding);
        overlay->PopClipRect();
        overlay->AddRect(buttonMin, buttonMax, IM_COL32(255, 255, 255, 140), rounding, 0, 1.0f);
        const ImU32 markColor = packed(close ? rgb(255, 255, 255) : palette.captionMark);
        drawGlyph(overlay, buttonMin, button, glyph, markColor);
        result.buttons[index] = ShellRect{x, y, button, button};
        if (pressed) {
            result.command = glyph;
        }
        x -= button + gap;
    }
    return result;
}

}  // namespace iris
