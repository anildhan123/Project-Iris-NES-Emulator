#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace iris {

// Completed PPU picture. The pointer is owned by the system and stays valid until the next runFrame or unload.
struct FrameView {
    const std::uint8_t* rgba = nullptr;
    int width = 0;
    int height = 0;
};

// Mono samples produced by the most recent runFrame. Valid until the next runFrame or unload.
struct AudioView {
    const float* samples = nullptr;
    int count = 0;
    int sampleRate = 48000;
};

struct SystemInfo {
    std::string title;
    std::string detail;
    bool batteryBacked = false;
};

// Metadata from a state file. The thumbnail is owned by this struct.
struct StatePreview {
    bool readable = false;
    bool sameGame = false;
    std::string title;
    std::string description;
    std::string contentId;
    std::string emulatorVersion;
    std::int64_t savedAtUnixMs = 0;
    int thumbnailWidth = 0;
    int thumbnailHeight = 0;
    std::vector<std::uint8_t> thumbnailRgba;
    std::string error;
};

// Button bits match the NES serial order: A, B, Select, Start, Up, Down, Left, Right.
inline constexpr std::uint8_t kButtonA = 0x01;
inline constexpr std::uint8_t kButtonB = 0x02;
inline constexpr std::uint8_t kButtonSelect = 0x04;
inline constexpr std::uint8_t kButtonStart = 0x08;
inline constexpr std::uint8_t kButtonUp = 0x10;
inline constexpr std::uint8_t kButtonDown = 0x20;
inline constexpr std::uint8_t kButtonLeft = 0x40;
inline constexpr std::uint8_t kButtonRight = 0x80;

// The desktop shell talks only to this interface. It does not include CPU, PPU, or mapper types.
class IEmulatedSystem {
public:
    virtual ~IEmulatedSystem() = default;

    virtual bool loadContent(const std::filesystem::path& path, std::string& error) = 0;
    virtual void unload() = 0;
    [[nodiscard]] virtual bool loaded() const = 0;
    virtual void reset() = 0;
    virtual void setPaused(bool paused) = 0;
    [[nodiscard]] virtual bool paused() const = 0;
    virtual void runFrame() = 0;
    [[nodiscard]] virtual FrameView frame() const = 0;
    [[nodiscard]] virtual AudioView audio() const = 0;
    virtual void setController(int port, std::uint8_t buttons) = 0;
    [[nodiscard]] virtual SystemInfo info() const = 0;
    // Host pacing for one emulated frame. This is independent of the monitor refresh rate.
    [[nodiscard]] virtual std::int64_t framePeriodNs() const = 0;
    [[nodiscard]] virtual std::string contentId() const = 0;
    virtual bool saveState(std::vector<std::uint8_t>& bytes, std::string_view description, std::string& error) = 0;
    virtual bool loadState(const std::uint8_t* data, std::size_t size, std::string& error) = 0;
    virtual void inspectState(const std::uint8_t* data, std::size_t size, StatePreview& preview) const = 0;
    virtual bool copyBattery(std::vector<std::uint8_t>& bytes) const = 0;
    virtual bool restoreBattery(const std::uint8_t* data, std::size_t size, std::string& error) = 0;
};

}  // namespace iris
