#include "iris/snes/snes_system.hpp"

#include "iris/byte_io.hpp"
#include "iris/version.hpp"

#include <array>
#include <chrono>
#include <string>

namespace iris::systems::snes {
namespace {

constexpr std::uint16_t kStateVersion = 1;
constexpr std::uint16_t kSystemId = 2;
constexpr int kThumbnailWidth = 64;
constexpr int kThumbnailHeight = 56;
constexpr std::size_t kMaxTextBytes = 512;

std::string hexEncode(const std::uint8_t* data, std::size_t size) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        text[index * 2] = kDigits[data[index] >> 4];
        text[index * 2 + 1] = kDigits[data[index] & 0x0F];
    }
    return text;
}

int hexValue(char text) {
    if (text >= '0' && text <= '9') {
        return text - '0';
    }
    if (text >= 'a' && text <= 'f') {
        return text - 'a' + 10;
    }
    if (text >= 'A' && text <= 'F') {
        return text - 'A' + 10;
    }
    return -1;
}

std::vector<std::uint8_t> thumbnailFrom(const std::uint8_t* frame) {
    std::vector<std::uint8_t> thumb(static_cast<std::size_t>(kThumbnailWidth * kThumbnailHeight * 4), 0);
    if (frame == nullptr) {
        return thumb;
    }
    for (int y = 0; y < kThumbnailHeight; ++y) {
        for (int x = 0; x < kThumbnailWidth; ++x) {
            const std::size_t source =
                (static_cast<std::size_t>(y * 4) * Ppu::kWidth + static_cast<std::size_t>(x * 4)) * 4;
            const std::size_t destination =
                (static_cast<std::size_t>(y) * kThumbnailWidth + static_cast<std::size_t>(x)) * 4;
            thumb[destination] = frame[source];
            thumb[destination + 1] = frame[source + 1];
            thumb[destination + 2] = frame[source + 2];
            thumb[destination + 3] = 255;
        }
    }
    return thumb;
}

struct Envelope {
    std::array<std::uint8_t, 32> hash{};
    std::string title;
    std::string description;
    std::string emulatorVersion;
    std::uint64_t savedAt = 0;
    std::vector<std::uint8_t> thumbnail;
    std::vector<std::uint8_t> components;
};

bool decodeEnvelope(const std::uint8_t* data, std::size_t size, Envelope& envelope, std::string& error) {
    if (data == nullptr || size < 8) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    iris::ByteReader reader(data, size);
    char magic[4] = {};
    reader.bytes(reinterpret_cast<std::uint8_t*>(magic), 4);
    if (!reader.ok() || std::string(magic, 4) != "IRSN") {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    const auto version = reader.u16();
    const auto system = reader.u16();
    const auto major = reader.u16();
    const auto minor = reader.u16();
    const auto patch = reader.u16();
    reader.bytes(envelope.hash.data(), envelope.hash.size());
    (void)reader.u8();
    (void)reader.u8();
    envelope.savedAt = reader.u64();
    const auto titleBytes = reader.u16();
    const auto descriptionBytes = reader.u16();
    if (!reader.ok() || version != kStateVersion || system != kSystemId || titleBytes > kMaxTextBytes ||
        descriptionBytes > kMaxTextBytes) {
        error = version != kStateVersion || system != kSystemId ? "This state is for a different game."
                                                                : "The state file is damaged or incomplete.";
        return false;
    }
    envelope.emulatorVersion = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    envelope.title.assign(titleBytes, '\0');
    envelope.description.assign(descriptionBytes, '\0');
    if (titleBytes > 0) {
        reader.bytes(reinterpret_cast<std::uint8_t*>(envelope.title.data()), titleBytes);
    }
    if (descriptionBytes > 0) {
        reader.bytes(reinterpret_cast<std::uint8_t*>(envelope.description.data()), descriptionBytes);
    }
    const auto thumbWidth = reader.u16();
    const auto thumbHeight = reader.u16();
    if (!reader.ok() || thumbWidth != kThumbnailWidth || thumbHeight != kThumbnailHeight) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    envelope.thumbnail.resize(static_cast<std::size_t>(thumbWidth) * thumbHeight * 4);
    reader.bytes(envelope.thumbnail.data(), envelope.thumbnail.size());
    const auto componentBytes = reader.u32();
    if (!reader.ok() || componentBytes > 3 * 1024 * 1024) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    envelope.components.resize(componentBytes);
    reader.bytes(envelope.components.data(), envelope.components.size());
    const auto checksum = reader.u32();
    if (!reader.ok() || !reader.exhausted()) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    const std::uint32_t actual = iris::crc32(data, size - 4);
    if (actual != checksum) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    return true;
}

}  // namespace

bool SnesSystem::exportComponents(std::vector<std::uint8_t>& bytes) const {
    iris::ByteWriter writer;
    cpu_.saveState(writer);
    sa1_.saveState(writer);
    bus_.saveState(writer);
    writer.boolean(paused_);
    writer.u8(static_cast<std::uint8_t>(standard_));
    writer.u8(static_cast<std::uint8_t>(regionChoice_));
    bytes = writer.data();
    return true;
}

bool SnesSystem::importComponents(const std::uint8_t* data, std::size_t size) {
    iris::ByteReader reader(data, size);
    if (!cpu_.loadState(reader) || !sa1_.loadState(reader) || !bus_.loadState(reader)) {
        return false;
    }
    const auto paused = reader.boolean();
    const auto standard = reader.u8();
    const auto region = reader.u8();
    if (!reader.ok() || !reader.exhausted() || standard > static_cast<std::uint8_t>(TvStandard::Pal) ||
        region > static_cast<std::uint8_t>(iris::RegionChoice::Dendy)) {
        return false;
    }
    paused_ = paused;
    standard_ = static_cast<TvStandard>(standard);
    regionChoice_ = static_cast<iris::RegionChoice>(region);
    return true;
}

bool SnesSystem::saveState(std::vector<std::uint8_t>& bytes, std::string_view description, std::string& error) {
    if (!loaded_) {
        error = "No game is loaded.";
        return false;
    }
    if (description.size() > kMaxTextBytes || cartridge_.title.size() > kMaxTextBytes || contentId_.size() != 64) {
        error = "The description is too long.";
        return false;
    }
    std::array<std::uint8_t, 32> hash{};
    for (std::size_t index = 0; index < hash.size(); ++index) {
        const int high = hexValue(contentId_[index * 2]);
        const int low = hexValue(contentId_[index * 2 + 1]);
        if (high < 0 || low < 0) {
            error = "The state could not be created.";
            return false;
        }
        hash[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    std::vector<std::uint8_t> components;
    if (!exportComponents(components)) {
        error = "The state could not be created.";
        return false;
    }
    const auto thumbnail = thumbnailFrom(bus_.ppu().pixels());
    const auto savedAt = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    iris::ByteWriter writer;
    writer.bytes(reinterpret_cast<const std::uint8_t*>("IRSN"), 4);
    writer.u16(kStateVersion);
    writer.u16(kSystemId);
    writer.u16(static_cast<std::uint16_t>(iris::kVersionMajor));
    writer.u16(static_cast<std::uint16_t>(iris::kVersionMinor));
    writer.u16(static_cast<std::uint16_t>(iris::kVersionPatch));
    writer.bytes(hash.data(), hash.size());
    writer.u8(static_cast<std::uint8_t>(cartridge_.standard));
    writer.u8(static_cast<std::uint8_t>(cartridge_.chip));
    writer.u64(savedAt);
    writer.u16(static_cast<std::uint16_t>(cartridge_.title.size()));
    writer.u16(static_cast<std::uint16_t>(description.size()));
    writer.bytes(reinterpret_cast<const std::uint8_t*>(cartridge_.title.data()), cartridge_.title.size());
    writer.bytes(reinterpret_cast<const std::uint8_t*>(description.data()), description.size());
    writer.u16(kThumbnailWidth);
    writer.u16(kThumbnailHeight);
    writer.bytes(thumbnail.data(), thumbnail.size());
    writer.u32(static_cast<std::uint32_t>(components.size()));
    writer.bytes(components.data(), components.size());
    const std::uint32_t checksum = iris::crc32(writer.data().data(), writer.data().size());
    writer.u32(checksum);
    bytes = writer.data();
    return true;
}

bool SnesSystem::loadState(const std::uint8_t* data, std::size_t size, std::string& error) {
    if (!loaded_) {
        error = "No game is loaded.";
        return false;
    }
    Envelope envelope;
    if (!decodeEnvelope(data, size, envelope, error)) {
        return false;
    }
    if (hexEncode(envelope.hash.data(), envelope.hash.size()) != contentId_) {
        error = "This state is for a different game.";
        return false;
    }
    std::vector<std::uint8_t> backup;
    if (!exportComponents(backup)) {
        error = "The state could not be loaded.";
        return false;
    }
    if (!importComponents(envelope.components.data(), envelope.components.size())) {
        (void)importComponents(backup.data(), backup.size());
        error = "The state file is damaged or incomplete.";
        return false;
    }
    return true;
}

void SnesSystem::inspectState(const std::uint8_t* data, std::size_t size, StatePreview& preview) const {
    preview = {};
    Envelope envelope;
    if (!decodeEnvelope(data, size, envelope, preview.error)) {
        preview.readable = false;
        return;
    }
    preview.readable = true;
    preview.title = envelope.title;
    preview.description = envelope.description;
    preview.contentId = hexEncode(envelope.hash.data(), envelope.hash.size());
    preview.emulatorVersion = envelope.emulatorVersion;
    preview.savedAtUnixMs = static_cast<std::int64_t>(envelope.savedAt);
    preview.thumbnailWidth = kThumbnailWidth;
    preview.thumbnailHeight = kThumbnailHeight;
    preview.thumbnailRgba = envelope.thumbnail;
    preview.sameGame = loaded_ && preview.contentId == contentId_;
}

}  // namespace iris::systems::snes
