#include "iris/desktop_shell.hpp"

#include "iris/library.hpp"
#include "iris/log.hpp"
#include "iris/paths.hpp"
#include "iris/picture_pass.hpp"
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
#include <shellapi.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

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

enum class DialogKind {
    OpenRom,
    ScanFolder,
};

struct PendingDialog {
    std::mutex mutex;
    bool active = false;
    bool ready = false;
    bool failed = false;
    DialogKind kind = DialogKind::OpenRom;
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

std::string pathToUtf8(const std::filesystem::path& path) {
    const auto text = path.lexically_normal().generic_u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::string scancodeLabel(int code) {
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
    if (name == nullptr || name[0] == '\0') {
        return "Unknown";
    }
    return name;
}

int audioQueueSamples(AudioLatency latency) {
    switch (latency) {
    case AudioLatency::Short:
        return 12000;
    case AudioLatency::Long:
        return 96000;
    case AudioLatency::Balanced:
        return 48000;
    }
    return 48000;
}

const char* playbackDeviceItem(void* data, int index) {
    const auto* items = static_cast<const std::vector<std::string>*>(data);
    if (items == nullptr || index < 0 || index >= static_cast<int>(items->size())) {
        return nullptr;
    }
    return (*items)[static_cast<std::size_t>(index)].c_str();
}

void drawBindingButton(const char* label, int& scancode, const char* chord, int*& capture) {
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(150.0f);
    std::string shown = "Press a key";
    if (capture != &scancode) {
        shown = chord == nullptr ? scancodeLabel(scancode) : std::string(chord) + scancodeLabel(scancode);
    }
    if (ImGui::Button(shown.c_str(), ImVec2(180.0f, 0.0f))) {
        capture = &scancode;
    }
    ImGui::PopID();
}

std::uint8_t controllerButtons(const InputBindings& bindings) {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    if (keys == nullptr || count <= 0) {
        return 0;
    }
    const auto down = [&](int code) { return code >= 0 && code < count && keys[code]; };
    std::uint8_t buttons = 0;
    if (down(bindings.a)) buttons = static_cast<std::uint8_t>(buttons | kButtonA);
    if (down(bindings.b)) buttons = static_cast<std::uint8_t>(buttons | kButtonB);
    if (down(bindings.select)) buttons = static_cast<std::uint8_t>(buttons | kButtonSelect);
    if (down(bindings.start) || down(SDL_SCANCODE_KP_ENTER)) buttons = static_cast<std::uint8_t>(buttons | kButtonStart);
    if (down(bindings.up)) buttons = static_cast<std::uint8_t>(buttons | kButtonUp);
    if (down(bindings.down)) buttons = static_cast<std::uint8_t>(buttons | kButtonDown);
    if (down(bindings.left)) buttons = static_cast<std::uint8_t>(buttons | kButtonLeft);
    if (down(bindings.right)) buttons = static_cast<std::uint8_t>(buttons | kButtonRight);
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
    if (!SDL_GL_SetSwapInterval(config_.vsync ? 1 : 0)) {
        log::warn(std::string("VSync was not changed: ") + SDL_GetError());
    }

    const auto openPlayback = [&](const std::string& deviceName) {
        if (session.audio != nullptr) {
            SDL_DestroyAudioStream(session.audio);
            session.audio = nullptr;
        }
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_F32;
        spec.channels = 1;
        spec.freq = 48000;
        SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        bool named = false;
        if (!deviceName.empty()) {
            int count = 0;
            SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
            if (devices != nullptr) {
                for (int index = 0; index < count; ++index) {
                    const char* name = SDL_GetAudioDeviceName(devices[index]);
                    if (name != nullptr && deviceName == name) {
                        device = devices[index];
                        named = true;
                        break;
                    }
                }
                SDL_free(devices);
            }
            if (!named) {
                log::info("Audio device \"" + deviceName + "\" is not connected. Using the default playback device.");
            }
        }
        session.audio = SDL_OpenAudioDeviceStream(device, &spec, nullptr, nullptr);
        if (session.audio == nullptr && device != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
            log::warn(std::string("The selected audio device did not open: ") + SDL_GetError());
            session.audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        }
        if (session.audio == nullptr) {
            log::warn(std::string("Audio output was not opened: ") + SDL_GetError());
            return;
        }
        if (!SDL_ResumeAudioStreamDevice(session.audio)) {
            log::warn(std::string("Audio output was not started: ") + SDL_GetError());
            return;
        }
        const char* opened = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(session.audio));
        log::info(std::string("Audio output: ") + (opened == nullptr ? "default" : opened));
    };
    openPlayback(config_.audioDevice);

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

    XpPalette palette = xpPalette(config_.theme);
    const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (glVersion != nullptr) {
        log::info(std::string("OpenGL ") + glVersion);
    }
    PicturePass picturePass;
    if (!picturePass.initialize()) {
        log::warn("The picture will be shown as crisp pixels.");
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
    bool openPicture = false;
    bool loadInitialRom = !initialRom_.empty();
    std::string errorText;
    std::string errorTitle = "Could not open game";
    PendingDialog dialog;
    TextureGuard texture;
    const std::string versionLabel = std::string(kProductName) + " " + kVersionString;
    const std::string aboutText = std::string(kProductName) + " " + kVersionString +
                                   "\n\nThis build plays NROM, MMC1, UxROM, CNROM, and MMC3. "
                                   "A NES 2.0 header can select PAL timing. "
                                   "Iris Demo, written for this project, is generated beside the executable.\n\n"
                                   "Open a ROM with File, Open Game, or from the library. "
                                   "The default controls are the arrow keys, Z for A, X for B, Right Shift for Select, and Enter for Start. "
                                   "Numpad Enter also presses Start. Escape pauses. "
                                   "F5 saves the selected state slot and F7 loads it. Ctrl+R resets. "
                                   "Options, Settings changes the theme, picture, audio, and keys. "
                                   "Shift+1 through Shift+0 select a slot, and Emulation, State Slots shows previews. "
                                   "Battery-backed RAM is stored separately from those slots. "
                                   "View sets the scale, 4:3 aspect, and presentation filter. Alt+Enter toggles fullscreen. "
                                   "Filters and color settings do not change the emulated picture.";
    auto previous = std::chrono::steady_clock::now();
    std::int64_t accumulated = system_.framePeriodNs();
    double hostFps = 0.0;
    double emulatedFps = 0.0;
    double lastEmulationMs = 0.0;
    double lastPresentMs = 0.0;
    int hostFrames = 0;
    int emulatedFrames = 0;
    auto fpsWindow = previous;
    const auto setFullscreen = [&](bool enabled) {
        const bool active = (SDL_GetWindowFlags(session.window) & SDL_WINDOW_FULLSCREEN) != 0;
        if (enabled == active) {
            config_.fullscreen = enabled;
            return;
        }
        if (enabled) {
            SDL_GetWindowSize(session.window, &windowWidth_, &windowHeight_);
        }
        if (!SDL_SetWindowFullscreen(session.window, enabled)) {
            errorText = SDL_GetError();
            if (errorText.empty()) {
                errorText = "Fullscreen is not available.";
            }
            errorTitle = "Could not change fullscreen";
            openError = true;
            log::error(errorText);
            return;
        }
        config_.fullscreen = enabled;
    };
    if (config_.fullscreen) {
        setFullscreen(true);
    }
    const auto appPaths = resolveAppPaths();
    GameLibrary library;
    std::string libraryNote;
    std::string selectedGame;
    char libraryQuery[128] = {};
    LibrarySort librarySort = LibrarySort::Title;
    bool openSettings = false;
    bool openLicenses = false;
    bool openRemove = false;
    bool confirmReset = false;
    bool refreshAudioList = false;
    Config settingsDraft = config_;
    int* captureBinding = nullptr;
    std::string removePath;
    std::vector<std::string> playbackNames;
    std::vector<float> audioScratch;
    bool wasMuted = config_.muted;
    if (appPaths.has_value()) {
        const auto loadedLibrary = loadLibraryFile(appPaths->library / "library.json");
        if (loadedLibrary.ok()) {
            library = loadedLibrary.value();
        } else if (loadedLibrary.error().code != ErrorCode::NotFound) {
            log::error(loadedLibrary.error().message);
            libraryNote = "The library file could not be read and was left unchanged.";
        }
    }
    constexpr int kStateSlots = 10;
    int selectedSlot = 0;
    bool openStateSlots = false;
    bool refreshStateSlots = false;
    std::array<std::string, kStateSlots> slotDescriptions{};
    std::array<std::array<char, 201>, kStateSlots> slotDescriptionEdit{};
    std::array<StatePreview, kStateSlots> slotPreviews{};
    std::array<TextureGuard, kStateSlots> slotTextures{};
    for (auto& text : slotDescriptionEdit) {
        text.fill('\0');
    }

    const auto slotFile = [&](int slot) -> std::filesystem::path {
        if (!appPaths.has_value() || system_.contentId().size() != 64) {
            return {};
        }
        char name[24] = {};
        std::snprintf(name, sizeof(name), "slot-%02d.irstate", slot + 1);
        return appPaths->states / system_.contentId() / name;
    };

    const auto readFile = [](const std::filesystem::path& path, std::size_t limit, std::vector<std::uint8_t>& bytes, std::string& error) {
        std::error_code failure;
        if (!std::filesystem::is_regular_file(path, failure)) {
            error = "The file could not be read.";
            return false;
        }
        const auto size = std::filesystem::file_size(path, failure);
        if (failure || size > limit) {
            error = "The file could not be read.";
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        bytes.resize(static_cast<std::size_t>(size));
        if (!bytes.empty()) {
            input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        if (!input) {
            error = "The file could not be read.";
            return false;
        }
        return true;
    };

    const auto flushBattery = [&]() {
        if (!appPaths.has_value()) {
            return;
        }
        std::vector<std::uint8_t> bytes;
        if (!system_.copyBattery(bytes)) {
            return;
        }
        std::error_code failure;
        std::filesystem::create_directories(appPaths->saves, failure);
        const auto file = appPaths->saves / (system_.contentId() + ".sav");
        if (failure || !writeBinaryFileAtomically(file, bytes.data(), bytes.size())) {
            log::error("Could not write the battery save.");
        }
    };

    const auto importBattery = [&]() {
        if (!appPaths.has_value() || !system_.info().batteryBacked) {
            return;
        }
        const auto file = appPaths->saves / (system_.contentId() + ".sav");
        std::error_code failure;
        if (!std::filesystem::is_regular_file(file, failure)) {
            return;
        }
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!readFile(file, 2 * 1024 * 1024, bytes, error) || !system_.restoreBattery(bytes.data(), bytes.size(), error)) {
            errorText = error.empty() ? "The battery save does not match this cartridge and was not loaded." : error;
            errorTitle = "Could not load the battery save";
            openError = true;
            log::error(errorText);
        }
    };

    const auto saveSelectedSlot = [&]() {
        if (!system_.loaded()) {
            return;
        }
        if (!appPaths.has_value()) {
            errorText = "Could not find the save folder.";
            errorTitle = "Could not save the state";
            openError = true;
            return;
        }
        std::vector<std::uint8_t> bytes;
        std::string error;
        const std::string description = slotDescriptions[static_cast<std::size_t>(selectedSlot)];
        if (!system_.saveState(bytes, description, error)) {
            errorText = error;
            errorTitle = "Could not save the state";
            openError = true;
            log::error(error);
            return;
        }
        const auto file = slotFile(selectedSlot);
        std::error_code failure;
        std::filesystem::create_directories(file.parent_path(), failure);
        if (failure || file.empty() || !writeBinaryFileAtomically(file, bytes.data(), bytes.size())) {
            errorText = "Could not write the state file.";
            errorTitle = "Could not save the state";
            openError = true;
            log::error(errorText);
            return;
        }
        log::info("Saved state slot " + std::to_string(selectedSlot + 1) + ".");
        refreshStateSlots = true;
    };

    const auto loadSelectedSlot = [&]() {
        if (!system_.loaded()) {
            return;
        }
        const auto file = slotFile(selectedSlot);
        std::error_code failure;
        if (file.empty() || !std::filesystem::is_regular_file(file, failure)) {
            errorText = "Slot " + std::to_string(selectedSlot + 1) + " is empty.";
            errorTitle = "Could not load the state";
            openError = true;
            return;
        }
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!readFile(file, 4 * 1024 * 1024, bytes, error) || !system_.loadState(bytes.data(), bytes.size(), error)) {
            errorText = error.empty() ? "The state file is damaged or incomplete." : error;
            errorTitle = "Could not load the state";
            openError = true;
            log::error(errorText);
            return;
        }
        log::info("Loaded state slot " + std::to_string(selectedSlot + 1) + ".");
    };

    const auto saveLibrary = [&]() {
        if (!appPaths.has_value()) {
            errorText = "Could not find the library folder.";
            errorTitle = "Could not save the library";
            openError = true;
            return false;
        }
        std::error_code failure;
        std::filesystem::create_directories(appPaths->library, failure);
        if (failure || !saveLibraryFile(appPaths->library / "library.json", library)) {
            errorText = "Could not write the library.";
            errorTitle = "Could not save the library";
            openError = true;
            log::error(errorText);
            return false;
        }
        return true;
    };

    const auto openRom = [&](const std::filesystem::path& path) {
        flushBattery();
        std::string error;
        if (!system_.loadContent(path, error)) {
            errorText = error;
            errorTitle = "Could not open game";
            openError = true;
            log::error(error);
            return;
        }
        const SystemInfo info = system_.info();
        log::info(std::string("Loaded ") + info.title + " (" + info.detail + ").");
        if (info.batteryBacked) {
            log::warn("Battery-backed RAM is stored separately from save states.");
        }
        importBattery();
        accumulated = system_.framePeriodNs();
        LibraryEntry entry;
        entry.path = pathToUtf8(path);
        entry.title = info.title;
        entry.detail = info.detail;
        entry.contentId = system_.contentId();
        entry.addedUnixMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
        entry.lastPlayedUnixMs = entry.addedUnixMs;
        std::string upsertError;
        if (!LibraryService::upsert(library, std::move(entry), upsertError)) {
            errorText = upsertError;
            errorTitle = "Could not update the library";
            openError = true;
            return;
        }
        saveLibrary();
    };

    const auto showOpenDialog = [&]() {
        std::lock_guard lock(dialog.mutex);
        if (dialog.active) {
            return;
        }
        dialog.kind = DialogKind::OpenRom;
        dialog.active = true;
        dialog.ready = false;
        SDL_ShowOpenFileDialog(
            onOpenFile, &dialog, session.window, kFilters, 2, basePath.empty() ? nullptr : basePath.c_str(), false);
    };

    const auto scanFolder = [&](const std::filesystem::path& folder) {
        std::error_code failure;
        if (!std::filesystem::is_directory(folder, failure)) {
            errorText = "That folder could not be opened.";
            errorTitle = "Could not scan the folder";
            openError = true;
            return;
        }
        int listed = 0;
        int skipped = 0;
        bool libraryFull = false;
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        const RomScan found = collectNesFiles(folder);
        for (const std::filesystem::path& file : found.files) {
            ContentDescription description;
            std::string describeError;
            if (!system_.describeContent(file, description, describeError)) {
                ++skipped;
                continue;
            }
            LibraryEntry entry;
            entry.path = pathToUtf8(file);
            entry.title = description.title.empty() ? file.stem().string() : description.title;
            entry.detail = description.detail;
            entry.contentId = description.contentId;
            entry.addedUnixMs = now;
            entry.lastPlayedUnixMs = 0;
            std::string upsertError;
            if (!LibraryService::upsert(library, std::move(entry), upsertError)) {
                libraryFull = true;
                break;
            }
            ++listed;
        }
        if (!saveLibrary()) {
            return;
        }
        libraryNote = "Added " + std::to_string(listed) + (listed == 1 ? " game." : " games.");
        libraryNote += " Subfolders were included.";
        if (skipped > 0) {
            libraryNote += " Skipped " + std::to_string(skipped) + (skipped == 1 ? " file." : " files.");
        }
        if (found.capped) {
            libraryNote += " Stopped after 2000 ROM files.";
        }
        if (found.depthLimited) {
            libraryNote += " Stopped after 16 folder levels.";
        }
        if (libraryFull) {
            libraryNote += " The library is full.";
        }
        if (found.incomplete) {
            libraryNote += " The folder could not be read completely.";
        }
    };

    const auto showScanDialog = [&]() {
        std::lock_guard lock(dialog.mutex);
        if (dialog.active) {
            return;
        }
        dialog.kind = DialogKind::ScanFolder;
        dialog.active = true;
        dialog.ready = false;
        SDL_ShowOpenFolderDialog(
            onOpenFile, &dialog, session.window, basePath.empty() ? nullptr : basePath.c_str(), false);
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
            if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED && !event.adevice.recording && session.audio != nullptr &&
                event.adevice.which == SDL_GetAudioStreamDevice(session.audio)) {
                log::info("The audio device was disconnected. Switching to the default playback device.");
                const std::string gone = config_.audioDevice;
                config_.audioDevice.clear();
                if (settingsDraft.audioDevice == gone) {
                    settingsDraft.audioDevice.clear();
                }
                openPlayback("");
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (captureBinding != nullptr) {
                    const int code = static_cast<int>(event.key.scancode);
                    if (validScancode(code)) {
                        *captureBinding = code;
                    }
                    captureBinding = nullptr;
                } else if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
                    const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                    const bool alt = (event.key.mod & SDL_KMOD_ALT) != 0;
                    const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                    const int code = static_cast<int>(event.key.scancode);
                    if (ctrl && !alt && code == config_.input.openGame) {
                        showOpenDialog();
                    } else if (ctrl && !alt && code == config_.input.reset && system_.loaded()) {
                        system_.reset();
                    } else if (alt && !ctrl && code == config_.input.fullscreen) {
                        setFullscreen(!config_.fullscreen);
                    } else if (!ctrl && !alt && !shift && code == config_.input.pause && system_.loaded()) {
                        system_.setPaused(!system_.paused());
                    } else if (!ctrl && !alt && !shift && code == config_.input.quickSave && system_.loaded()) {
                        saveSelectedSlot();
                    } else if (!ctrl && !alt && !shift && code == config_.input.quickLoad && system_.loaded()) {
                        loadSelectedSlot();
                    } else if (shift && !ctrl && !alt && system_.loaded()) {
                        int slot = -1;
                        if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_9) {
                            slot = event.key.scancode - SDL_SCANCODE_1;
                        } else if (event.key.scancode == SDL_SCANCODE_0) {
                            slot = 9;
                        }
                        if (slot >= 0) {
                            selectedSlot = slot;
                        }
                    }
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
            DialogKind kind = DialogKind::OpenRom;
            {
                std::lock_guard lock(dialog.mutex);
                if (dialog.ready) {
                    chosen = dialog.path;
                    failed = dialog.failed;
                    failure = dialog.error;
                    kind = dialog.kind;
                    dialog.ready = false;
                    dialog.path.clear();
                }
            }
            if (failed) {
                errorText = failure;
                errorTitle = kind == DialogKind::ScanFolder ? "Could not scan the folder" : "Could not open game";
                openError = true;
                log::error(failure);
            } else if (!chosen.empty()) {
                if (kind == DialogKind::ScanFolder) {
                    scanFolder(pathFromUtf8(chosen));
                } else {
                    openRom(pathFromUtf8(chosen));
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous).count();
        previous = now;
        const std::int64_t period = system_.framePeriodNs();
        if (system_.loaded() && !system_.paused()) {
            accumulated += elapsed;
            if (accumulated >= period) {
                const auto emulationStart = std::chrono::steady_clock::now();
                system_.runFrame();
                lastEmulationMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - emulationStart).count();
                ++emulatedFrames;
                const AudioView audio = system_.audio();
                if (session.audio != nullptr && config_.muted && !wasMuted) {
                    SDL_ClearAudioStream(session.audio);
                }
                wasMuted = config_.muted;
                if (session.audio != nullptr && audio.samples != nullptr && audio.count > 0 &&
                    SDL_GetAudioStreamQueued(session.audio) <
                        audioQueueSamples(config_.audioLatency) * static_cast<int>(sizeof(float))) {
                    audioScratch.resize(static_cast<std::size_t>(audio.count));
                    if (config_.muted || config_.volume <= 0.0f) {
                        std::fill(audioScratch.begin(), audioScratch.end(), 0.0f);
                    } else {
                        const float volume = std::min(config_.volume, 1.0f);
                        for (int index = 0; index < audio.count; ++index) {
                            audioScratch[static_cast<std::size_t>(index)] = audio.samples[index] * volume;
                        }
                    }
                    SDL_PutAudioStreamData(
                        session.audio, audioScratch.data(), audio.count * static_cast<int>(sizeof(float)));
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
        system_.setController(0, system_.loaded() && !uiOwnsInput ? controllerButtons(config_.input) : 0);
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
            const auto chord = [](const char* prefix, int code) {
                return prefix == nullptr ? scancodeLabel(code) : std::string(prefix) + scancodeLabel(code);
            };
            const std::string openShortcut = chord("Ctrl+", config_.input.openGame);
            const std::string resetShortcut = chord("Ctrl+", config_.input.reset);
            const std::string saveShortcut = scancodeLabel(config_.input.quickSave);
            const std::string loadShortcut = scancodeLabel(config_.input.quickLoad);
            const std::string pauseShortcut = scancodeLabel(config_.input.pause);
            const std::string fullscreenShortcut = chord("Alt+", config_.input.fullscreen);
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Open Game...", openShortcut.c_str())) {
                        showOpenDialog();
                    }
                    if (ImGui::BeginMenu("Open Recent")) {
                        const auto recentGames = LibraryService::recent(library, 10);
                        if (recentGames.empty()) {
                            ImGui::MenuItem("Empty", nullptr, false, false);
                        }
                        for (const LibraryEntry& game : recentGames) {
                            std::error_code missing;
                            const bool exists = std::filesystem::is_regular_file(pathFromUtf8(game.path), missing);
                            const std::string label = exists ? game.title : game.title + " (missing)";
                            if (ImGui::MenuItem(label.c_str(), nullptr, false, exists)) {
                                openRom(pathFromUtf8(game.path));
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Scan Folder...")) {
                        showScanDialog();
                    }
                    ImGui::MenuItem("Library", nullptr, &config_.showLibrary);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Close Game", nullptr, false, system_.loaded())) {
                        flushBattery();
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
                    if (ImGui::MenuItem(pauseLabel, pauseShortcut.c_str(), false, system_.loaded())) {
                        system_.setPaused(!system_.paused());
                    }
                    if (ImGui::MenuItem("Reset", resetShortcut.c_str(), false, system_.loaded())) {
                        system_.reset();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save State", saveShortcut.c_str(), false, system_.loaded())) {
                        saveSelectedSlot();
                    }
                    if (ImGui::MenuItem("Load State", loadShortcut.c_str(), false, system_.loaded())) {
                        loadSelectedSlot();
                    }
                    if (ImGui::MenuItem("State Slots...", nullptr, false, system_.loaded())) {
                        openStateSlots = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View")) {
                    if (ImGui::BeginMenu("Scale")) {
                        if (ImGui::MenuItem("Fit", nullptr, config_.scaleMode == ScaleMode::Fit)) {
                            config_.scaleMode = ScaleMode::Fit;
                        }
                        for (int scale = kMinimumIntegerScale; scale <= kMaximumIntegerScale; ++scale) {
                            const std::string label = std::to_string(scale) + "x";
                            const bool selected = config_.scaleMode == ScaleMode::Integer && config_.integerScale == scale;
                            if (ImGui::MenuItem(label.c_str(), nullptr, selected)) {
                                config_.scaleMode = ScaleMode::Integer;
                                config_.integerScale = scale;
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Aspect")) {
                        if (ImGui::MenuItem("Pixel", nullptr, config_.aspect == AspectMode::Pixel)) {
                            config_.aspect = AspectMode::Pixel;
                        }
                        if (ImGui::MenuItem("4:3", nullptr, config_.aspect == AspectMode::Television)) {
                            config_.aspect = AspectMode::Television;
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Preset")) {
                        const auto applyPreset = [&](PictureFilter filter) {
                            config_.filter = filter;
                            config_.brightness = kBrightness.defaultValue;
                            config_.contrast = kContrast.defaultValue;
                            config_.saturation = kSaturation.defaultValue;
                            config_.gamma = kGamma.defaultValue;
                            if (filter == PictureFilter::Sharp) {
                                config_.sharpness = kSharpness.defaultValue;
                            }
                            if (filter == PictureFilter::Crt) {
                                config_.scanline = kScanline.defaultValue;
                            }
                        };
                        if (ImGui::MenuItem(
                                "Original",
                                nullptr,
                                matchesPreset(
                                    config_.filter, PictureFilter::Nearest, config_.brightness, config_.contrast,
                                    config_.saturation, config_.gamma, config_.sharpness, config_.scanline))) {
                            applyPreset(PictureFilter::Nearest);
                        }
                        if (ImGui::MenuItem(
                                "Smooth",
                                nullptr,
                                matchesPreset(
                                    config_.filter, PictureFilter::Smooth, config_.brightness, config_.contrast,
                                    config_.saturation, config_.gamma, config_.sharpness, config_.scanline))) {
                            applyPreset(PictureFilter::Smooth);
                        }
                        if (ImGui::MenuItem(
                                "Sharp",
                                nullptr,
                                matchesPreset(
                                    config_.filter, PictureFilter::Sharp, config_.brightness, config_.contrast,
                                    config_.saturation, config_.gamma, config_.sharpness, config_.scanline))) {
                            applyPreset(PictureFilter::Sharp);
                        }
                        if (ImGui::MenuItem(
                                "CRT",
                                nullptr,
                                matchesPreset(
                                    config_.filter, PictureFilter::Crt, config_.brightness, config_.contrast,
                                    config_.saturation, config_.gamma, config_.sharpness, config_.scanline))) {
                            applyPreset(PictureFilter::Crt);
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Picture...")) {
                        openPicture = true;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Fullscreen", fullscreenShortcut.c_str(), config_.fullscreen)) {
                        setFullscreen(!config_.fullscreen);
                    }
                    if (ImGui::MenuItem("VSync", nullptr, &config_.vsync)) {
                        if (!SDL_GL_SetSwapInterval(config_.vsync ? 1 : 0)) {
                            config_.vsync = !config_.vsync;
                            errorText = SDL_GetError();
                            if (errorText.empty()) {
                                errorText = "VSync could not be changed.";
                            }
                            errorTitle = "Could not change VSync";
                            openError = true;
                        }
                    }
                    ImGui::MenuItem("Performance", nullptr, &config_.showPerformance);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Options")) {
                    if (ImGui::MenuItem("Settings...")) {
                        openSettings = true;
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
                    if (ImGui::MenuItem("Open Source Licenses")) {
                        openLicenses = true;
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
            std::string scaleStatus;
            if (config_.showLibrary) {
                ImGui::BeginChild("##library", ImVec2(220.0f, viewportHeight), ImGuiChildFlags_Borders);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##find", "Find", libraryQuery, sizeof(libraryQuery));
                int sortIndex = librarySort == LibrarySort::LastPlayed ? 1 : 0;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Combo("##sort", &sortIndex, "Title\0Last played\0")) {
                    librarySort = sortIndex == 1 ? LibrarySort::LastPlayed : LibrarySort::Title;
                }
                if (ImGui::Button("Scan folder")) {
                    showScanDialog();
                }
                ImGui::TextWrapped("Subfolders are included.");
                if (!libraryNote.empty()) {
                    ImGui::TextWrapped("%s", libraryNote.c_str());
                }
                const float listHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
                ImGui::BeginChild("##library-list", ImVec2(0.0f, listHeight > 1.0f ? listHeight : 1.0f), ImGuiChildFlags_Borders);
                const auto shown = LibraryService::query(library, libraryQuery, librarySort);
                for (const LibraryEntry& game : shown) {
                    ImGui::PushID(game.path.c_str());
                    std::error_code missing;
                    const bool exists = std::filesystem::is_regular_file(pathFromUtf8(game.path), missing);
                    const std::string label = exists ? game.title : game.title + " (missing)";
                    if (ImGui::Selectable(label.c_str(), selectedGame == game.path)) {
                        selectedGame = game.path;
                        if (exists && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            openRom(pathFromUtf8(game.path));
                        }
                    }
                    if (!game.detail.empty()) {
                        ImGui::TextDisabled("%s", game.detail.c_str());
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                std::error_code selectedError;
                const bool canPlay =
                    !selectedGame.empty() && std::filesystem::is_regular_file(pathFromUtf8(selectedGame), selectedError);
                ImGui::BeginDisabled(!canPlay);
                if (ImGui::Button("Play")) {
                    openRom(pathFromUtf8(selectedGame));
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(selectedGame.empty());
                if (ImGui::Button("Remove")) {
                    removePath = selectedGame;
                    openRemove = true;
                }
                ImGui::EndDisabled();
                ImGui::EndChild();
                ImGui::SameLine();
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
                const float scaleX = io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f;
                const float scaleY = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
                const int pixelW = std::max(1, static_cast<int>(std::floor(static_cast<double>(availW) * scaleX)));
                const int pixelH = std::max(1, static_cast<int>(std::floor(static_cast<double>(availH) * scaleY)));
                const PicturePlacement placement = placePicture(
                    pixelW, pixelH, frame.width, frame.height, config_.scaleMode, config_.integerScale, config_.aspect);
                scaleStatus = placement.fellBack ? placementNote(config_.scaleMode, config_.integerScale, placement)
                                                 : placementLabel(placement, config_.aspect);
                if (placement.width > 0 && placement.height > 0) {
                    const float widgetW = static_cast<float>(placement.width) / scaleX;
                    const float widgetH = static_cast<float>(placement.height) / scaleY;
                    const ImVec2 cursor = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor.x + (availW - widgetW) * 0.5f, cursor.y + (availH - widgetH) * 0.5f));
                    const auto presentStart = std::chrono::steady_clock::now();
                    PictureAdjustments adjustments;
                    adjustments.filter = config_.filter;
                    adjustments.brightness = config_.brightness;
                    adjustments.contrast = config_.contrast;
                    adjustments.saturation = config_.saturation;
                    adjustments.gamma = config_.gamma;
                    adjustments.sharpness = config_.sharpness;
                    adjustments.scanline = config_.scanline;
                    const bool shaded = picturePass.ready() &&
                                        picturePass.draw(texture.id, frame.width, frame.height, placement.width, placement.height, adjustments);
                    lastPresentMs =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - presentStart).count();
                    if (shaded) {
                        ImGui::Image(static_cast<ImTextureID>(picturePass.texture()), ImVec2(widgetW, widgetH));
                    } else {
                        ImGui::Image(static_cast<ImTextureID>(texture.id), ImVec2(widgetW, widgetH));
                        if (!scaleStatus.empty()) {
                            scaleStatus += " - ";
                        }
                        scaleStatus += "crisp pixels";
                    }
                }
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
            if (!scaleStatus.empty()) {
                status += " - ";
                status += scaleStatus;
            }
            if (config_.showPerformance && hostFps > 0.0) {
                char performance[96] = {};
                std::snprintf(
                    performance,
                    sizeof(performance),
                    "%.0f fps, %.0f emulated, emu %.1f ms, present %.1f ms",
                    hostFps,
                    emulatedFps,
                    lastEmulationMs,
                    lastPresentMs);
                status += " - ";
                status += performance;
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

            if (openSettings) {
                settingsDraft = config_;
                captureBinding = nullptr;
                confirmReset = false;
                refreshAudioList = true;
                ImGui::OpenPopup("Settings");
                openSettings = false;
            }
            const ImGuiViewport* settingsViewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(settingsViewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(560.0f, 520.0f), ImGuiCond_Appearing);
            if (ImGui::BeginPopupModal("Settings", nullptr)) {
                const auto applyDraft = [&]() {
                    if (bindingConflict(settingsDraft.input).has_value()) {
                        return;
                    }
                    const bool deviceChanged = settingsDraft.audioDevice != config_.audioDevice;
                    const bool themeChanged = settingsDraft.theme != config_.theme;
                    const bool logChanged = settingsDraft.logLevel != config_.logLevel;
                    const bool vsyncChanged = settingsDraft.vsync != config_.vsync;
                    const bool wasFullscreen = (SDL_GetWindowFlags(session.window) & SDL_WINDOW_FULLSCREEN) != 0;
                    const bool fullscreenChanged = settingsDraft.fullscreen != wasFullscreen;
                    const int keptWidth = config_.windowWidth;
                    const int keptHeight = config_.windowHeight;
                    config_ = settingsDraft;
                    config_.windowWidth = keptWidth;
                    config_.windowHeight = keptHeight;
                    settingsDraft.windowWidth = keptWidth;
                    settingsDraft.windowHeight = keptHeight;
                    if (themeChanged) {
                        applyXpTheme(config_.theme, contentScale);
                        palette = xpPalette(config_.theme);
                    }
                    if (logChanged) {
                        log::setLevel(config_.logLevel);
                    }
                    if (vsyncChanged && !SDL_GL_SetSwapInterval(config_.vsync ? 1 : 0)) {
                        config_.vsync = !config_.vsync;
                        settingsDraft.vsync = config_.vsync;
                        errorText = SDL_GetError();
                        if (errorText.empty()) {
                            errorText = "VSync could not be changed.";
                        }
                        errorTitle = "Could not change VSync";
                        openError = true;
                    }
                    if (deviceChanged) {
                        openPlayback(config_.audioDevice);
                    }
                    if (config_.muted && session.audio != nullptr) {
                        SDL_ClearAudioStream(session.audio);
                    }
                    if (fullscreenChanged) {
                        config_.fullscreen = wasFullscreen;
                        setFullscreen(settingsDraft.fullscreen);
                        settingsDraft.fullscreen = config_.fullscreen;
                    }
                };
                const auto reloadDevices = [&]() {
                    playbackNames.clear();
                    int count = 0;
                    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
                    if (devices == nullptr) {
                        return;
                    }
                    for (int index = 0; index < count; ++index) {
                        const char* name = SDL_GetAudioDeviceName(devices[index]);
                        if (name != nullptr && name[0] != '\0') {
                            playbackNames.emplace_back(name);
                        }
                    }
                    SDL_free(devices);
                };
                if (ImGui::BeginTabBar("settings-tabs")) {
                    if (ImGui::BeginTabItem("General")) {
                        int theme = static_cast<int>(settingsDraft.theme);
                        if (ImGui::Combo("Theme", &theme, "XP Blue\0Olive Green\0Silver\0")) {
                            settingsDraft.theme = static_cast<Theme>(theme);
                        }
                        int level = static_cast<int>(settingsDraft.logLevel);
                        if (ImGui::Combo("Log level", &level, "Trace\0Debug\0Info\0Warn\0Error\0Critical\0")) {
                            settingsDraft.logLevel = static_cast<LogLevel>(level);
                        }
                        ImGui::Checkbox("Show the game library", &settingsDraft.showLibrary);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Video")) {
                        int scale = static_cast<int>(settingsDraft.scaleMode);
                        if (ImGui::Combo("Scale", &scale, "Fit\0Integer\0")) {
                            settingsDraft.scaleMode = static_cast<ScaleMode>(scale);
                        }
                        ImGui::SliderInt(
                            "Integer scale", &settingsDraft.integerScale, kMinimumIntegerScale, kMaximumIntegerScale);
                        int aspect = static_cast<int>(settingsDraft.aspect);
                        if (ImGui::Combo("Aspect", &aspect, "Pixel\0Television 4:3\0")) {
                            settingsDraft.aspect = static_cast<AspectMode>(aspect);
                        }
                        int filter = static_cast<int>(settingsDraft.filter);
                        if (ImGui::Combo("Filter", &filter, "Original\0Smooth\0Sharp\0CRT\0")) {
                            settingsDraft.filter = static_cast<PictureFilter>(filter);
                        }
                        ImGui::SliderFloat("Brightness", &settingsDraft.brightness, kBrightness.minimum, kBrightness.maximum, "%.2f");
                        ImGui::SliderFloat("Contrast", &settingsDraft.contrast, kContrast.minimum, kContrast.maximum, "%.2f");
                        ImGui::SliderFloat(
                            "Saturation", &settingsDraft.saturation, kSaturation.minimum, kSaturation.maximum, "%.2f");
                        ImGui::SliderFloat("Gamma", &settingsDraft.gamma, kGamma.minimum, kGamma.maximum, "%.2f");
                        if (settingsDraft.filter == PictureFilter::Sharp) {
                            ImGui::SliderFloat(
                                "Sharpness", &settingsDraft.sharpness, kSharpness.minimum, kSharpness.maximum, "%.2f");
                        }
                        if (settingsDraft.filter == PictureFilter::Crt) {
                            ImGui::SliderFloat("Scanlines", &settingsDraft.scanline, kScanline.minimum, kScanline.maximum, "%.2f");
                        }
                        ImGui::Checkbox("Fullscreen", &settingsDraft.fullscreen);
                        ImGui::Checkbox("VSync", &settingsDraft.vsync);
                        ImGui::Checkbox("Performance", &settingsDraft.showPerformance);
                        ImGui::TextWrapped("These settings change how the picture is shown. The emulated image stays as the PPU drew it.");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Audio")) {
                        if (refreshAudioList) {
                            reloadDevices();
                            refreshAudioList = false;
                        }
                        ImGui::SliderFloat("Volume", &settingsDraft.volume, 0.0f, 1.0f, "%.2f");
                        ImGui::Checkbox("Mute", &settingsDraft.muted);
                        int latency = static_cast<int>(settingsDraft.audioLatency);
                        if (ImGui::Combo("Latency", &latency, "Short\0Balanced\0Long\0")) {
                            settingsDraft.audioLatency = static_cast<AudioLatency>(latency);
                        }
                        std::vector<std::string> choices;
                        choices.emplace_back("System default");
                        for (const std::string& name : playbackNames) {
                            choices.push_back(name);
                        }
                        if (!settingsDraft.audioDevice.empty() &&
                            std::find(playbackNames.begin(), playbackNames.end(), settingsDraft.audioDevice) == playbackNames.end()) {
                            choices.push_back(settingsDraft.audioDevice);
                        }
                        int currentDevice = 0;
                        for (int index = 1; index < static_cast<int>(choices.size()); ++index) {
                            if (choices[static_cast<std::size_t>(index)] == settingsDraft.audioDevice) {
                                currentDevice = index;
                            }
                        }
                        if (ImGui::Combo("Device", &currentDevice, playbackDeviceItem, &choices, static_cast<int>(choices.size()))) {
                            settingsDraft.audioDevice =
                                currentDevice == 0 ? std::string() : choices[static_cast<std::size_t>(currentDevice)];
                        }
                        if (ImGui::Button("Refresh devices")) {
                            reloadDevices();
                        }
                        ImGui::TextWrapped("Volume is applied when samples are queued. The emulated audio is not rewritten.");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Input")) {
                        ImGui::TextUnformatted("Controller");
                        drawBindingButton("A", settingsDraft.input.a, nullptr, captureBinding);
                        drawBindingButton("B", settingsDraft.input.b, nullptr, captureBinding);
                        drawBindingButton("Select", settingsDraft.input.select, nullptr, captureBinding);
                        drawBindingButton("Start", settingsDraft.input.start, nullptr, captureBinding);
                        drawBindingButton("Up", settingsDraft.input.up, nullptr, captureBinding);
                        drawBindingButton("Down", settingsDraft.input.down, nullptr, captureBinding);
                        drawBindingButton("Left", settingsDraft.input.left, nullptr, captureBinding);
                        drawBindingButton("Right", settingsDraft.input.right, nullptr, captureBinding);
                        ImGui::Separator();
                        ImGui::TextUnformatted("Shortcuts");
                        drawBindingButton("Open game", settingsDraft.input.openGame, "Ctrl+", captureBinding);
                        drawBindingButton("Quick save", settingsDraft.input.quickSave, nullptr, captureBinding);
                        drawBindingButton("Quick load", settingsDraft.input.quickLoad, nullptr, captureBinding);
                        drawBindingButton("Pause", settingsDraft.input.pause, nullptr, captureBinding);
                        drawBindingButton("Fullscreen", settingsDraft.input.fullscreen, "Alt+", captureBinding);
                        drawBindingButton("Reset", settingsDraft.input.reset, "Ctrl+", captureBinding);
                        ImGui::TextWrapped(
                            "Click a key, then press the replacement. Numpad Enter also presses Start. "
                            "Shift+1 through Shift+0 still select state slots.");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Paths")) {
                        if (appPaths.has_value()) {
                            ImGui::TextWrapped("Data folder");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->root).c_str());
                            ImGui::TextWrapped("Configuration");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->configFile).c_str());
                            ImGui::TextWrapped("Library");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->library / "library.json").c_str());
                            ImGui::TextWrapped("Save states");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->states).c_str());
                            ImGui::TextWrapped("Battery RAM");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->saves).c_str());
                            ImGui::TextWrapped("Logs");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->logs).c_str());
                            if (ImGui::Button("Open data folder")) {
#if defined(_WIN32)
                                const auto opened = reinterpret_cast<INT_PTR>(ShellExecuteW(
                                    nullptr, L"open", appPaths->root.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
                                if (opened <= 32) {
                                    errorText = "The data folder could not be opened.";
                                    errorTitle = "Could not open the folder";
                                    openError = true;
                                }
#else
                                errorText = "Opening the data folder is not available on this system.";
                                errorTitle = "Could not open the folder";
                                openError = true;
#endif
                            }
                        } else {
                            ImGui::TextUnformatted("The data folder could not be found.");
                        }
                        ImGui::TextWrapped("Set IRIS_DATA_DIR before starting Iris to use a different data folder.");
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                if (const std::optional<std::string> conflict = bindingConflict(settingsDraft.input)) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.0f, 0.0f, 1.0f));
                    ImGui::TextWrapped("%s", conflict->c_str());
                    ImGui::PopStyleColor();
                }
                const bool conflicted = bindingConflict(settingsDraft.input).has_value();
                ImGui::BeginDisabled(conflicted);
                if (ImGui::Button("OK", ImVec2(75.0f, 0.0f))) {
                    applyDraft();
                    captureBinding = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Apply", ImVec2(75.0f, 0.0f))) {
                    applyDraft();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(75.0f, 0.0f))) {
                    captureBinding = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset to defaults")) {
                    confirmReset = true;
                }
                if (confirmReset) {
                    ImGui::OpenPopup("Reset settings");
                    confirmReset = false;
                }
                if (ImGui::BeginPopupModal("Reset settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped(
                        "Theme, picture, audio, and controls return to their defaults. The window size stays as it is.");
                    if (ImGui::Button("Reset", ImVec2(75.0f, 0.0f))) {
                        const int width = settingsDraft.windowWidth;
                        const int height = settingsDraft.windowHeight;
                        const bool fullscreen = settingsDraft.fullscreen;
                        settingsDraft = ConfigService::defaults();
                        settingsDraft.windowWidth = width;
                        settingsDraft.windowHeight = height;
                        settingsDraft.fullscreen = fullscreen;
                        captureBinding = nullptr;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(75.0f, 0.0f))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::EndPopup();
            }
            if (openRemove) {
                ImGui::OpenPopup("Remove from library");
                openRemove = false;
            }
            if (ImGui::BeginPopupModal("Remove from library", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped("Remove this game from the library? The file stays where it is.");
                if (ImGui::Button("Remove", ImVec2(75.0f, 0.0f))) {
                    if (LibraryService::remove(library, removePath)) {
                        if (selectedGame == removePath) {
                            selectedGame.clear();
                        }
                        saveLibrary();
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(75.0f, 0.0f))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            drawModal("About Iris Emulator", aboutText.c_str(), openAbout);
            if (openPicture) {
                ImGui::OpenPopup("Picture");
                openPicture = false;
            }
            if (ImGui::BeginPopupModal("Picture", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("These settings change how the picture is shown. The emulated image stays as the PPU drew it.");
                ImGui::Text("Filter: %s", pictureFilterName(config_.filter));
                ImGui::Spacing();
                ImGui::SetNextItemWidth(220.0f);
                ImGui::SliderFloat("Brightness", &config_.brightness, kBrightness.minimum, kBrightness.maximum, "%.2f");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::SliderFloat("Contrast", &config_.contrast, kContrast.minimum, kContrast.maximum, "%.2f");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::SliderFloat("Saturation", &config_.saturation, kSaturation.minimum, kSaturation.maximum, "%.2f");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::SliderFloat("Gamma", &config_.gamma, kGamma.minimum, kGamma.maximum, "%.2f");
                if (config_.filter == PictureFilter::Sharp) {
                    ImGui::SetNextItemWidth(220.0f);
                    ImGui::SliderFloat("Sharpness", &config_.sharpness, kSharpness.minimum, kSharpness.maximum, "%.2f");
                }
                if (config_.filter == PictureFilter::Crt) {
                    ImGui::SetNextItemWidth(220.0f);
                    ImGui::SliderFloat("Scanlines", &config_.scanline, kScanline.minimum, kScanline.maximum, "%.2f");
                }
                if (ImGui::Button("Original colors")) {
                    config_.brightness = kBrightness.defaultValue;
                    config_.contrast = kContrast.defaultValue;
                    config_.saturation = kSaturation.defaultValue;
                    config_.gamma = kGamma.defaultValue;
                }
                ImGui::SameLine();
                if (ImGui::Button("Close", ImVec2(75.0f, 0.0f))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (openStateSlots) {
                ImGui::OpenPopup("State Slots");
                openStateSlots = false;
                refreshStateSlots = true;
            }
            if (ImGui::BeginPopupModal("State Slots", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (refreshStateSlots) {
                    refreshStateSlots = false;
                    for (int slot = 0; slot < kStateSlots; ++slot) {
                        StatePreview preview;
                        const auto file = slotFile(slot);
                        std::error_code failure;
                        std::vector<std::uint8_t> bytes;
                        std::string error;
                        if (!file.empty() && std::filesystem::is_regular_file(file, failure) &&
                            readFile(file, 4 * 1024 * 1024, bytes, error)) {
                            system_.inspectState(bytes.data(), bytes.size(), preview);
                        }
                        slotPreviews[static_cast<std::size_t>(slot)] = preview;
                        const std::string description = preview.readable ? preview.description : std::string{};
                        slotDescriptions[static_cast<std::size_t>(slot)] = description;
                        auto& edit = slotDescriptionEdit[static_cast<std::size_t>(slot)];
                        edit.fill('\0');
                        std::snprintf(edit.data(), edit.size(), "%s", description.c_str());
                        if (preview.readable && !preview.thumbnailRgba.empty()) {
                            uploadFrame(slotTextures[static_cast<std::size_t>(slot)].id,
                                        FrameView{preview.thumbnailRgba.data(), preview.thumbnailWidth, preview.thumbnailHeight});
                        }
                    }
                }
                ImGui::TextUnformatted("F5 saves the current slot. F7 loads it. Shift+1 through Shift+0 select a slot.");
                ImGui::Spacing();
                if (ImGui::BeginChild("##state-slots", ImVec2(560.0f, 360.0f), ImGuiChildFlags_Borders)) {
                    for (int slot = 0; slot < kStateSlots; ++slot) {
                        ImGui::PushID(slot);
                        const StatePreview& preview = slotPreviews[static_cast<std::size_t>(slot)];
                        if (preview.readable && !preview.thumbnailRgba.empty()) {
                            ImGui::Image(static_cast<ImTextureID>(slotTextures[static_cast<std::size_t>(slot)].id), ImVec2(64.0f, 60.0f));
                        } else {
                            ImGui::Dummy(ImVec2(64.0f, 60.0f));
                        }
                        ImGui::SameLine();
                        ImGui::BeginGroup();
                        const std::string heading = "Slot " + std::to_string(slot + 1) + (slot == selectedSlot ? " (current)" : "");
                        ImGui::TextUnformatted(heading.c_str());
                        if (!preview.readable) {
                            ImGui::TextUnformatted("Empty");
                        } else {
                            std::string when = preview.title;
                            if (preview.savedAtUnixMs > 0) {
                                const std::time_t seconds = static_cast<std::time_t>(preview.savedAtUnixMs / 1000);
                                std::tm local{};
#if defined(_WIN32)
                                localtime_s(&local, &seconds);
#else
                                localtime_r(&seconds, &local);
#endif
                                char buffer[32] = {};
                                if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local) > 0) {
                                    when += "  ";
                                    when += buffer;
                                }
                            }
                            ImGui::TextUnformatted(when.c_str());
                            if (!preview.sameGame) {
                                ImGui::TextUnformatted("Different game");
                            }
                        }
                        ImGui::SetNextItemWidth(220.0f);
                        ImGui::InputText("Description", slotDescriptionEdit[static_cast<std::size_t>(slot)].data(),
                                         slotDescriptionEdit[static_cast<std::size_t>(slot)].size());
                        if (ImGui::Button("Use")) {
                            selectedSlot = slot;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Save")) {
                            selectedSlot = slot;
                            slotDescriptions[static_cast<std::size_t>(slot)] = slotDescriptionEdit[static_cast<std::size_t>(slot)].data();
                            saveSelectedSlot();
                        }
                        ImGui::SameLine();
                        ImGui::BeginDisabled(!(preview.readable && preview.sameGame));
                        if (ImGui::Button("Load")) {
                            selectedSlot = slot;
                            loadSelectedSlot();
                        }
                        ImGui::EndDisabled();
                        ImGui::EndGroup();
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
                if (ImGui::Button("Close", ImVec2(75.0f, 0.0f))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            drawModal(
                "Legal Notices",
                "Iris Emulator does not include commercial games and does not download ROMs. "
                "Iris Demo is an original program written for this project.\n\n"
                "Open only game files you have obtained and are legally entitled to use. "
                "Owning an original cartridge does not by itself make every method of dumping or downloading a game lawful. "
                "Those rules depend on jurisdiction and circumstance.",
                openLegal);
            drawModal(
                "Open Source Licenses",
                "This build uses the following open-source libraries. Full license texts are in THIRD_PARTY_NOTICES.md beside the program.\n\n"
                "SDL 3.4.18 — zlib License\n"
                "Dear ImGui 1.92.9b — MIT License\n"
                "spdlog 1.17.0 — MIT License, which bundles fmt, also MIT\n"
                "nlohmann/json 3.12.0 — MIT License\n"
                "Catch2 3.16.1 — Boost Software License 1.0. Test builds only. Not part of the application.\n\n"
                "Iris does not include Tahoma. On Windows it reads the copy installed with the operating system.",
                openLicenses);
            drawModal(errorTitle.c_str(), errorText.c_str(), openError);
        }
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(palette.faceRed, palette.faceGreen, palette.faceBlue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(session.window);
        ++hostFrames;
        const double fpsElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsWindow).count();
        if (fpsElapsed >= 0.5) {
            hostFps = static_cast<double>(hostFrames) / fpsElapsed;
            emulatedFps = static_cast<double>(emulatedFrames) / fpsElapsed;
            hostFrames = 0;
            emulatedFrames = 0;
            fpsWindow = std::chrono::steady_clock::now();
        }
    }

    flushBattery();
    if ((SDL_GetWindowFlags(session.window) & SDL_WINDOW_FULLSCREEN) == 0) {
        SDL_GetWindowSize(session.window, &windowWidth_, &windowHeight_);
    }
    config_.windowWidth = windowWidth_;
    config_.windowHeight = windowHeight_;
    return 0;
}

}  // namespace iris
