#include "iris/desktop_shell.hpp"

#include "iris/library.hpp"
#include "iris/log.hpp"
#include "iris/paths.hpp"
#include "iris/picture_pass.hpp"
#include "iris/version.hpp"
#include "iris/shell_theme.hpp"

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
#include <mmsystem.h>
#include <shellapi.h>
#include <intrin.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace iris {
namespace {

#if defined(_WIN32)
// WASAPI shared mode on some Realtek drivers rejects every format, including the mix
// format Windows reports. DirectSound ends up in that same engine and Play fails.
// waveOut still accepts 16-bit stereo and consumes the buffer.
class WavePlayback {
public:
    bool start() {
        stop();
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 48000;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = 48000 * 4;
        if (waveOutOpen(&wave_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            wave_ = nullptr;
            return false;
        }
        open_ = true;
        return true;
    }

    void stop() {
        if (wave_ == nullptr) {
            open_ = false;
            return;
        }
        waveOutReset(wave_);
        for (int slot = 0; slot < kSlots; ++slot) {
            if ((headers_[slot].dwFlags & WHDR_PREPARED) != 0) {
                waveOutUnprepareHeader(wave_, &headers_[slot], sizeof(WAVEHDR));
            }
            headers_[slot] = WAVEHDR{};
        }
        waveOutClose(wave_);
        wave_ = nullptr;
        open_ = false;
        slot_ = 0;
    }

    ~WavePlayback() { stop(); }

    [[nodiscard]] bool open() const { return open_; }

    [[nodiscard]] int queuedFrames() const {
        int frames = 0;
        for (int slot = 0; slot < kSlots; ++slot) {
            const WAVEHDR& header = headers_[slot];
            if ((header.dwFlags & WHDR_PREPARED) != 0 && (header.dwFlags & WHDR_DONE) == 0) {
                frames += static_cast<int>(header.dwBufferLength / 4);
            }
        }
        return frames;
    }

    void clear() {
        if (wave_ != nullptr) {
            waveOutReset(wave_);
        }
    }

    bool write(const float* samples, int count, float volume) {
        if (wave_ == nullptr || samples == nullptr || count <= 0) {
            return false;
        }
        int slot = -1;
        for (int index = 0; index < kSlots; ++index) {
            const int candidate = (slot_ + index) % kSlots;
            const DWORD flags = headers_[candidate].dwFlags;
            if ((flags & WHDR_PREPARED) == 0 || (flags & WHDR_DONE) != 0) {
                slot = candidate;
                break;
            }
        }
        if (slot < 0) {
            return false;
        }
        WAVEHDR& header = headers_[slot];
        if ((header.dwFlags & WHDR_PREPARED) != 0) {
            waveOutUnprepareHeader(wave_, &header, sizeof(header));
        }
        auto& storage = storage_[slot];
        storage.resize(static_cast<std::size_t>(count) * 2);
        for (int index = 0; index < count; ++index) {
            float value = samples[index] * volume;
            if (value > 1.0f) {
                value = 1.0f;
            }
            if (value < -1.0f) {
                value = -1.0f;
            }
            const auto sample = static_cast<std::int16_t>(value * 32767.0f);
            storage[static_cast<std::size_t>(index) * 2] = sample;
            storage[static_cast<std::size_t>(index) * 2 + 1] = sample;
        }
        header = WAVEHDR{};
        header.lpData = reinterpret_cast<LPSTR>(storage.data());
        header.dwBufferLength = static_cast<DWORD>(storage.size() * sizeof(std::int16_t));
        if (waveOutPrepareHeader(wave_, &header, sizeof(header)) != MMSYSERR_NOERROR) {
            return false;
        }
        if (waveOutWrite(wave_, &header, sizeof(header)) != MMSYSERR_NOERROR) {
            waveOutUnprepareHeader(wave_, &header, sizeof(header));
            return false;
        }
        slot_ = (slot + 1) % kSlots;
        return true;
    }

private:
    static constexpr int kSlots = 8;
    HWAVEOUT wave_ = nullptr;
    bool open_ = false;
    int slot_ = 0;
    WAVEHDR headers_[kSlots]{};
    std::vector<std::int16_t> storage_[kSlots];
};
#endif

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

struct WindowChrome {
    int width = 0;
    int height = 0;
    int border = 0;
    int titleBottom = 0;
    int grab = 8;
    SDL_Rect buttons[3]{};
    int buttonCount = 0;
    bool resizable = false;
};

bool containsPoint(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

void updateTopCornerShape(SDL_Window* window, int radius) {
#if defined(_WIN32)
    if (window == nullptr) {
        return;
    }
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (hwnd == nullptr) {
        return;
    }
    RECT client{};
    if (GetClientRect(hwnd, &client) == FALSE) {
        return;
    }
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    static HWND appliedWindow = nullptr;
    static int appliedWidth = -1;
    static int appliedHeight = -1;
    static int appliedRadius = -1;
    if (hwnd == appliedWindow && width == appliedWidth && height == appliedHeight && radius == appliedRadius) {
        return;
    }
    appliedWindow = hwnd;
    appliedWidth = width;
    appliedHeight = height;
    appliedRadius = radius;
    if (radius <= 1 || width <= radius * 2 || height <= radius * 2) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }
    const int diameter = radius * 2;
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, diameter, diameter);
    if (region == nullptr) {
        return;
    }
    // The round region curves every corner. Cover the bottom corners so only the top stays round.
    HRGN squareBottom = CreateRectRgn(0, height - radius, width + 1, height + 1);
    if (squareBottom != nullptr) {
        CombineRgn(region, region, squareBottom, RGN_OR);
        DeleteObject(squareBottom);
    }
    if (SetWindowRgn(hwnd, region, TRUE) == 0) {
        DeleteObject(region);
    }
#else
    (void)window;
    (void)radius;
#endif
}

SDL_HitTestResult SDLCALL chromeHitTest(SDL_Window*, const SDL_Point* area, void* data) {
    const auto* chrome = static_cast<const WindowChrome*>(data);
    if (chrome == nullptr || area == nullptr || chrome->border <= 0 || chrome->width <= 0 || chrome->height <= 0) {
        return SDL_HITTEST_NORMAL;
    }
    const int x = area->x;
    const int y = area->y;
    for (int index = 0; index < chrome->buttonCount; ++index) {
        if (containsPoint(chrome->buttons[index], x, y)) {
            return SDL_HITTEST_NORMAL;
        }
    }
    const int band = std::max(chrome->border, chrome->grab);
    const bool left = x < band;
    const bool right = x >= chrome->width - band;
    const bool top = y < band;
    const bool bottom = y >= chrome->height - band;
    if (chrome->resizable) {
        if (top && left) {
            return SDL_HITTEST_RESIZE_TOPLEFT;
        }
        if (top && right) {
            return SDL_HITTEST_RESIZE_TOPRIGHT;
        }
        if (bottom && left) {
            return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        }
        if (bottom && right) {
            return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        }
        if (top) {
            return SDL_HITTEST_RESIZE_TOP;
        }
        if (bottom) {
            return SDL_HITTEST_RESIZE_BOTTOM;
        }
        if (left) {
            return SDL_HITTEST_RESIZE_LEFT;
        }
        if (right) {
            return SDL_HITTEST_RESIZE_RIGHT;
        }
    }
    if (y < chrome->titleBottom && x >= chrome->border && x < chrome->width - chrome->border) {
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
}

bool beginClassicPopup(const char* title, const ShellPalette& palette, ImGuiWindowFlags flags = 0) {
    const ImVec4 bar(palette.titleBottom.red, palette.titleBottom.green, palette.titleBottom.blue, 1.0f);
    const ImVec4 edge(palette.frame.red, palette.frame.green, palette.frame.blue, 1.0f);
    const float scale = ImGui::GetStyle().FontScaleDpi > 1.0f ? ImGui::GetStyle().FontScaleDpi : 1.0f;
    ImGui::PushStyleColor(ImGuiCol_TitleBg, bar);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, bar);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, bar);
    ImGui::PushStyleColor(ImGuiCol_Border, edge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 4.0f * scale);
    const bool shown = ImGui::BeginPopupModal(title, nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    if (shown) {
        paintDialogTitle(title, palette);
    }
    return shown;
}

void drawModal(const char* title, const char* body, bool& open, const ShellPalette& palette) {
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
    const ImVec4 bar(palette.titleBottom.red, palette.titleBottom.green, palette.titleBottom.blue, 1.0f);
    const ImVec4 edge(palette.frame.red, palette.frame.green, palette.frame.blue, 1.0f);
    const float scale = ImGui::GetStyle().FontScaleDpi > 1.0f ? ImGui::GetStyle().FontScaleDpi : 1.0f;
    ImGui::PushStyleColor(ImGuiCol_TitleBg, bar);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, bar);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, bar);
    ImGui::PushStyleColor(ImGuiCol_Border, edge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 4.0f * scale);
    const bool shown = ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    if (!shown) {
        return;
    }
    paintDialogTitle(title, palette);
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

int audioPaceSamples(AudioLatency latency) {
    switch (latency) {
    case AudioLatency::Short:
        return 48000 * 20 / 1000;
    case AudioLatency::Long:
        return 48000 * 80 / 1000;
    case AudioLatency::Balanced:
        return 48000 * 40 / 1000;
    }
    return 48000 * 40 / 1000;
}

// Below this the device can run dry before the next paced frame. One NTSC frame is
// 800 samples at 48000 Hz, so 400 ran out during a late battery frame and the
// waveform device clicked. Two frames of cushion still lets the frame clock lead.
constexpr int kAudioRefillSamples = 1600;

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

struct PadSample {
    std::uint8_t held = 0;
    std::uint8_t turbo = 0;
};

PadSample controllerButtons(const InputBindings& bindings, const bool* pressed, int pressedCount) {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    const auto down = [&](int code) {
        const bool fromState = keys != nullptr && code >= 0 && code < count && keys[code];
        const bool fromEvent = pressed != nullptr && code >= 0 && code < pressedCount && pressed[code];
        return fromState || fromEvent;
    };
    PadSample sample;
    if (down(bindings.a)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonA);
    if (down(bindings.b)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonB);
    if (down(bindings.select)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonSelect);
    if (down(bindings.start) || down(SDL_SCANCODE_KP_ENTER)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonStart);
    if (down(bindings.up)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonUp);
    if (down(bindings.down)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonDown);
    if (down(bindings.left)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonLeft);
    if (down(bindings.right)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonRight);
    if (down(bindings.turboA)) sample.turbo = static_cast<std::uint8_t>(sample.turbo | kButtonA);
    if (down(bindings.turboB)) sample.turbo = static_cast<std::uint8_t>(sample.turbo | kButtonB);
    return sample;
}

const char* gamepadButtonName(int button) {
    switch (button) {
    case 0:
        return "South";
    case 1:
        return "East";
    case 2:
        return "West";
    case 3:
        return "North";
    case 4:
        return "Back";
    case 6:
        return "Start";
    case 9:
        return "Left shoulder";
    case 10:
        return "Right shoulder";
    case 11:
        return "Up";
    case 12:
        return "Down";
    case 13:
        return "Left";
    case 14:
        return "Right";
    default:
        return "Button";
    }
}

void cropPicture(
    const std::vector<std::uint8_t>& source,
    int sourceWidth,
    int sourceHeight,
    int crop,
    std::vector<std::uint8_t>& destination,
    int& width,
    int& height) {
    if (source.empty() || sourceWidth <= 0 || sourceHeight <= 0 || crop < 0 || crop * 2 >= sourceWidth ||
        crop * 2 >= sourceHeight) {
        destination = source;
        width = sourceWidth;
        height = sourceHeight;
        return;
    }
    width = sourceWidth - crop * 2;
    height = sourceHeight - crop * 2;
    destination.resize(static_cast<std::size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        const std::size_t from = (static_cast<std::size_t>(y + crop) * static_cast<std::size_t>(sourceWidth) +
                                  static_cast<std::size_t>(crop)) *
                                 4;
        std::memcpy(destination.data() + static_cast<std::size_t>(y * width * 4), source.data() + from, static_cast<std::size_t>(width * 4));
    }
}

bool writeBmp(const std::filesystem::path& path, const std::uint8_t* rgba, int width, int height) {
    if (rgba == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    const int rowStride = width * 3;
    const int padded = (rowStride + 3) & ~3;
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(padded * height);
    const std::uint32_t fileBytes = 54u + pixelBytes;
    std::vector<std::uint8_t> file(fileBytes, 0);
    file[0] = 'B';
    file[1] = 'M';
    file[2] = static_cast<std::uint8_t>(fileBytes);
    file[3] = static_cast<std::uint8_t>(fileBytes >> 8);
    file[4] = static_cast<std::uint8_t>(fileBytes >> 16);
    file[5] = static_cast<std::uint8_t>(fileBytes >> 24);
    file[10] = 54;
    file[14] = 40;
    file[18] = static_cast<std::uint8_t>(width);
    file[19] = static_cast<std::uint8_t>(width >> 8);
    file[22] = static_cast<std::uint8_t>(height);
    file[23] = static_cast<std::uint8_t>(height >> 8);
    file[26] = 1;
    file[28] = 24;
    for (int y = 0; y < height; ++y) {
        std::uint8_t* row = file.data() + 54 + static_cast<std::size_t>(height - 1 - y) * static_cast<std::size_t>(padded);
        const std::uint8_t* source = rgba + static_cast<std::size_t>(y * width * 4);
        for (int x = 0; x < width; ++x) {
            row[x * 3] = source[x * 4 + 2];
            row[x * 3 + 1] = source[x * 4 + 1];
            row[x * 3 + 2] = source[x * 4];
        }
    }
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    return output.good();
}

void preferPowerSavingGpu() {
#if defined(_WIN32)
    // A laptop shares one power budget. Leaving the discrete GPU awake for a
    // 256 by 224 picture slows the CPU that is running the emulator. Windows
    // reads this when the graphics device is created.
    wchar_t path[32768];
    const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (length == 0 || length >= std::size(path)) {
        return;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\DirectX\\UserGpuPreferences",
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return;
    }
    const wchar_t preference[] = L"GpuPreference=1;";
    RegSetValueExW(
        key,
        path,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(preference),
        static_cast<DWORD>(sizeof(preference)));
    RegCloseKey(key);
#endif
}

void keepHostResponsive() {
#if defined(_WIN32)
    // Battery mode otherwise coalesces short sleeps and caps a light process.
    // A plugged-in plan already leaves the timer at 1 ms, which hides that.
    // This is the process, so NES and Super NES both keep the frame clock.
    PROCESS_POWER_THROTTLING_STATE processPower{};
    processPower.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    processPower.ControlMask =
        PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    processPower.StateMask = 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &processPower, sizeof(processPower));
    // A background priority on battery lets the shell and the emulator share a throttled core.
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    log::info("Battery power throttling is off so NES and Super NES frames stay on time.");
#endif
}

void keepWorkerResponsive() {
#if defined(_WIN32)
    THREAD_POWER_THROTTLING_STATE threadPower{};
    threadPower.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    threadPower.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    threadPower.StateMask = 0;
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &threadPower, sizeof(threadPower));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif
}

#if defined(_WIN32)
extern "C" {
__declspec(dllimport) unsigned int __stdcall timeBeginPeriod(unsigned int);
__declspec(dllimport) unsigned int __stdcall timeEndPeriod(unsigned int);
}
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// One high-resolution wait, then a short spin. Yielding the last millisecond
// is what the battery scheduler stretches into the next 15 ms tick. The game
// then runs slow and the audio device runs dry, which sounds like clipping.
class FrameClock {
public:
    FrameClock() {
        timeBeginPeriod(1);
        timer_ = CreateWaitableTimerExW(
            nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        highResolution_ = timer_ != nullptr;
        if (timer_ == nullptr) {
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        }
    }

    ~FrameClock() {
        if (timer_ != nullptr) {
            CloseHandle(timer_);
        }
        timeEndPeriod(1);
    }

    FrameClock(const FrameClock&) = delete;
    FrameClock& operator=(const FrameClock&) = delete;

    void waitUntil(std::chrono::steady_clock::time_point deadline) {
        using namespace std::chrono;
        auto now = steady_clock::now();
        if (now >= deadline) {
            return;
        }
        if (now + milliseconds(2) < deadline) {
            sleepOnce(duration_cast<nanoseconds>(deadline - milliseconds(1) - now));
        }
        // Sleep(1) covers an early timer. The last slice stays under a few
        // hundred microseconds so a waiting frame does not hold a core awake.
        while (steady_clock::now() + milliseconds(2) < deadline) {
            Sleep(1);
        }
        if (highResolution_) {
            const auto left = deadline - microseconds(200) - steady_clock::now();
            if (left > microseconds(200)) {
                sleepOnce(duration_cast<nanoseconds>(left));
            }
        }
        while (steady_clock::now() < deadline) {
#if defined(_M_IX86) || defined(_M_X64)
            _mm_pause();
#else
            std::this_thread::yield();
#endif
        }
    }

    void sleepOnce(std::chrono::nanoseconds duration) {
        if (duration.count() <= 0) {
            return;
        }
        // A coarse timer rounds a short wait up to the battery tick.
        if (!highResolution_ && duration < std::chrono::milliseconds(8)) {
            const auto end = std::chrono::steady_clock::now() + duration;
            while (std::chrono::steady_clock::now() < end) {
#if defined(_M_IX86) || defined(_M_X64)
                _mm_pause();
#else
                std::this_thread::yield();
#endif
            }
            return;
        }
        if (timer_ != nullptr) {
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(duration.count() / 100);
            if (due.QuadPart == 0) {
                due.QuadPart = -1;
            }
            if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE) != FALSE) {
                WaitForSingleObject(timer_, INFINITE);
                return;
            }
        }
        Sleep(static_cast<DWORD>((duration.count() + 999999) / 1000000));
    }

private:
    HANDLE timer_ = nullptr;
    bool highResolution_ = false;
};
#endif

ImU32 shellPacked(const ShellColor& color, int alpha = 255) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return IM_COL32(channel(color.red), channel(color.green), channel(color.blue), alpha);
}

void drawOriginalConsole(ImDrawList* draw, ImVec2 origin, bool superNes) {
    if (draw == nullptr) {
        return;
    }
    if (!superNes) {
        draw->AddRectFilled(origin, ImVec2(origin.x + 128.0f, origin.y + 40.0f), IM_COL32(88, 90, 96, 255), 4.0f);
        draw->AddRectFilled(ImVec2(origin.x + 14.0f, origin.y + 8.0f), ImVec2(origin.x + 78.0f, origin.y + 24.0f), IM_COL32(36, 38, 42, 255), 2.0f);
        draw->AddCircleFilled(ImVec2(origin.x + 96.0f, origin.y + 20.0f), 7.0f, IM_COL32(176, 64, 58, 255));
        draw->AddCircleFilled(ImVec2(origin.x + 114.0f, origin.y + 20.0f), 7.0f, IM_COL32(58, 92, 168, 255));
        return;
    }
    draw->AddRectFilled(origin, ImVec2(origin.x + 148.0f, origin.y + 36.0f), IM_COL32(72, 76, 84, 255), 8.0f);
    draw->AddRectFilled(ImVec2(origin.x + 36.0f, origin.y + 8.0f), ImVec2(origin.x + 96.0f, origin.y + 24.0f), IM_COL32(32, 34, 40, 255), 2.0f);
    draw->AddRectFilled(ImVec2(origin.x + 108.0f, origin.y + 12.0f), ImVec2(origin.x + 122.0f, origin.y + 26.0f), IM_COL32(168, 72, 78, 255), 3.0f);
    draw->AddRectFilled(ImVec2(origin.x + 128.0f, origin.y + 12.0f), ImVec2(origin.x + 142.0f, origin.y + 26.0f), IM_COL32(70, 110, 168, 255), 3.0f);
}

struct StartupPick {
    bool quit = false;
    SystemKind kind = SystemKind::Nes;
    std::string rom;
};

StartupPick pickSystem(
    SDL_Window* window, const ShellPalette& palette, const std::filesystem::path& nesDemo, const std::filesystem::path& snesDemo) {
    std::error_code demoError;
    const bool nesReady = !nesDemo.empty() && std::filesystem::is_regular_file(nesDemo, demoError);
    const bool snesReady = !snesDemo.empty() && std::filesystem::is_regular_file(snesDemo, demoError);
    StartupPick pick;
    bool chosen = false;
    while (!chosen) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                pick.quit = true;
                return pick;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                pick.quit = true;
                return pick;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin(
            "##startup",
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar(2);

        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        drawShadedBand(origin, ImVec2(origin.x + size.x, origin.y + size.y), palette.headerTop, palette.face);
        const float band = 84.0f;
        drawShadedBand(origin, ImVec2(origin.x + size.x, origin.y + band), palette.titleTop, palette.titleBottom);
        draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + band * 0.42f), IM_COL32(255, 255, 255, 36));
        draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 16.0f), shellPacked(palette.titleText), "Iris");
        draw->AddText(
            ImVec2(origin.x + 28.0f, origin.y + 44.0f), shellPacked(palette.titleText), "Choose a system, then play or open a game.");
        draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), shellPacked(palette.frame));

        const float gap = 20.0f;
        const float footer = 56.0f;
        const bool sideBySide = size.x >= 620.0f;
        const float cardWidth = sideBySide ? std::min(320.0f, std::max(220.0f, (size.x - 64.0f - gap) * 0.5f))
                                           : std::min(420.0f, std::max(240.0f, size.x - 64.0f));
        const float cardHeight = 286.0f;
        const float contentWidth = sideBySide ? cardWidth * 2.0f + gap : cardWidth;
        const float startX = std::max(16.0f, (size.x - contentWidth) * 0.5f);
        ImGui::SetCursorPos(ImVec2(0.0f, band + 16.0f));
        ImGui::BeginChild("##system-cards", ImVec2(size.x, std::max(1.0f, size.y - band - footer - 16.0f)), ImGuiChildFlags_None);

        const auto card = [&](const char* id, const char* title, const char* body, bool superNes, SystemKind cardKind,
                              const std::filesystem::path& demo, bool demoReady) {
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg, ImVec4(std::min(palette.face.red + 0.06f, 1.0f), std::min(palette.face.green + 0.06f, 1.0f),
                                         std::min(palette.face.blue + 0.05f, 1.0f), 1.0f));
            ImGui::BeginChild(id, ImVec2(cardWidth, cardHeight), ImGuiChildFlags_Borders);
            drawSectionHeader(title, palette);
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            const float iconWidth = superNes ? 148.0f : 128.0f;
            ImGui::SetCursorPosX(std::max(8.0f, (cardWidth - iconWidth) * 0.5f));
            drawOriginalConsole(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), superNes);
            ImGui::Dummy(ImVec2(1.0f, superNes ? 36.0f : 40.0f));
            ImGui::TextWrapped("%s", body);
            const float buttonWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            ImGui::SetCursorPosY(cardHeight - ImGui::GetFrameHeight() * 2.0f - 18.0f);
            if (ImGui::Button("Continue", ImVec2(buttonWidth, 0.0f))) {
                pick.kind = cardKind;
                pick.rom.clear();
                chosen = true;
            }
            ImGui::BeginDisabled(!demoReady);
            if (ImGui::Button("Play Iris Demo", ImVec2(buttonWidth, 0.0f))) {
                pick.kind = cardKind;
                pick.rom = demo.string();
                chosen = true;
            }
            ImGui::EndDisabled();
            if (!demoReady && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Iris Demo is placed beside the program when it is built.");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        };

        ImGui::SetCursorPos(ImVec2(startX, 12.0f));
        card(
            "##nes-card", "NES", "Cartridges, save states, and sound. Continue to open a game, or play the original demo.",
            false, SystemKind::Nes, nesDemo, nesReady);
        if (sideBySide) {
            ImGui::SameLine(0.0f, gap);
        } else {
            ImGui::SetCursorPosX(startX);
        }
        card(
            "##snes-card", "Super NES",
            "LoROM and HiROM. The demo is stripes and a marker. The arrow keys move it.",
            true, SystemKind::Snes, snesDemo, snesReady);
        ImGui::SetCursorPosX(startX);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::PushTextWrapPos(startX + contentWidth);
        ImGui::TextUnformatted("Super NES sound is the SPC700 and stereo DSP with Gaussian BRR. LoROM, HiROM, ExHiROM, DSP-1 through DSP-4, Super FX, and SA-1 are in this build. DMA, HDMA, and IRQs take cycles on the scanline. Timing inside an instruction is still approximate. Save states can be stored and restored for the loaded game. Place an optional 64-byte IPL at firmware/ipl.rom; Nintendo's dump is not shipped.");
        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        ImGui::SetCursorPos(ImVec2((size.x - 120.0f) * 0.5f, size.y - 42.0f));
        if (ImGui::Button("Quit", ImVec2(120.0f, 0.0f))) {
            pick.quit = true;
            chosen = true;
        }

        ImGui::End();
        ImGui::Render();
        glClearColor(palette.face.red, palette.face.green, palette.face.blue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    return pick;
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

DesktopShell::DesktopShell(const Config& config, SystemFactory factory, std::string initialRom)
    : config_(config), factory_(factory), initialRom_(std::move(initialRom)), windowWidth_(config.windowWidth),
      windowHeight_(config.windowHeight) {}

int DesktopShell::run() {
    hideSpawnedConsole();
    preferPowerSavingGpu();
    keepHostResponsive();
    SdlSession session;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log::error(std::string("SDL_Init failed: ") + SDL_GetError());
        return 1;
    }
    session.sdlInitialized = true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        log::warn(std::string("Audio is unavailable: ") + SDL_GetError());
    }
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        log::warn(std::string("Gamepads are unavailable: ") + SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#if defined(__APPLE__)
    // macOS has no OpenGL 3.3 Core context. 3.2 compatibility is the supported profile.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    const SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
                                  SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_BORDERLESS;
    session.window = SDL_CreateWindow(kProductName, windowWidth_, windowHeight_, flags);
    if (session.window == nullptr) {
        log::error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(session.window, kMinWindowWidth, kMinWindowHeight);
    WindowChrome chrome;
    if (!SDL_SetWindowHitTest(session.window, chromeHitTest, &chrome)) {
        log::warn(std::string("The window frame cannot be dragged: ") + SDL_GetError());
    }

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

    std::mutex audioLock;
    std::atomic<int> playbackChannels{1};
#if defined(_WIN32)
    WavePlayback waveform;
#endif
    const auto openPlayback = [&](const std::string& deviceName) {
        std::lock_guard playback(audioLock);
        if (session.audio != nullptr) {
            SDL_DestroyAudioStream(session.audio);
            session.audio = nullptr;
        }
        SDL_AudioSpec mono{};
        mono.format = SDL_AUDIO_F32;
        mono.channels = 1;
        mono.freq = 48000;
        const auto openDevice = [&](SDL_AudioDeviceID device) -> bool {
            session.audio = SDL_OpenAudioDeviceStream(device, &mono, nullptr, nullptr);
            if (session.audio != nullptr) {
                playbackChannels.store(1);
                return true;
            }
            const std::string monoError = SDL_GetError();
            SDL_AudioSpec stereo = mono;
            stereo.channels = 2;
            session.audio = SDL_OpenAudioDeviceStream(device, &stereo, nullptr, nullptr);
            if (session.audio != nullptr) {
                playbackChannels.store(2);
                log::info("Audio output is stereo because the device refused mono: " + monoError);
                return true;
            }
            session.audio = SDL_OpenAudioDeviceStream(device, nullptr, nullptr, nullptr);
            if (session.audio != nullptr && SDL_SetAudioStreamFormat(session.audio, &mono, nullptr)) {
                playbackChannels.store(1);
                log::info("Audio output is using the device's own format: " + monoError);
                return true;
            }
            if (session.audio != nullptr) {
                SDL_DestroyAudioStream(session.audio);
                session.audio = nullptr;
            }
            log::warn("Audio output was not opened: " + monoError);
            return false;
        };
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
        if (!openDevice(device) && device != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
            (void)openDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK);
        }
        if (session.audio == nullptr) {
#if defined(_WIN32)
            // Shared-mode WASAPI rejects these speakers. Leave that subsystem so the
            // waveform device can open the same endpoint without a stuck client.
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            if (waveform.start()) {
                log::info("WASAPI rejected the speakers. Audio output is using the Windows waveform device.");
                return;
            }
            log::warn("The Windows waveform device did not open.");
            SDL_InitSubSystem(SDL_INIT_AUDIO);
#endif
            return;
        }
#if defined(_WIN32)
        waveform.stop();
#endif
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
    // Arrow keys are the Super NES direction pad. Keyboard navigation would
    // move the library instead, and a focused text field can swallow them.
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    applyShellTheme(config_.theme, contentScale);

    if (!ImGui_ImplSDL3_InitForOpenGL(session.window, session.context)) {
        log::error("ImGui SDL3 backend failed to initialize.");
        return 1;
    }
#if defined(__APPLE__)
    const char* const glslVersion = "#version 150";
#else
    const char* const glslVersion = "#version 330 core";
#endif
    if (!ImGui_ImplOpenGL3_Init(glslVersion)) {
        ImGui_ImplSDL3_Shutdown();
        log::error("ImGui OpenGL 3 backend failed to initialize.");
        return 1;
    }
    session.imguiBackends = true;
    loadUiFont();

    ShellPalette palette = shellPalette(config_.theme);
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

    SystemKind kind = SystemKind::Nes;
    if (!initialRom_.empty()) {
        kind = systemKindForPath(std::filesystem::path(initialRom_));
    } else {
        const StartupPick pick = pickSystem(
            session.window, palette, std::filesystem::path(basePath) / "iris_demo.nes",
            std::filesystem::path(basePath) / "iris_demo.sfc");
        if (pick.quit) {
            return 0;
        }
        kind = pick.kind;
        if (!pick.rom.empty()) {
            initialRom_ = pick.rom;
        }
    }
    if (factory_ == nullptr) {
        log::error("No system is available.");
        return 1;
    }
    system_ = factory_(kind);
    if (system_ == nullptr) {
        log::error("The selected system could not be created.");
        return 1;
    }
    system_->setRegionChoice(config_.region);

    std::string romFilterName = kind == SystemKind::Snes ? "Super NES ROM" : "NES ROM";
    std::string romFilterPattern = kind == SystemKind::Snes ? "sfc;smc" : "nes";
    SDL_DialogFileFilter filters[2] = {
        {romFilterName.c_str(), romFilterPattern.c_str()},
        {"All files", "*"},
    };

    bool requestExit = false;
    bool openAbout = false;
    bool openGameInfo = false;
    std::string gameInfoText;
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
                                   "\n\nThis build plays NES cartridges on NROM, MMC1, UxROM, CNROM, MMC3, AxROM, Color Dreams, BNROM, GxROM, and Camerica. "
                                   "A NES 2.0 header can select PAL timing. "
                                   "Super NES plays a LoROM, HiROM, or ExHiROM board, including save RAM, four backgrounds, scroll, priority, mosaic, DMA, HDMA, windows, and color math. DMA, HDMA, and IRQs take their cycles on the scanline. Timing inside an instruction is still approximate. A cartridge can upload an SPC700 program, and the DSP mixes stereo at 48000 Hz with Gaussian BRR. DSP-1 through DSP-4, Super FX, and SA-1 boards run in this build. An optional user IPL may be placed at firmware/ipl.rom; Nintendo's dump is not shipped. Iris Demo moves a marker with the arrow keys and stays silent because it does not upload a program. Save states restore the running machine for the same game. "
                                   "Iris Demo, written for this project, is generated beside the executable.\n\n"
                                   "Open a ROM with File, Open Game, or from the library. "
                                   "The arrow keys move. Enter (or Numpad Enter) is Start — use that to leave title screens and menus. "
                                   "On Super NES, Z and A hold A and Y (run/confirm on many games), X is B, and S repeats B. "
                                   "On NES, Z is A, X is B, A repeats A, and S repeats B. Right Shift is Select. "
                                   "Escape pauses. F9 toggles fast forward. "
                                   "F5 saves the selected state slot and F7 loads it. F12 saves a screenshot. Ctrl+R resets. "
                                   "Options, Settings changes the theme, picture, audio, region, and keys. "
                                   "Shift+1 through Shift+0 select a slot, and Emulation, State Slots shows previews. "
                                   "Battery-backed RAM is stored separately from those slots. "
                                   "View sets the scale, 4:3 aspect, and presentation filter. Alt+Enter toggles fullscreen. "
                                   "Filters and color settings do not change the emulated picture.";
    auto previous = std::chrono::steady_clock::now();
    double hostFps = 0.0;
    double emulatedFps = 0.0;
    double lastEmulationMs = 0.0;
    double lastPresentMs = 0.0;
    int hostFrames = 0;
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
    int* capturePad = nullptr;
    bool openControllers = false;
    bool fastOn = false;
    ControllerSetup controllerDraft = config_.controllers;
    std::atomic<bool> stopWorker{false};
    std::atomic<bool> fastForward{false};
    std::atomic<bool> requestStep{false};
    std::atomic<std::uint8_t> livePort0{0};
    std::atomic<std::uint8_t> turboPort0{0};
    std::atomic<std::uint8_t> turboPort1{0};
    std::atomic<std::uint8_t> livePort1{0};
    std::atomic<int> pacedFrames{0};
    struct PublishedPicture {
        std::vector<std::uint8_t> rgba;
        std::vector<float> audio;
        int width = 0;
        int height = 0;
        std::uint64_t serial = 0;
    } published;
    std::vector<std::uint8_t> viewPicture;
    int viewWidth = 0;
    int viewHeight = 0;
    bool viewLoaded = false;
    std::string viewContentId;
    std::string loadedGamePath;
    bool viewPaused = false;
    SystemInfo viewInfo;
    std::uint64_t queuedSerial = 0;
    double shownEmulationMs = 0.0;
    // Key events as well as SDL_GetKeyboardState. Text input can clear the state array for arrows.
    std::array<bool, SDL_SCANCODE_COUNT> keyboardDown{};
    struct OpenPad {
        SDL_JoystickID id = 0;
        SDL_Gamepad* pad = nullptr;
        std::string name;
    };
    std::vector<OpenPad> pads;
    const auto closePads = [&]() {
        for (OpenPad& pad : pads) {
            if (pad.pad != nullptr) {
                SDL_CloseGamepad(pad.pad);
                pad.pad = nullptr;
            }
        }
        pads.clear();
    };
    const auto refreshPads = [&]() {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        std::vector<SDL_JoystickID> live;
        if (ids != nullptr) {
            live.assign(ids, ids + count);
            SDL_free(ids);
        }
        pads.erase(
            std::remove_if(
                pads.begin(),
                pads.end(),
                [&](OpenPad& pad) {
                    const bool present = std::find(live.begin(), live.end(), pad.id) != live.end();
                    if (!present && pad.pad != nullptr) {
                        SDL_CloseGamepad(pad.pad);
                        pad.pad = nullptr;
                    }
                    return !present;
                }),
            pads.end());
        for (const SDL_JoystickID id : live) {
            const bool known = std::any_of(pads.begin(), pads.end(), [&](const OpenPad& pad) { return pad.id == id; });
            if (known) {
                continue;
            }
            SDL_Gamepad* pad = SDL_OpenGamepad(id);
            if (pad == nullptr) {
                continue;
            }
            const char* name = SDL_GetGamepadName(pad);
            pads.push_back(OpenPad{id, pad, name == nullptr ? "Controller" : name});
        }
    };
    const auto padButtons = [&](const std::string& device, const GamepadMap& map) {
        if (device.empty()) {
            return PadSample{};
        }
        if (device == "keyboard") {
            return controllerButtons(config_.input, keyboardDown.data(), static_cast<int>(keyboardDown.size()));
        }
        for (const OpenPad& pad : pads) {
            if (pad.name != device || pad.pad == nullptr) {
                continue;
            }
            const auto down = [&](int button) {
                return button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT &&
                       SDL_GetGamepadButton(pad.pad, static_cast<SDL_GamepadButton>(button));
            };
            PadSample sample;
            if (down(map.a)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonA);
            if (down(map.b)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonB);
            if (down(map.select)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonSelect);
            if (down(map.start)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonStart);
            if (down(map.up)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonUp);
            if (down(map.down)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonDown);
            if (down(map.left)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonLeft);
            if (down(map.right)) sample.held = static_cast<std::uint8_t>(sample.held | kButtonRight);
            if (down(map.turboA)) sample.turbo = static_cast<std::uint8_t>(sample.turbo | kButtonA);
            if (down(map.turboB)) sample.turbo = static_cast<std::uint8_t>(sample.turbo | kButtonB);
            return sample;
        }
        return PadSample{};
    };
    const auto saveScreenshot = [&]() {
        if (viewPicture.empty() || viewWidth <= 0 || viewHeight <= 0 || !appPaths.has_value()) {
            return;
        }
        std::vector<std::uint8_t> presented;
        int width = 0;
        int height = 0;
        cropPicture(viewPicture, viewWidth, viewHeight, config_.overscan ? 8 : 0, presented, width, height);
        std::error_code failure;
        std::filesystem::create_directories(appPaths->screenshots, failure);
        const auto stamp = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(stamp);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        char name[64] = {};
        std::snprintf(
            name,
            sizeof(name),
            "iris-%04d%02d%02d-%02d%02d%02d.bmp",
            local.tm_year + 1900,
            local.tm_mon + 1,
            local.tm_mday,
            local.tm_hour,
            local.tm_min,
            local.tm_sec);
        const auto file = appPaths->screenshots / name;
        if (failure || !writeBmp(file, presented.data(), width, height)) {
            errorText = "The screenshot could not be written.";
            errorTitle = "Could not save the screenshot";
            openError = true;
            return;
        }
        libraryNote = "Saved " + file.filename().string();
    };
    std::recursive_mutex gate;
    std::thread emulation([&]() {
        keepWorkerResponsive();
#if defined(_WIN32)
        FrameClock clock;
#endif
        auto nextDue = std::chrono::steady_clock::now();
        bool turboOn = true;
        const auto drivePorts = [&]() {
            system_->setController(0, applyTurbo(livePort0.load(), turboPort0.load(), turboOn));
            system_->setController(1, applyTurbo(livePort1.load(), turboPort1.load(), turboOn));
            turboOn = !turboOn;
        };
        while (!stopWorker.load()) {
            std::chrono::steady_clock::time_point frameDeadline{};
            std::chrono::nanoseconds idleWait{0};
            bool waitForFrame = false;
            std::vector<float> chunk;
            bool clearPlayback = false;
            bool useWave = false;
            {
                int queuedSamples = -1;
                {
                    std::lock_guard playback(audioLock);
#if defined(_WIN32)
                    if (waveform.open()) {
                        useWave = true;
                        queuedSamples = waveform.queuedFrames();
                    } else
#endif
                    if (session.audio != nullptr) {
                        const int queuedBytes = SDL_GetAudioStreamQueued(session.audio);
                        const int channels = std::max(playbackChannels.load(), 1);
                        queuedSamples =
                            queuedBytes > 0 ? queuedBytes / (static_cast<int>(sizeof(float)) * channels) : 0;
                    }
                }
                std::unique_lock lock(gate, std::try_to_lock);
                if (!lock.owns_lock()) {
                    idleWait = std::chrono::milliseconds(1);
                } else {
                    const bool step = requestStep.exchange(false);
                    if (!system_->loaded() || (system_->paused() && !step)) {
                        nextDue = std::chrono::steady_clock::now();
                        idleWait = std::chrono::milliseconds(10);
                    } else if (fastForward.load()) {
                        clearPlayback = true;
                        drivePorts();
                        const auto started = std::chrono::steady_clock::now();
                        system_->runFrame();
                        shownEmulationMs = std::chrono::duration<double, std::milli>(
                                               std::chrono::steady_clock::now() - started)
                                               .count();
                        pacedFrames.fetch_add(1);
                        const FrameView frame = system_->frame();
                        if (frame.rgba != nullptr && frame.width > 0 && frame.height > 0) {
                            const std::size_t bytes = static_cast<std::size_t>(frame.width) *
                                                      static_cast<std::size_t>(frame.height) * 4;
                            published.rgba.assign(frame.rgba, frame.rgba + bytes);
                            published.width = frame.width;
                            published.height = frame.height;
                        }
                        ++published.serial;
                        if (step) {
                            system_->setPaused(true);
                        }
                        nextDue = std::chrono::steady_clock::now();
                    } else {
                        const AudioView previousAudio = system_->audio();
                        const bool systemHasAudio = previousAudio.samples != nullptr && previousAudio.count > 0;
                        const bool wantAudio = queuedSamples >= 0 && !config_.muted && config_.volume > 0.0f;
                        const bool followAudio = systemHasAudio && wantAudio;
                        const int paceTarget = audioPaceSamples(config_.audioLatency);
                        const auto now = std::chrono::steady_clock::now();
                        // Waiting out a full audio buffer is what made the picture miss
                        // NTSC speed. The frame clock stays in charge. Audio only pulls
                        // a frame forward when this system has produced samples and the
                        // device is about to starve. A silent system must not spin.
                        const bool refill = followAudio && queuedSamples < kAudioRefillSamples;
                        if (!refill && now < nextDue) {
                            frameDeadline = nextDue;
                            waitForFrame = true;
                        } else {
                            drivePorts();
                            const auto started = std::chrono::steady_clock::now();
                            system_->runFrame();
                            shownEmulationMs = std::chrono::duration<double, std::milli>(
                                                   std::chrono::steady_clock::now() - started)
                                                   .count();
                            pacedFrames.fetch_add(1);
                            const FrameView frame = system_->frame();
                            if (frame.rgba != nullptr && frame.width > 0 && frame.height > 0) {
                                const std::size_t bytes = static_cast<std::size_t>(frame.width) *
                                                          static_cast<std::size_t>(frame.height) * 4;
                                published.rgba.assign(frame.rgba, frame.rgba + bytes);
                                published.width = frame.width;
                                published.height = frame.height;
                            }
                            const AudioView audio = system_->audio();
                            if (wantAudio && audio.samples != nullptr && audio.count > 0 && queuedSamples <= paceTarget &&
                                queuedSamples + audio.count < audioQueueSamples(config_.audioLatency)) {
                                const float volume = std::min(config_.volume, 1.0f);
                                const int channels = useWave ? 1 : playbackChannels.load();
                                const int sourceChannels = audio.channels > 0 ? audio.channels : 1;
                                chunk.resize(static_cast<std::size_t>(audio.count) * static_cast<std::size_t>(channels));
                                for (int index = 0; index < audio.count; ++index) {
                                    for (int channel = 0; channel < channels; ++channel) {
                                        const int source = channel < sourceChannels ? channel : sourceChannels - 1;
                                        chunk[static_cast<std::size_t>(index * channels + channel)] =
                                            audio.samples[index * sourceChannels + source] * volume;
                                    }
                                }
                            }
                            ++published.serial;
                            const auto period = std::chrono::nanoseconds(system_->framePeriodNs());
                            if (step) {
                                system_->setPaused(true);
                            }
                            if (now < nextDue) {
                                nextDue = now + period;
                            } else {
                                nextDue += period;
                                if (nextDue < now) {
                                    nextDue = now + period;
                                }
                            }
                        }
                    }
                }
            }
            if (!chunk.empty() || clearPlayback) {
                std::lock_guard playback(audioLock);
#if defined(_WIN32)
                if (waveform.open()) {
                    if (clearPlayback) {
                        waveform.clear();
                    } else {
                        waveform.write(chunk.data(), static_cast<int>(chunk.size()), 1.0f);
                    }
                } else
#endif
                if (session.audio != nullptr) {
                    if (clearPlayback) {
                        SDL_ClearAudioStream(session.audio);
                    } else {
                        SDL_PutAudioStreamData(
                            session.audio, chunk.data(), static_cast<int>(chunk.size() * sizeof(float)));
                    }
                }
            }
            if (waitForFrame) {
#if defined(_WIN32)
                clock.waitUntil(frameDeadline);
#else
                const auto coarse = frameDeadline - std::chrono::milliseconds(1);
                const auto now = std::chrono::steady_clock::now();
                if (now < coarse) {
                    std::this_thread::sleep_for(coarse - now);
                }
                while (std::chrono::steady_clock::now() < frameDeadline) {
                    std::this_thread::yield();
                }
#endif
            } else if (idleWait.count() > 0) {
#if defined(_WIN32)
                clock.sleepOnce(idleWait);
#else
                std::this_thread::sleep_for(idleWait);
#endif
            }
        }
    });
    std::string removePath;
    std::vector<std::string> playbackNames;
    bool wasMuted = config_.muted;
    bool focusPicture = false;
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
        std::lock_guard lock(gate);
        if (!appPaths.has_value() || system_->contentId().size() != 64) {
            return {};
        }
        char name[32] = {};
        std::snprintf(name, sizeof(name), "slot-%02d.irstate", slot + 1);
        return appPaths->states / system_->contentId() / name;
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
        std::lock_guard lock(gate);
        if (!appPaths.has_value()) {
            return;
        }
        std::vector<std::uint8_t> bytes;
        if (!system_->copyBattery(bytes)) {
            return;
        }
        std::error_code failure;
        std::filesystem::create_directories(appPaths->saves, failure);
        const auto file = appPaths->saves / (system_->contentId() + ".sav");
        if (failure || !writeBinaryFileAtomically(file, bytes.data(), bytes.size())) {
            log::error("Could not write the battery save.");
        }
    };

    const auto importBattery = [&]() {
        std::lock_guard lock(gate);
        if (!appPaths.has_value() || !system_->info().batteryBacked) {
            return;
        }
        const auto file = appPaths->saves / (system_->contentId() + ".sav");
        std::error_code failure;
        if (!std::filesystem::is_regular_file(file, failure)) {
            return;
        }
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!readFile(file, 2 * 1024 * 1024, bytes, error) || !system_->restoreBattery(bytes.data(), bytes.size(), error)) {
            errorText = error.empty() ? "The battery save does not match this cartridge and was not loaded." : error;
            errorTitle = "Could not load the battery save";
            openError = true;
            log::error(errorText);
        }
    };

    const auto saveSelectedSlot = [&]() {
        std::lock_guard lock(gate);
        if (!system_->loaded()) {
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
        if (!system_->saveState(bytes, description, error)) {
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
        std::lock_guard lock(gate);
        if (!system_->loaded()) {
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
        if (!readFile(file, 4 * 1024 * 1024, bytes, error) || !system_->loadState(bytes.data(), bytes.size(), error)) {
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

    const auto adoptKind = [&](SystemKind next) -> bool {
        std::lock_guard lock(gate);
        if (next == kind) {
            return true;
        }
        flushBattery();
        auto created = factory_(next);
        if (created == nullptr) {
            errorText = "The selected system could not be created.";
            errorTitle = "Could not switch system";
            openError = true;
            return false;
        }
        system_ = std::move(created);
        system_->setRegionChoice(config_.region);
        kind = next;
        romFilterName = kind == SystemKind::Snes ? "Super NES ROM" : "NES ROM";
        romFilterPattern = kind == SystemKind::Snes ? "sfc;smc" : "nes";
        filters[0].name = romFilterName.c_str();
        filters[0].pattern = romFilterPattern.c_str();
        published.rgba.clear();
        published.width = 0;
        published.height = 0;
        ++published.serial;
        loadedGamePath.clear();
        libraryNote = kind == SystemKind::Snes ? "Switched to Super NES." : "Switched to NES.";
        return true;
    };

    const auto openRom = [&](const std::filesystem::path& path) -> bool {
        std::string extension = path.extension().string();
        for (char& character : extension) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if ((extension == ".sfc" || extension == ".smc") && kind != SystemKind::Snes) {
            if (!adoptKind(SystemKind::Snes)) {
                return false;
            }
        } else if (extension == ".nes" && kind != SystemKind::Nes) {
            if (!adoptKind(SystemKind::Nes)) {
                return false;
            }
        }
        std::lock_guard lock(gate);
        flushBattery();
        std::string error;
        if (!system_->loadContent(path, error)) {
            errorText = error;
            errorTitle = "Could not open game";
            openError = true;
            log::error(error);
            return false;
        }
        const SystemInfo info = system_->info();
        loadedGamePath = pathToUtf8(path);
        log::info(std::string("Loaded ") + info.title + " (" + info.detail + ").");
        if (info.batteryBacked) {
            log::warn("Battery-backed RAM is stored separately from save states.");
        }
        importBattery();
        LibraryEntry entry;
        entry.path = pathToUtf8(path);
        entry.title = info.title;
        entry.detail = info.detail;
        entry.contentId = system_->contentId();
        entry.addedUnixMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
        entry.lastPlayedUnixMs = entry.addedUnixMs;
        std::string upsertError;
        if (!LibraryService::upsert(library, std::move(entry), upsertError)) {
            errorText = upsertError;
            errorTitle = "Could not update the library";
            openError = true;
            return true;
        }
        saveLibrary();
        return true;
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
            onOpenFile, &dialog, session.window, filters, 2, basePath.empty() ? nullptr : basePath.c_str(), false);
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
        const RomScan found = kind == SystemKind::Snes ? collectRomFiles(folder, {".sfc", ".smc"}) : collectNesFiles(folder);
        for (const std::filesystem::path& file : found.files) {
            ContentDescription description;
            std::string describeError;
            if (!system_->describeContent(file, description, describeError)) {
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

#if defined(_WIN32)
    FrameClock hostClock;
#endif
    while (!requestExit) {
        const auto hostFrameStart = std::chrono::steady_clock::now();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                const int code = static_cast<int>(event.key.scancode);
                if (code > 0 && code < SDL_SCANCODE_COUNT) {
                    keyboardDown[static_cast<std::size_t>(code)] = event.type == SDL_EVENT_KEY_DOWN;
                }
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST &&
                event.window.windowID == SDL_GetWindowID(session.window)) {
                keyboardDown.fill(false);
            }
            if (event.type == SDL_EVENT_QUIT) {
                requestExit = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(session.window)) {
                requestExit = true;
            }
            if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED && !event.adevice.recording) {
                bool disconnected = false;
                {
                    std::lock_guard playback(audioLock);
                    disconnected = session.audio != nullptr &&
                                   event.adevice.which == SDL_GetAudioStreamDevice(session.audio);
                }
                if (disconnected) {
                    log::info("The audio device was disconnected. Switching to the default playback device.");
                    const std::string gone = config_.audioDevice;
                    config_.audioDevice.clear();
                    if (settingsDraft.audioDevice == gone) {
                        settingsDraft.audioDevice.clear();
                    }
                    openPlayback("");
                }
            }
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && capturePad != nullptr) {
                *capturePad = static_cast<int>(event.gbutton.button);
                capturePad = nullptr;
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
                    } else if (ctrl && !alt && code == config_.input.reset && system_->loaded()) {
                        std::lock_guard lock(gate);
                        system_->reset();
                    } else if (alt && !ctrl && code == config_.input.fullscreen) {
                        setFullscreen(!config_.fullscreen);
                    } else if (!ctrl && !alt && !shift && code == config_.input.pause && system_->loaded()) {
                        std::lock_guard lock(gate);
                        system_->setPaused(!system_->paused());
                    } else if (!ctrl && !alt && !shift && code == config_.input.quickSave && system_->loaded()) {
                        saveSelectedSlot();
                    } else if (!ctrl && !alt && !shift && code == config_.input.quickLoad && system_->loaded()) {
                        loadSelectedSlot();
                    } else if (!ctrl && !alt && !shift && code == config_.input.fastForward && system_->loaded()) {
                        fastOn = !fastOn;
                        fastForward.store(fastOn);
                    } else if (!ctrl && !alt && !shift && code == config_.input.screenshot && system_->loaded()) {
                        saveScreenshot();
                    } else if (shift && !ctrl && !alt && system_->loaded()) {
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
            DialogKind dialogKind = DialogKind::OpenRom;
            {
                std::lock_guard lock(dialog.mutex);
                if (dialog.ready) {
                    chosen = dialog.path;
                    failed = dialog.failed;
                    failure = dialog.error;
                    dialogKind = dialog.kind;
                    dialog.ready = false;
                    dialog.path.clear();
                }
            }
            if (failed) {
                errorText = failure;
                errorTitle = dialogKind == DialogKind::ScanFolder ? "Could not scan the folder" : "Could not open game";
                openError = true;
                log::error(failure);
            } else if (!chosen.empty()) {
                if (dialogKind == DialogKind::ScanFolder) {
                    scanFolder(pathFromUtf8(chosen));
                } else {
                    openRom(pathFromUtf8(chosen));
                }
            }
        }

        refreshPads();
        const bool uiOwnsInput = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
        if (uiOwnsInput) {
            livePort0.store(0);
            livePort1.store(0);
            turboPort0.store(0);
            turboPort1.store(0);
        } else {
            PadSample port0 = padButtons(config_.controllers.port1, config_.controllers.pad1);
            PadSample port1 = padButtons(config_.controllers.port2, config_.controllers.pad2);
            if (kind == SystemKind::Snes) {
                // Turbo A is the A key, and it only pulses. A roll needs Y held.
                const auto holdRun = [](PadSample& sample) {
                    sample.held = static_cast<std::uint8_t>(sample.held | (sample.turbo & kButtonA));
                    sample.turbo = static_cast<std::uint8_t>(sample.turbo & static_cast<std::uint8_t>(~kButtonA));
                };
                holdRun(port0);
                holdRun(port1);
            }
            livePort0.store(port0.held);
            livePort1.store(port1.held);
            turboPort0.store(port0.turbo);
            turboPort1.store(port1.turbo);
        }
        {
            std::lock_guard lock(gate);
            viewLoaded = system_->loaded();
            viewPaused = system_->paused();
            viewInfo = viewLoaded ? system_->info() : SystemInfo{};
            viewContentId = viewLoaded ? system_->contentId() : std::string{};
            lastEmulationMs = shownEmulationMs;
            if (published.serial != queuedSerial) {
                viewPicture = published.rgba;
                viewWidth = published.width;
                viewHeight = published.height;
                queuedSerial = published.serial;
            }
        }
        if (config_.muted && !wasMuted) {
            std::lock_guard playback(audioLock);
#if defined(_WIN32)
            if (waveform.open()) {
                waveform.clear();
            }
#endif
            if (session.audio != nullptr) {
                SDL_ClearAudioStream(session.audio);
            }
        }
        wasMuted = config_.muted;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const SDL_WindowFlags frameFlags = SDL_GetWindowFlags(session.window);
        const bool fullscreen = (frameFlags & SDL_WINDOW_FULLSCREEN) != 0;
        const bool maximized = (frameFlags & SDL_WINDOW_MAXIMIZED) != 0;
        const float uiScale = contentScale < 1.0f ? 1.0f : contentScale;
        const float frameBorder = fullscreen ? 0.0f : 4.0f * uiScale;
        const float titleHeight = fullscreen ? 0.0f : std::max(26.0f, 28.0f * uiScale);
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                 ImGuiWindowFlags_NoSavedSettings |
                                                 ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::PushStyleColor(
            ImGuiCol_WindowBg, ImVec4(palette.frame.red, palette.frame.green, palette.frame.blue, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::Begin("##client", nullptr, windowFlags)) {
            const auto chord = [](const char* prefix, int code) {
                return prefix == nullptr ? scancodeLabel(code) : std::string(prefix) + scancodeLabel(code);
            };
            const std::string openShortcut = chord("Ctrl+", config_.input.openGame);
            const std::string resetShortcut = chord("Ctrl+", config_.input.reset);
            const std::string saveShortcut = scancodeLabel(config_.input.quickSave);
            const std::string loadShortcut = scancodeLabel(config_.input.quickLoad);
            const std::string pauseShortcut = scancodeLabel(config_.input.pause);
            const std::string forwardShortcut = scancodeLabel(config_.input.fastForward);
            const std::string screenshotShortcut = scancodeLabel(config_.input.screenshot);
            const std::string fullscreenShortcut = chord("Alt+", config_.input.fullscreen);
            chrome.width = static_cast<int>(std::lround(viewport->Size.x));
            chrome.height = static_cast<int>(std::lround(viewport->Size.y));
            chrome.border = static_cast<int>(std::lround(frameBorder));
            chrome.titleBottom = static_cast<int>(std::lround(frameBorder + titleHeight));
            chrome.grab = static_cast<int>(std::lround(std::max(8.0f, 8.0f * uiScale)));
            chrome.resizable = !fullscreen && !maximized;
            chrome.buttonCount = 0;
            const int cornerRadius =
                fullscreen || maximized ? 0 : static_cast<int>(std::lround(std::max(8.0f, 8.0f * uiScale)));
            updateTopCornerShape(session.window, cornerRadius);
            if (!fullscreen) {
                const ImVec2 titleMin(viewport->Pos.x + frameBorder, viewport->Pos.y + frameBorder);
                const ImVec2 titleMax(viewport->Pos.x + viewport->Size.x - frameBorder, titleMin.y + titleHeight);
                const TitleBar titleBar =
                    drawTitleBar(titleMin, titleMax, kProductName, maximized, palette, frameBorder);
                chrome.buttonCount = 3;
                for (int index = 0; index < 3; ++index) {
                    chrome.buttons[index].x =
                        static_cast<int>(std::lround(titleBar.buttons[index].x - viewport->Pos.x));
                    chrome.buttons[index].y =
                        static_cast<int>(std::lround(titleBar.buttons[index].y - viewport->Pos.y));
                    chrome.buttons[index].w = static_cast<int>(std::lround(titleBar.buttons[index].width));
                    chrome.buttons[index].h = static_cast<int>(std::lround(titleBar.buttons[index].height));
                }
                if (titleBar.command == CaptionCommand::Minimize) {
                    SDL_MinimizeWindow(session.window);
                } else if (titleBar.command == CaptionCommand::Maximize) {
                    SDL_MaximizeWindow(session.window);
                } else if (titleBar.command == CaptionCommand::Restore) {
                    SDL_RestoreWindow(session.window);
                } else if (titleBar.command == CaptionCommand::Close) {
                    requestExit = true;
                }
                ImGui::GetWindowDrawList()->AddRect(
                    ImVec2(viewport->Pos.x + frameBorder, viewport->Pos.y + frameBorder),
                    ImVec2(
                        viewport->Pos.x + viewport->Size.x - frameBorder,
                        viewport->Pos.y + viewport->Size.y - frameBorder),
                    IM_COL32(255, 255, 255, 70),
                    0.0f,
                    0,
                    1.0f);
            }
            ImGui::SetCursorPos(ImVec2(frameBorder, frameBorder + titleHeight));
            const ImVec2 bodySize(
                std::max(1.0f, viewport->Size.x - frameBorder * 2.0f),
                std::max(1.0f, viewport->Size.y - frameBorder * 2.0f - titleHeight));
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg, ImVec4(palette.face.red, palette.face.green, palette.face.blue, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::BeginChild(
                "##body",
                bodySize,
                ImGuiChildFlags_None,
                ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();
            // The client window padding is zero. A menu popup copies the style
            // when it opens, so the inset and the row gap have to be set here.
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * uiScale, 6.0f * uiScale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * uiScale, 6.0f * uiScale));
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::BeginMenu("System")) {
                        if (ImGui::MenuItem("NES", nullptr, kind == SystemKind::Nes) && kind != SystemKind::Nes) {
                            (void)adoptKind(SystemKind::Nes);
                        }
                        if (ImGui::MenuItem("Super NES", nullptr, kind == SystemKind::Snes) &&
                            kind != SystemKind::Snes) {
                            (void)adoptKind(SystemKind::Snes);
                        }
                        ImGui::EndMenu();
                    }
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
                    if (ImGui::MenuItem("Close Game", nullptr, false, viewLoaded)) {
                        flushBattery();
                        std::lock_guard lock(gate);
                        system_->unload();
                        viewLoaded = false;
                        loadedGamePath.clear();
                        viewContentId.clear();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Exit")) {
                        requestExit = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Emulation")) {
                    const char* pauseLabel = viewPaused ? "Resume" : "Pause";
                    if (ImGui::MenuItem(pauseLabel, pauseShortcut.c_str(), false, viewLoaded)) {
                        std::lock_guard lock(gate);
                        system_->setPaused(!system_->paused());
                        viewPaused = system_->paused();
                    }
                    if (ImGui::MenuItem("Reset", resetShortcut.c_str(), false, viewLoaded)) {
                        std::lock_guard lock(gate);
                        system_->reset();
                    }
                    if (ImGui::MenuItem("Power Cycle", nullptr, false, viewLoaded)) {
                        std::lock_guard lock(gate);
                        system_->powerCycle();
                    }
                    if (ImGui::BeginMenu("Region")) {
                        const auto chooseRegion = [&](RegionChoice choice) {
                            config_.region = choice;
                            std::lock_guard lock(gate);
                            system_->setRegionChoice(choice);
                        };
                        if (ImGui::MenuItem("Automatic", nullptr, config_.region == RegionChoice::Automatic)) {
                            chooseRegion(RegionChoice::Automatic);
                        }
                        if (ImGui::MenuItem("NTSC", nullptr, config_.region == RegionChoice::Ntsc)) {
                            chooseRegion(RegionChoice::Ntsc);
                        }
                        if (ImGui::MenuItem("PAL", nullptr, config_.region == RegionChoice::Pal)) {
                            chooseRegion(RegionChoice::Pal);
                        }
                        if (ImGui::MenuItem("Dendy", nullptr, config_.region == RegionChoice::Dendy)) {
                            chooseRegion(RegionChoice::Dendy);
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Frame Step", nullptr, false, viewLoaded)) {
                        std::lock_guard lock(gate);
                        system_->setPaused(true);
                        requestStep.store(true);
                        viewPaused = true;
                    }
                    if (ImGui::MenuItem("Fast Forward", forwardShortcut.c_str(), fastOn, viewLoaded)) {
                        fastOn = !fastOn;
                        fastForward.store(fastOn);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save State", saveShortcut.c_str(), false, viewLoaded)) {
                        saveSelectedSlot();
                    }
                    if (ImGui::MenuItem("Load State", loadShortcut.c_str(), false, viewLoaded)) {
                        loadSelectedSlot();
                    }
                    if (ImGui::MenuItem("State Slots...", nullptr, false, viewLoaded)) {
                        openStateSlots = true;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Game Information", nullptr, false, viewLoaded)) {
                        openGameInfo = true;
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
                    ImGui::MenuItem("Overscan", nullptr, &config_.overscan);
                    if (ImGui::MenuItem("Screenshot", screenshotShortcut.c_str(), false, viewLoaded && !viewPicture.empty())) {
                        saveScreenshot();
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
                    if (ImGui::MenuItem("Controllers...")) {
                        openControllers = true;
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
            ImGui::PopStyleVar(2);

            const float toolbarHeight = ImGui::GetFrameHeight() + 10.0f;
            const ImVec2 bandMin = ImGui::GetCursorScreenPos();
            const float bandWidth = ImGui::GetContentRegionAvail().x;
            const ImVec2 bandMax(bandMin.x + bandWidth, bandMin.y + toolbarHeight);
            drawShadedBand(bandMin, bandMax, palette.toolbarTop, palette.toolbarBottom);
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(bandMin.x, bandMax.y - 1.0f),
                ImVec2(bandMax.x, bandMax.y - 1.0f),
                IM_COL32(140, 136, 120, 255));
            ImGui::SetCursorScreenPos(ImVec2(bandMin.x + 8.0f, bandMin.y + 4.0f));
            if (ImGui::Button("Open")) {
                showOpenDialog();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!viewLoaded);
            if (ImGui::Button(viewPaused ? "Resume" : "Pause")) {
                std::lock_guard lock(gate);
                system_->setPaused(!system_->paused());
                viewPaused = system_->paused();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                std::lock_guard lock(gate);
                system_->reset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                saveSelectedSlot();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                loadSelectedSlot();
            }
            ImGui::EndDisabled();
            ImGui::SetCursorScreenPos(ImVec2(bandMin.x, bandMax.y));

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
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::BeginChild("##library", ImVec2(220.0f, viewportHeight), ImGuiChildFlags_Borders);
                ImGui::PopStyleVar();
                drawSectionHeader("Library", palette);
                ImGui::Indent(6.0f);
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
                    const float line = ImGui::GetTextLineHeight();
                    const bool hasDetail = !game.detail.empty();
                    const float rowHeight = hasDetail ? line * 2.0f + 6.0f : line + 4.0f;
                    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                    if (ImGui::Selectable(
                            "##game",
                            selectedGame == game.path,
                            ImGuiSelectableFlags_AllowDoubleClick,
                            ImVec2(0.0f, rowHeight))) {
                        selectedGame = game.path;
                        if (exists && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                            openRom(pathFromUtf8(game.path))) {
                            focusPicture = true;
                        }
                    }
                    ImDrawList* listDraw = ImGui::GetWindowDrawList();
                    const ImVec2 textPos(rowPos.x + 4.0f, rowPos.y + 2.0f);
                    listDraw->AddText(
                        textPos, ImGui::GetColorU32(exists ? ImGuiCol_Text : ImGuiCol_TextDisabled), label.c_str());
                    if (hasDetail) {
                        listDraw->AddText(
                            ImVec2(textPos.x, textPos.y + line + 2.0f),
                            ImGui::GetColorU32(ImGuiCol_TextDisabled),
                            game.detail.c_str());
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                std::error_code selectedError;
                const bool canPlay =
                    !selectedGame.empty() && std::filesystem::is_regular_file(pathFromUtf8(selectedGame), selectedError);
                ImGui::BeginDisabled(!canPlay);
                if (ImGui::Button("Play")) {
                    if (openRom(pathFromUtf8(selectedGame))) {
                        focusPicture = true;
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(selectedGame.empty());
                if (ImGui::Button("Remove")) {
                    removePath = selectedGame;
                    openRemove = true;
                }
                ImGui::EndDisabled();
                ImGui::Unindent(6.0f);
                ImGui::EndChild();
                ImGui::SameLine();
            }
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            if (focusPicture) {
                ImGui::SetNextWindowFocus();
                focusPicture = false;
            }
            ImGui::BeginChild(
                "##viewport",
                ImVec2(0.0f, viewportHeight),
                ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            std::vector<std::uint8_t> presented;
            int presentWidth = 0;
            int presentHeight = 0;
            if (!viewPicture.empty()) {
                cropPicture(viewPicture, viewWidth, viewHeight, config_.overscan ? 8 : 0, presented, presentWidth, presentHeight);
            }
            if (viewLoaded && !presented.empty() && presentWidth > 0 && presentHeight > 0) {
                const FrameView frame{presented.data(), presentWidth, presentHeight};
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
            if (viewLoaded) {
                status = viewInfo.title + " - " + viewInfo.detail;
                if (viewPaused) {
                    status += " - Paused";
                }
                if (fastOn) {
                    status += " - Fast";
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
                ImVec4(palette.face.red, palette.face.green, palette.face.blue, 1.0f));
            const ImU32 text = IM_COL32(0, 0, 0, 255);
            draw->PushClipRect(
                ImVec2(windowPos.x, statusTop),
                ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                false);
            draw->AddRectFilled(
                ImVec2(windowPos.x, statusTop), ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), face);
            draw->AddLine(
                ImVec2(windowPos.x, statusTop), ImVec2(windowPos.x + windowSize.x, statusTop), IM_COL32(255, 255, 255, 180));
            draw->AddLine(
                ImVec2(windowPos.x, statusTop + 1.0f),
                ImVec2(windowPos.x + windowSize.x, statusTop + 1.0f),
                IM_COL32(140, 136, 120, 255));
            draw->AddText(ImVec2(windowPos.x + 8.0f, statusTop + 4.0f), text, status.c_str());
            const ImVec2 versionSize = ImGui::CalcTextSize(versionLabel.c_str());
            draw->AddText(
                ImVec2(windowPos.x + windowSize.x - versionSize.x - 8.0f, statusTop + 4.0f),
                text,
                versionLabel.c_str());
            draw->PopClipRect();

            if (openControllers) {
                controllerDraft = config_.controllers;
                capturePad = nullptr;
                openControllers = false;
                ImGui::OpenPopup("Controllers");
            }
            if (beginClassicPopup("Controllers", palette, ImGuiWindowFlags_AlwaysAutoResize)) {
                const auto portRow = [&](const char* label, std::string& device, GamepadMap& map) {
                    ImGui::SeparatorText(label);
                    const char* current = device.empty() ? "None" : device.c_str();
                    if (ImGui::BeginCombo((std::string("Device##") + label).c_str(), current)) {
                        if (ImGui::Selectable("None", device.empty())) {
                            device.clear();
                        }
                        if (ImGui::Selectable("Keyboard", device == "keyboard")) {
                            device = "keyboard";
                        }
                        for (const OpenPad& pad : pads) {
                            ImGui::PushID(static_cast<int>(pad.id));
                            if (ImGui::Selectable(pad.name.c_str(), device == pad.name)) {
                                device = pad.name;
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if (device == "keyboard") {
                        ImGui::TextUnformatted("Keyboard uses the keys in Settings.");
                    } else if (!device.empty()) {
                        const auto bind = [&](const char* name, int& button) {
                            const std::string caption =
                                std::string(name) + ": " + (capturePad == &button ? "..." : gamepadButtonName(button));
                            if (ImGui::Button(caption.c_str())) {
                                capturePad = &button;
                            }
                        };
                        bind("A", map.a);
                        ImGui::SameLine();
                        bind("B", map.b);
                        ImGui::SameLine();
                        bind("Turbo A", map.turboA);
                        ImGui::SameLine();
                        bind("Turbo B", map.turboB);
                        ImGui::SameLine();
                        bind("Select", map.select);
                        ImGui::SameLine();
                        bind("Start", map.start);
                        bind("Up", map.up);
                        ImGui::SameLine();
                        bind("Down", map.down);
                        ImGui::SameLine();
                        bind("Left", map.left);
                        ImGui::SameLine();
                        bind("Right", map.right);
                        ImGui::TextUnformatted("Choose a button, then press it on the controller.");
                    }
                };
                portRow("Port 1", controllerDraft.port1, controllerDraft.pad1);
                portRow("Port 2", controllerDraft.port2, controllerDraft.pad2);
                if (ImGui::Button("OK", ImVec2(75.0f, 0.0f))) {
                    config_.controllers = controllerDraft;
                    capturePad = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(75.0f, 0.0f))) {
                    capturePad = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

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
            ImGui::SetNextWindowSize(ImVec2(560.0f, 560.0f), ImGuiCond_Appearing);
            if (beginClassicPopup("Settings", palette)) {
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
                    const RegionChoice previousRegion = config_.region;
                    const int keptWidth = config_.windowWidth;
                    const int keptHeight = config_.windowHeight;
                    settingsDraft.controllers = config_.controllers;
                    config_ = settingsDraft;
                    config_.windowWidth = keptWidth;
                    config_.windowHeight = keptHeight;
                    settingsDraft.windowWidth = keptWidth;
                    settingsDraft.windowHeight = keptHeight;
                    if (themeChanged) {
                        applyShellTheme(config_.theme, contentScale);
                        palette = shellPalette(config_.theme);
                    }
                    if (logChanged) {
                        log::setLevel(config_.logLevel);
                    }
                    if (config_.region != previousRegion) {
                        std::lock_guard lock(gate);
                        system_->setRegionChoice(config_.region);
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
                    if (config_.muted) {
                        std::lock_guard playback(audioLock);
#if defined(_WIN32)
                        if (waveform.open()) {
                            waveform.clear();
                        }
#endif
                        if (session.audio != nullptr) {
                            SDL_ClearAudioStream(session.audio);
                        }
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
                        if (ImGui::Combo("Theme", &theme, "Blue\0Olive\0Silver\0")) {
                            settingsDraft.theme = static_cast<Theme>(theme);
                            applyShellTheme(settingsDraft.theme, contentScale);
                            palette = shellPalette(settingsDraft.theme);
                        }
                        int level = static_cast<int>(settingsDraft.logLevel);
                        if (ImGui::Combo("Log level", &level, "Trace\0Debug\0Info\0Warn\0Error\0Critical\0")) {
                            settingsDraft.logLevel = static_cast<LogLevel>(level);
                        }
                        ImGui::Checkbox("Show the game library", &settingsDraft.showLibrary);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Video")) {
                        const auto tip = [](const char* text) { ImGui::SetItemTooltip("%s", text); };
                        int scale = static_cast<int>(settingsDraft.scaleMode);
                        if (ImGui::Combo("Scale", &scale, "Fit\0Integer\0")) {
                            settingsDraft.scaleMode = static_cast<ScaleMode>(scale);
                        }
                        tip("Fit uses the window. Integer keeps whole pixels at the scale below.");
                        ImGui::SliderInt(
                            "Integer scale", &settingsDraft.integerScale, kMinimumIntegerScale, kMaximumIntegerScale);
                        tip("Used when Scale is Integer. The range is 1 to 6.");
                        int aspect = static_cast<int>(settingsDraft.aspect);
                        if (ImGui::Combo("Aspect", &aspect, "Pixel\0Television 4:3\0")) {
                            settingsDraft.aspect = static_cast<AspectMode>(aspect);
                        }
                        tip("Pixel keeps the PPU aspect. Television 4:3 is only a presentation stretch.");
                        int filter = static_cast<int>(settingsDraft.filter);
                        if (ImGui::Combo("Filter", &filter, "Original\0Smooth\0Sharp\0CRT\0")) {
                            settingsDraft.filter = static_cast<PictureFilter>(filter);
                        }
                        tip("Samples the finished picture. The emulated image stays as the PPU drew it.");
                        ImGui::SliderFloat("Brightness", &settingsDraft.brightness, kBrightness.minimum, kBrightness.maximum, "%.2f");
                        tip("Shifts the shown colors. It does not change the PPU palette.");
                        ImGui::SliderFloat("Contrast", &settingsDraft.contrast, kContrast.minimum, kContrast.maximum, "%.2f");
                        tip("Scales the shown contrast. It does not change the PPU palette.");
                        ImGui::SliderFloat(
                            "Saturation", &settingsDraft.saturation, kSaturation.minimum, kSaturation.maximum, "%.2f");
                        tip("Scales the shown color. Zero is grayscale.");
                        ImGui::SliderFloat("Gamma", &settingsDraft.gamma, kGamma.minimum, kGamma.maximum, "%.2f");
                        tip("Adjusts the shown midtones.");
                        if (settingsDraft.filter == PictureFilter::Sharp) {
                            ImGui::SliderFloat(
                                "Sharpness", &settingsDraft.sharpness, kSharpness.minimum, kSharpness.maximum, "%.2f");
                            tip("Used by the Sharp filter.");
                        }
                        if (settingsDraft.filter == PictureFilter::Crt) {
                            ImGui::SliderFloat("Scanlines", &settingsDraft.scanline, kScanline.minimum, kScanline.maximum, "%.2f");
                            tip("Darkens every other shown line. Zero turns scanlines off.");
                        }
                        ImGui::Checkbox("Overscan", &settingsDraft.overscan);
                        tip("Crops eight pixels from each edge of the shown picture. The emulated picture is left as the system drew it.");
                        ImGui::Checkbox("Fullscreen", &settingsDraft.fullscreen);
                        tip("Fills the display. Alt and the Fullscreen key do the same.");
                        ImGui::Checkbox("VSync", &settingsDraft.vsync);
                        tip("Waits for the display refresh before swapping the window.");
                        ImGui::Checkbox("Performance", &settingsDraft.showPerformance);
                        tip("Shows frame timing on the picture.");
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
                        drawBindingButton("Turbo A", settingsDraft.input.turboA, nullptr, captureBinding);
                        drawBindingButton("Turbo B", settingsDraft.input.turboB, nullptr, captureBinding);
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
                        drawBindingButton("Fast forward", settingsDraft.input.fastForward, nullptr, captureBinding);
                        drawBindingButton("Screenshot", settingsDraft.input.screenshot, nullptr, captureBinding);
                        drawBindingButton("Fullscreen", settingsDraft.input.fullscreen, "Alt+", captureBinding);
                        drawBindingButton("Reset", settingsDraft.input.reset, "Ctrl+", captureBinding);
                        ImGui::TextWrapped(
                            "Click a key, then press the replacement. Numpad Enter also presses Start. "
                            "Shift+1 through Shift+0 still select state slots. "
                            "Fast forward skips the frame wait, and a screenshot saves the shown picture.");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Emulation")) {
                        int region = static_cast<int>(settingsDraft.region);
                        if (ImGui::Combo("Region", &region, "Automatic\0NTSC\0PAL\0Dendy\0")) {
                            settingsDraft.region = static_cast<RegionChoice>(region);
                        }
                        ImGui::SetItemTooltip(
                            "Automatic follows the cartridge header. The other choices change the frame length without clearing work RAM.");
                        ImGui::TextWrapped(
                            "Apply stores the choice. Power Cycle and Reset stay in the Emulation menu. "
                            "Power Cycle clears work RAM. Reset keeps it. Battery-backed cartridge RAM stays either way.");
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
                            ImGui::TextWrapped("Screenshots");
                            ImGui::TextWrapped("%s", pathToUtf8(appPaths->screenshots).c_str());
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
                    if (settingsDraft.theme != config_.theme) {
                        applyShellTheme(config_.theme, contentScale);
                        palette = shellPalette(config_.theme);
                    }
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
                if (beginClassicPopup("Reset settings", palette, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped(
                        "Theme, picture, audio, region, and controls return to their defaults. The window size stays as it is.");
                    if (ImGui::Button("Reset", ImVec2(75.0f, 0.0f))) {
                        const int width = settingsDraft.windowWidth;
                        const int height = settingsDraft.windowHeight;
                        const bool keepFullscreen = settingsDraft.fullscreen;
                        settingsDraft = ConfigService::defaults();
                        settingsDraft.windowWidth = width;
                        settingsDraft.windowHeight = height;
                        settingsDraft.fullscreen = keepFullscreen;
                        applyShellTheme(settingsDraft.theme, contentScale);
                        palette = shellPalette(settingsDraft.theme);
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
            if (beginClassicPopup("Remove from library", palette, ImGuiWindowFlags_AlwaysAutoResize)) {
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
            if (openGameInfo) {
                gameInfoText = viewInfo.title + "\n" + viewInfo.detail;
                gameInfoText += viewInfo.batteryBacked ? "\nBattery-backed RAM" : "\nNo battery-backed RAM";
                if (!viewContentId.empty()) {
                    gameInfoText += "\n" + viewContentId;
                }
                if (!loadedGamePath.empty()) {
                    gameInfoText += "\n" + loadedGamePath;
                }
            }
            drawModal("Game Information", gameInfoText.c_str(), openGameInfo, palette);
            drawModal("About Iris Emulator", aboutText.c_str(), openAbout, palette);
            if (openPicture) {
                ImGui::OpenPopup("Picture");
                openPicture = false;
            }
            if (beginClassicPopup("Picture", palette, ImGuiWindowFlags_AlwaysAutoResize)) {
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
            if (beginClassicPopup("State Slots", palette, ImGuiWindowFlags_AlwaysAutoResize)) {
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
                            {
                                std::lock_guard lock(gate);
                                system_->inspectState(bytes.data(), bytes.size(), preview);
                            }
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
                const std::string slotHint = scancodeLabel(config_.input.quickSave) + " saves the current slot. " +
                                             scancodeLabel(config_.input.quickLoad) +
                                             " loads it. Shift+1 through Shift+0 select a slot.";
                ImGui::TextUnformatted(slotHint.c_str());
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
                openLegal,
                palette);
            drawModal(
                "Open Source Licenses",
                "This build uses the following open-source libraries. Full license texts are in THIRD_PARTY_NOTICES.md beside the program.\n\n"
                "SDL 3.4.18 — zlib License\n"
                "Dear ImGui 1.92.9b — MIT License\n"
                "spdlog 1.17.0 — MIT License, which bundles fmt, also MIT\n"
                "nlohmann/json 3.12.0 — MIT License\n"
                "Catch2 3.16.1 — Boost Software License 1.0. Test builds only. Not part of the application.\n\n"
                "Iris does not include Tahoma. On Windows it reads the copy installed with the operating system.",
                openLicenses,
                palette);
            drawModal(errorTitle.c_str(), errorText.c_str(), openError, palette);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Render();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        const ShellColor& clearColor = fullscreen ? palette.face : palette.frame;
        glClearColor(clearColor.red, clearColor.green, clearColor.blue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(session.window);
#if defined(_WIN32)
        // The shell was drawing as fast as the GPU allowed. On battery that
        // work took the core the emulator needs, and the audio queue ran dry.
        hostClock.waitUntil(hostFrameStart + std::chrono::nanoseconds(16'666'667));
#endif
        ++hostFrames;
        const double fpsElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsWindow).count();
        if (fpsElapsed >= 0.5) {
            hostFps = static_cast<double>(hostFrames) / fpsElapsed;
            emulatedFps = static_cast<double>(pacedFrames.exchange(0)) / fpsElapsed;
            hostFrames = 0;
            fpsWindow = std::chrono::steady_clock::now();
        }
    }

    stopWorker.store(true);
    if (emulation.joinable()) {
        emulation.join();
    }
    closePads();
    flushBattery();
    if ((SDL_GetWindowFlags(session.window) & SDL_WINDOW_FULLSCREEN) == 0) {
        SDL_GetWindowSize(session.window, &windowWidth_, &windowHeight_);
    }
    config_.windowWidth = windowWidth_;
    config_.windowHeight = windowHeight_;
    return 0;
}

}  // namespace iris
