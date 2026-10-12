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
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

namespace iris {
namespace {

struct SdlSession {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_AudioStream* audio = nullptr;
    bool sdlInitialized = false;
    bool imguiContext = false;
    bool imguiBackends = false;

    ~SdlSession() {
        if (audio != nullptr) {
            SDL_DestroyAudioStream(audio);
        }
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

struct TextureGuard {
    GLuint id = 0;
    ~TextureGuard() {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }
};

struct PendingDialog {
    std::mutex mutex;
    bool active = false;
    bool ready = false;
    bool failed = false;
    std::string path;
    std::string error;
};

void SDLCALL onOpenFile(void* userdata, const char* const* filelist, int /*filter*/) {
    auto* pending = static_cast<PendingDialog*>(userdata);
    std::lock_guard lock(pending->mutex);
    pending->active = false;
    pending->ready = true;
    pending->failed = false;
    pending->path.clear();
    pending->error.clear();
    if (filelist == nullptr) {
        pending->failed = true;
        const char* message = SDL_GetError();
        pending->error = (message == nullptr || message[0] == '\0') ? "The file dialog failed." : message;
        return;
    }
    if (filelist[0] != nullptr) {
        pending->path = filelist[0];
    }
}

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

std::filesystem::path pathFromUtf8(const std::string& text) {
    return std::filesystem::path(reinterpret_cast<const char8_t*>(text.c_str()));
}

std::uint8_t controllerButtons() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr) {
        return 0;
    }
    std::uint8_t buttons = 0;
    if (keys[SDL_SCANCODE_Z]) buttons = static_cast<std::uint8_t>(buttons | kButtonA);
    if (keys[SDL_SCANCODE_X]) buttons = static_cast<std::uint8_t>(buttons | kButtonB);
    if (keys[SDL_SCANCODE_RSHIFT]) buttons = static_cast<std::uint8_t>(buttons | kButtonSelect);
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) buttons = static_cast<std::uint8_t>(buttons | kButtonStart);
    if (keys[SDL_SCANCODE_UP]) buttons = static_cast<std::uint8_t>(buttons | kButtonUp);
    if (keys[SDL_SCANCODE_DOWN]) buttons = static_cast<std::uint8_t>(buttons | kButtonDown);
    if (keys[SDL_SCANCODE_LEFT]) buttons = static_cast<std::uint8_t>(buttons | kButtonLeft);
    if (keys[SDL_SCANCODE_RIGHT]) buttons = static_cast<std::uint8_t>(buttons | kButtonRight);
    return buttons;
}

void uploadFrame(GLuint& texture, const FrameView& frame) {
    if (texture == 0) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.rgba);
}

}  // namespace

DesktopShell::DesktopShell(const Config& config, IEmulatedSystem& system, std::string initialRom)
    : config_(config), system_(system), initialRom_(std::move(initialRom)), windowWidth_(config.windowWidth),
      windowHeight_(config.windowHeight) {}

int DesktopShell::run() {
    hideSpawnedConsole();
    SdlSession session;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log::error(std::string("SDL_Init failed: ") + SDL_GetError());
        return 1;
    }
    session.sdlInitialized = true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        log::warn(std::string("Audio is unavailable: ") + SDL_GetError());
    }

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

    SDL_AudioSpec audioSpec{};
    audioSpec.format = SDL_AUDIO_F32;
    audioSpec.channels = 1;
    audioSpec.freq = 48000;
    session.audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
    if (session.audio == nullptr) {
        log::warn(std::string("Audio output was not opened: ") + SDL_GetError());
    } else if (!SDL_ResumeAudioStreamDevice(session.audio)) {
        log::warn(std::string("Audio output was not started: ") + SDL_GetError());
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

    std::string basePath;
    if (const char* base = SDL_GetBasePath()) {
        basePath = base;
    }
    static const SDL_DialogFileFilter kFilters[] = {
        {"NES ROM", "nes"},
        {"All files", "*"},
    };

    bool requestExit = false;
    bool openAbout = false;
    bool openLegal = false;
    bool openError = false;
    bool loadInitialRom = !initialRom_.empty();
    std::string errorText;
    PendingDialog dialog;
    TextureGuard texture;
    const std::string versionLabel = std::string(kProductName) + " " + kVersionString;
    const std::string aboutText = std::string(kProductName) + " " + kVersionString +
                                   "\n\nThis build plays NROM, MMC1, UxROM, CNROM, and MMC3. "
                                   "A NES 2.0 header can select PAL timing. "
                                   "Iris Demo, written for this project, is generated beside the executable.\n\n"
                                   "Open a ROM with File, Open Game. Arrow keys move, Z is A, X is B, "
                                   "Right Shift is Select, and Enter is Start. Escape pauses.";
    auto previous = std::chrono::steady_clock::now();
    std::int64_t accumulated = system_.framePeriodNs();

    const auto openRom = [&](const std::filesystem::path& path) {
        std::string error;
        if (!system_.loadContent(path, error)) {
            errorText = error;
            openError = true;
            log::error(error);
            return;
        }
        const SystemInfo info = system_.info();
        log::info(std::string("Loaded ") + info.title + " (" + info.detail + ").");
        if (info.batteryBacked) {
            log::warn("Battery-backed RAM will not be saved in this version.");
        }
        accumulated = system_.framePeriodNs();
    };

    const auto showOpenDialog = [&]() {
        std::lock_guard lock(dialog.mutex);
        if (dialog.active) {
            return;
        }
        dialog.active = true;
        dialog.ready = false;
        SDL_ShowOpenFileDialog(
            onOpenFile, &dialog, session.window, kFilters, 2, basePath.empty() ? nullptr : basePath.c_str(), false);
    };

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
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                if (ctrl && event.key.scancode == SDL_SCANCODE_O) {
                    showOpenDialog();
                } else if (event.key.scancode == SDL_SCANCODE_ESCAPE && system_.loaded() &&
                           !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
                    system_.setPaused(!system_.paused());
                }
            }
        }
        if (requestExit) {
            break;
        }
        if ((SDL_GetWindowFlags(session.window) & SDL_WINDOW_MINIMIZED) != 0) {
            SDL_Delay(10);
            previous = std::chrono::steady_clock::now();
            continue;
        }

        if (loadInitialRom) {
            loadInitialRom = false;
            openRom(std::filesystem::path(initialRom_));
        }

        {
            std::string chosen;
            bool failed = false;
            std::string failure;
            {
                std::lock_guard lock(dialog.mutex);
                if (dialog.ready) {
                    chosen = dialog.path;
                    failed = dialog.failed;
                    failure = dialog.error;
                    dialog.ready = false;
                    dialog.path.clear();
                }
            }
            if (failed) {
                errorText = failure;
                openError = true;
                log::error(failure);
            } else if (!chosen.empty()) {
                openRom(pathFromUtf8(chosen));
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous).count();
        previous = now;
        const std::int64_t period = system_.framePeriodNs();
        if (system_.loaded() && !system_.paused()) {
            accumulated += elapsed;
            if (accumulated >= period) {
                system_.runFrame();
                const AudioView audio = system_.audio();
                if (session.audio != nullptr && audio.samples != nullptr && audio.count > 0 &&
                    SDL_GetAudioStreamQueued(session.audio) < 48000 * static_cast<int>(sizeof(float))) {
                    SDL_PutAudioStreamData(session.audio, audio.samples, audio.count * static_cast<int>(sizeof(float)));
                }
                accumulated -= period;
                if (accumulated > period) {
                    accumulated = 0;
                }
            }
        } else {
            accumulated = period;
        }

        const bool uiOwnsInput = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
        system_.setController(0, system_.loaded() && !uiOwnsInput ? controllerButtons() : 0);
        system_.setController(1, 0);

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
                    if (ImGui::MenuItem("Open Game...", "Ctrl+O")) {
                        showOpenDialog();
                    }
                    if (ImGui::MenuItem("Close Game", nullptr, false, system_.loaded())) {
                        system_.unload();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Exit")) {
                        requestExit = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Emulation")) {
                    const char* pauseLabel = system_.paused() ? "Resume" : "Pause";
                    if (ImGui::MenuItem(pauseLabel, "Esc", false, system_.loaded())) {
                        system_.setPaused(!system_.paused());
                    }
                    if (ImGui::MenuItem("Reset", nullptr, false, system_.loaded())) {
                        system_.reset();
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
            const FrameView frame = system_.frame();
            if (system_.loaded() && frame.rgba != nullptr && frame.width > 0 && frame.height > 0) {
                uploadFrame(texture.id, frame);
                const float availW = ImGui::GetContentRegionAvail().x;
                const float availH = ImGui::GetContentRegionAvail().y;
                float scale = std::min(availW / static_cast<float>(frame.width), availH / static_cast<float>(frame.height));
                if (scale >= 1.0f) {
                    scale = std::floor(scale);
                }
                const ImVec2 size(static_cast<float>(frame.width) * scale, static_cast<float>(frame.height) * scale);
                const ImVec2 cursor = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cursor.x + (availW - size.x) * 0.5f, cursor.y + (availH - size.y) * 0.5f));
                ImGui::Image(static_cast<ImTextureID>(texture.id), size);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            std::string status = "No game loaded";
            if (system_.loaded()) {
                const SystemInfo info = system_.info();
                status = info.title + " - " + info.detail;
                if (system_.paused()) {
                    status += " - Paused";
                }
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 face = ImGui::ColorConvertFloat4ToU32(
                ImVec4(palette.faceRed, palette.faceGreen, palette.faceBlue, 1.0f));
            const ImU32 text = IM_COL32(0, 0, 0, 255);
            draw->PushClipRect(
                ImVec2(windowPos.x, statusTop),
                ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                false);
            draw->AddRectFilled(
                ImVec2(windowPos.x, statusTop), ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), face);
            draw->AddLine(ImVec2(windowPos.x, statusTop), ImVec2(windowPos.x + windowSize.x, statusTop), accent, 1.0f);
            draw->AddText(ImVec2(windowPos.x + 8.0f, statusTop + 4.0f), text, status.c_str());
            const ImVec2 versionSize = ImGui::CalcTextSize(versionLabel.c_str());
            draw->AddText(
                ImVec2(windowPos.x + windowSize.x - versionSize.x - 8.0f, statusTop + 4.0f),
                text,
                versionLabel.c_str());
            draw->PopClipRect();

            drawModal("About Iris Emulator", aboutText.c_str(), openAbout);
            drawModal(
                "Legal Notices",
                "Iris Emulator does not include commercial games and does not download ROMs. "
                "Iris Demo is an original program written for this project.\n\n"
                "Open only game files you have obtained and are legally entitled to use. "
                "Owning an original cartridge does not by itself make every method of dumping or downloading a game lawful. "
                "Those rules depend on jurisdiction and circumstance.",
                openLegal);
            drawModal("Could not open game", errorText.c_str(), openError);
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
