#include "iris/desktop_shell.hpp"

#include "iris/log.hpp"
#include "iris/paths.hpp"
#include "iris/version.hpp"
#include "iris/xp_theme.hpp"

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <filesystem>
#include <string>

namespace iris {
namespace {

struct SdlSession {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool sdlInitialized = false;
    bool imguiContext = false;
    bool imguiBackends = false;

    ~SdlSession() {
        if (imguiBackends) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
        }
        if (imguiContext) {
            ImGui::DestroyContext();
        }
        if (context != nullptr) {
            SDL_GL_DestroyContext(context);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        if (sdlInitialized) {
            SDL_Quit();
        }
    }

    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;
    SdlSession() = default;
};

bool loadUiFont() {
    const auto windir = environmentValue("WINDIR");
    if (!windir.has_value()) {
        log::info("Using the built-in UI font because WINDIR is not set.");
        return false;
    }
    const std::filesystem::path fontPath = std::filesystem::path(*windir) / "Fonts" / "tahoma.ttf";
    std::error_code error;
    if (!std::filesystem::is_regular_file(fontPath, error)) {
        log::info("Using the built-in UI font because Tahoma is not installed.");
        return false;
    }

    // The operating-system font is read in place and is not copied into the repository.
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.string().c_str());
    if (font == nullptr) {
        log::warn("Tahoma could not be loaded. Using the built-in UI font.");
        return false;
    }
    log::info("Loaded Tahoma from the operating system font directory.");
    return true;
}

void hideSpawnedConsole() {
#if defined(_WIN32)
    // A console-subsystem process shows a console when it is started from Explorer.
    // Detach it for the windowed session. --version returns before this runs.
    FreeConsole();
#endif
}

void drawModal(const char* title, const char* body, bool& open) {
    if (open) {
        ImGui::OpenPopup(title);
        open = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float wrapWidth = 440.0f;
    const float available = viewport->WorkSize.x - 48.0f;
    if (available < wrapWidth) {
        wrapWidth = available;
    }
    if (wrapWidth < 280.0f) {
        wrapWidth = 280.0f;
    }
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowContentSize(ImVec2(wrapWidth, 0.0f));
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
    ImGui::TextUnformatted(body);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(75.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace

DesktopShell::DesktopShell(const Config& config)
    : config_(config), windowWidth_(config.windowWidth), windowHeight_(config.windowHeight) {}

int DesktopShell::run() {
    hideSpawnedConsole();
    SdlSession session;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log::error(std::string("SDL_Init failed: ") + SDL_GetError());
        return 1;
    }
    session.sdlInitialized = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    const SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
                                  SDL_WINDOW_HIGH_PIXEL_DENSITY;
    session.window = SDL_CreateWindow(kProductName, windowWidth_, windowHeight_, flags);
    if (session.window == nullptr) {
        log::error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(session.window, kMinWindowWidth, kMinWindowHeight);

    session.context = SDL_GL_CreateContext(session.window);
    if (session.context == nullptr) {
        log::error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
        return 1;
    }
    if (!SDL_GL_MakeCurrent(session.window, session.context)) {
        log::error(std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
        return 1;
    }
    if (!SDL_GL_SetSwapInterval(1)) {
        log::warn(std::string("VSync was not enabled: ") + SDL_GetError());
    }

    float contentScale = SDL_GetWindowDisplayScale(session.window);
    if (contentScale <= 0.0f) {
        contentScale = 1.0f;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    session.imguiContext = true;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    applyXpTheme(config_.theme, contentScale);

    if (!ImGui_ImplSDL3_InitForOpenGL(session.window, session.context)) {
        log::error("ImGui SDL3 backend failed to initialize.");
        return 1;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplSDL3_Shutdown();
        log::error("ImGui OpenGL 3 backend failed to initialize.");
        return 1;
    }
    session.imguiBackends = true;
    loadUiFont();

    const XpPalette palette = xpPalette(config_.theme);
    const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (glVersion != nullptr) {
        log::info(std::string("OpenGL ") + glVersion);
    }

    SDL_SetWindowPosition(session.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(session.window);

    bool requestExit = false;
    bool openAbout = false;
    bool openLegal = false;
    const std::string versionLabel = std::string(kProductName) + " " + kVersionString;

    while (!requestExit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                requestExit = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(session.window)) {
                requestExit = true;
            }
        }
        if (requestExit) {
            break;
        }
        if ((SDL_GetWindowFlags(session.window) & SDL_WINDOW_MINIMIZED) != 0) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                 ImGuiWindowFlags_NoSavedSettings |
                                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar;
        if (ImGui::Begin("##client", nullptr, windowFlags)) {
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Exit")) {
                        requestExit = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Help")) {
                    if (ImGui::MenuItem("About")) {
                        openAbout = true;
                    }
                    if (ImGui::MenuItem("Legal Notices")) {
                        openLegal = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            const ImVec2 accentStart = ImGui::GetCursorScreenPos();
            const float accentWidth = ImGui::GetContentRegionAvail().x;
            const ImU32 accent = ImGui::ColorConvertFloat4ToU32(
                ImVec4(palette.accentRed, palette.accentGreen, palette.accentBlue, 1.0f));
            ImGui::GetWindowDrawList()->AddRectFilled(
                accentStart, ImVec2(accentStart.x + accentWidth, accentStart.y + 3.0f), accent);
            ImGui::Dummy(ImVec2(0.0f, 3.0f));

            const float statusHeight = ImGui::GetFrameHeight() + 6.0f;
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const float statusTop = windowPos.y + windowSize.y - statusHeight;
            float viewportHeight = statusTop - ImGui::GetCursorScreenPos().y;
            if (viewportHeight < 1.0f) {
                viewportHeight = 1.0f;
            }
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::BeginChild(
                "##viewport",
                ImVec2(0.0f, viewportHeight),
                ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 face = ImGui::ColorConvertFloat4ToU32(
                ImVec4(palette.faceRed, palette.faceGreen, palette.faceBlue, 1.0f));
            const ImU32 text = IM_COL32(0, 0, 0, 255);
            draw->PushClipRect(
                ImVec2(windowPos.x, statusTop),
                ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                false);
            draw->AddRectFilled(ImVec2(windowPos.x, statusTop), ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), face);
            draw->AddLine(ImVec2(windowPos.x, statusTop), ImVec2(windowPos.x + windowSize.x, statusTop), accent, 1.0f);
            draw->AddText(ImVec2(windowPos.x + 8.0f, statusTop + 4.0f), text, "No game loaded");
            const ImVec2 versionSize = ImGui::CalcTextSize(versionLabel.c_str());
            draw->AddText(
                ImVec2(windowPos.x + windowSize.x - versionSize.x - 8.0f, statusTop + 4.0f),
                text,
                versionLabel.c_str());
            draw->PopClipRect();

            drawModal(
                "About Iris Emulator",
                "Iris Emulator 0.1.0\n\nThis build is the project foundation. It does not include an emulation core.",
                openAbout);
            drawModal(
                "Legal Notices",
                "Iris Emulator does not include commercial games and does not download ROMs.\n\n"
                "Open only game files you have obtained and are legally entitled to use. "
                "Owning an original cartridge does not by itself make every method of dumping or downloading a game lawful. "
                "Those rules depend on jurisdiction and circumstance.",
                openLegal);
        }
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(palette.faceRed, palette.faceGreen, palette.faceBlue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(session.window);
    }

    SDL_GetWindowSize(session.window, &windowWidth_, &windowHeight_);
    return 0;
}

}  // namespace iris
