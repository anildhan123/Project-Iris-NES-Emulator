#include "iris/nes/nes_system.hpp"

#include "iris/byte_io.hpp"
#include "iris/sha256.hpp"
#include "iris/version.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <map>
#include <string>

namespace iris::systems::nes {
namespace {

constexpr std::uint16_t kStateVersion = 1;
constexpr std::uint16_t kSystemId = 1;
constexpr int kThumbnailWidth = 64;
constexpr int kThumbnailHeight = 60;
constexpr std::size_t kMaxStateBytes = 4 * 1024 * 1024;
constexpr std::size_t kMaxTextBytes = 512;

const char* kChunkNames[] = {"CPU ", "PPU ", "APU ", "MAP ", "CART", "PAD ", "BUS ", "TIME"};

std::string hexEncode(const std::uint8_t* data, std::size_t size) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        text[index * 2] = kDigits[data[index] >> 4];
        text[index * 2 + 1] = kDigits[data[index] & 0x0F];
    }
    return text;
}

void writeChunk(ByteWriter& writer, const char* tag, const std::vector<std::uint8_t>& payload) {
    writer.bytes(reinterpret_cast<const std::uint8_t*>(tag), 4);
    writer.u32(static_cast<std::uint32_t>(payload.size()));
    writer.bytes(payload.data(), payload.size());
}

std::vector<std::uint8_t> thumbnailFrom(const std::uint8_t* pixels) {
    std::vector<std::uint8_t> thumb(static_cast<std::size_t>(kThumbnailWidth * kThumbnailHeight * 4));
    if (pixels == nullptr) {
        return thumb;
    }
    for (int y = 0; y < kThumbnailHeight; ++y) {
        for (int x = 0; x < kThumbnailWidth; ++x) {
            const std::size_t source = (static_cast<std::size_t>(y * 4) * 256 + static_cast<std::size_t>(x * 4)) * 4;
            const std::size_t destination = (static_cast<std::size_t>(y) * kThumbnailWidth + static_cast<std::size_t>(x)) * 4;
            std::memcpy(thumb.data() + destination, pixels + source, 4);
        }
    }
    return thumb;
}

struct Envelope {
    std::array<std::uint8_t, 32> hash{};
    std::uint16_t mapper = 0;
    std::uint16_t submapper = 0;
    std::uint8_t region = 0;
    std::uint64_t savedAt = 0;
    std::string title;
    std::string description;
    std::vector<std::uint8_t> thumbnail;
    std::vector<std::uint8_t> components;
    std::string emulatorVersion;
};

bool decodeEnvelope(const std::uint8_t* data, std::size_t size, Envelope& envelope, std::string& error) {
    if (data == nullptr || size < 16 || size > kMaxStateBytes) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    const std::uint32_t stored = crc32(data, size - 4);
    ByteReader tail(data + size - 4, 4);
    if (tail.u32() != stored) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    ByteReader reader(data, size - 4);
    char magic[4] = {};
    reader.bytes(reinterpret_cast<std::uint8_t*>(magic), 4);
    if (!reader.ok() || magic[0] != 'I' || magic[1] != 'R' || magic[2] != 'S' || magic[3] != '1') {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    const std::uint16_t version = reader.u16();
    const std::uint16_t system = reader.u16();
    const std::uint16_t major = reader.u16();
    const std::uint16_t minor = reader.u16();
    const std::uint16_t patch = reader.u16();
    if (version != kStateVersion) {
        error = "This state was written by a newer version and cannot be loaded.";
        return false;
    }
    if (system != kSystemId) {
        error = "This state is not for the NES.";
        return false;
    }
    reader.bytes(envelope.hash.data(), envelope.hash.size());
    envelope.mapper = reader.u16();
    envelope.submapper = reader.u16();
    envelope.region = reader.u8();
    const std::uint8_t flags = reader.u8();
    if (flags != 0) {
        error = "This state uses features this version does not support.";
        return false;
    }
    envelope.savedAt = reader.u64();
    const std::uint16_t titleLength = reader.u16();
    const std::uint16_t descriptionLength = reader.u16();
    if (titleLength > kMaxTextBytes || descriptionLength > kMaxTextBytes) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    envelope.title.assign(titleLength, '\0');
    envelope.description.assign(descriptionLength, '\0');
    if (titleLength > 0) {
        reader.bytes(reinterpret_cast<std::uint8_t*>(envelope.title.data()), titleLength);
    }
    if (descriptionLength > 0) {
        reader.bytes(reinterpret_cast<std::uint8_t*>(envelope.description.data()), descriptionLength);
    }
    const std::uint16_t thumbWidth = reader.u16();
    const std::uint16_t thumbHeight = reader.u16();
    if (thumbWidth != kThumbnailWidth || thumbHeight != kThumbnailHeight) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    envelope.thumbnail.resize(static_cast<std::size_t>(thumbWidth) * thumbHeight * 4);
    reader.bytes(envelope.thumbnail.data(), envelope.thumbnail.size());
    const std::uint32_t componentSize = reader.u32();
    if (!reader.ok() || componentSize > kMaxStateBytes) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    envelope.components.resize(componentSize);
    if (componentSize > 0) {
        reader.bytes(envelope.components.data(), componentSize);
    }
    if (!reader.ok() || !reader.exhausted()) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    envelope.emulatorVersion = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    return true;
}

}  // namespace

std::string contentIdForCartridge(const Cartridge& cartridge) {
    ByteWriter writer;
    writer.u16(static_cast<std::uint16_t>(cartridge.mapper()));
    writer.u16(static_cast<std::uint16_t>(cartridge.submapper()));
    writer.u8(static_cast<std::uint8_t>(cartridge.region()));
    writer.boolean(cartridge.chrRam());
    writer.bytes(cartridge.prg().data(), cartridge.prg().size());
    if (cartridge.chrRam()) {
        writer.u32(static_cast<std::uint32_t>(cartridge.chr().size()));
    } else {
        writer.bytes(cartridge.chr().data(), cartridge.chr().size());
    }
    return sha256Hex(writer.data().data(), writer.data().size());
}

void NesSystem::refreshContentId() {
    contentId_ = contentIdForCartridge(cartridge_);
}

bool NesSystem::exportComponents(std::vector<std::uint8_t>& out) const {
    if (mapper_ == nullptr) {
        return false;
    }
    ByteWriter cpu;
    cpu_.captureState(cpu);
    ByteWriter ppu;
    ppu_.captureState(ppu);
    ByteWriter apu;
    apu_.captureState(apu);
    ByteWriter map;
    mapper_->captureState(map);
    ByteWriter pad;
    port1_.captureState(pad);
    port2_.captureState(pad);
    ByteWriter bus;
    bus_.captureState(bus);
    ByteWriter cart;
    cart.u32(static_cast<std::uint32_t>(cartridge_.prgRam().size()));
    cart.bytes(cartridge_.prgRam().data(), cartridge_.prgRam().size());
    cart.boolean(cartridge_.chrRam());
    if (cartridge_.chrRam()) {
        cart.u32(static_cast<std::uint32_t>(cartridge_.chr().size()));
        cart.bytes(cartridge_.chr().data(), cartridge_.chr().size());
    } else {
        cart.u32(0);
    }
    ByteWriter time;
    time.u64(cpuCycles_);
    time.u32(static_cast<std::uint32_t>(ppuAccum_));
    time.u32(static_cast<std::uint32_t>(dmcStall_));
    time.boolean(nmiLevel_);
    time.boolean(paused_);

    ByteWriter all;
    writeChunk(all, "CPU ", cpu.data());
    writeChunk(all, "PPU ", ppu.data());
    writeChunk(all, "APU ", apu.data());
    writeChunk(all, "MAP ", map.data());
    writeChunk(all, "PAD ", pad.data());
    writeChunk(all, "BUS ", bus.data());
    writeChunk(all, "CART", cart.data());
    writeChunk(all, "TIME", time.data());
    out = all.data();
    return true;
}

bool NesSystem::importComponents(const std::uint8_t* data, std::size_t size, std::string& error) {
    std::map<std::string, std::vector<std::uint8_t>> chunks;
    ByteReader reader(data, size);
    while (reader.ok() && !reader.exhausted()) {
        char tag[4] = {};
        reader.bytes(reinterpret_cast<std::uint8_t*>(tag), 4);
        const std::uint32_t length = reader.u32();
        if (!reader.ok() || length > size) {
            error = "The state file is damaged or incomplete.";
            return false;
        }
        std::vector<std::uint8_t> payload(length);
        if (length > 0) {
            reader.bytes(payload.data(), length);
        }
        const std::string name(tag, tag + 4);
        if (!chunks.emplace(name, std::move(payload)).second) {
            error = "The state file is damaged or incomplete.";
            return false;
        }
    }
    if (!reader.ok()) {
        error = "The state file is damaged or incomplete.";
        return false;
    }
    for (const char* name : kChunkNames) {
        if (chunks.find(name) == chunks.end()) {
            error = "The state file is damaged or incomplete.";
            return false;
        }
    }
    if (chunks.size() != std::size(kChunkNames)) {
        error = "The state file is damaged or incomplete.";
        return false;
    }

    auto apply = [&](const char* name, auto&& function) {
        const auto& payload = chunks.at(name);
        ByteReader chunk(payload.data(), payload.size());
        if (!function(chunk)) {
            error = "The state file is damaged or incomplete.";
            return false;
        }
        return true;
    };
    if (!apply("CPU ", [&](ByteReader& chunk) { return cpu_.applyState(chunk); })) return false;
    if (!apply("PPU ", [&](ByteReader& chunk) { return ppu_.applyState(chunk); })) return false;
    if (!apply("APU ", [&](ByteReader& chunk) { return apu_.applyState(chunk); })) return false;
    if (!apply("MAP ", [&](ByteReader& chunk) { return mapper_->applyState(chunk); })) return false;
    if (!apply("PAD ", [&](ByteReader& chunk) {
            return port1_.applyState(chunk) && port2_.applyState(chunk) && chunk.exhausted();
        })) {
        return false;
    }
    if (!apply("BUS ", [&](ByteReader& chunk) { return bus_.applyState(chunk); })) return false;
    if (!apply("CART", [&](ByteReader& chunk) {
            const std::uint32_t prgSize = chunk.u32();
            if (prgSize != cartridge_.prgRam().size()) {
                return false;
            }
            std::vector<std::uint8_t> prg(prgSize);
            if (prgSize > 0) {
                chunk.bytes(prg.data(), prgSize);
            }
            const bool chrRam = chunk.boolean();
            const std::uint32_t chrSize = chunk.u32();
            if (chrRam != cartridge_.chrRam()) {
                return false;
            }
            std::vector<std::uint8_t> chr;
            if (chrRam) {
                if (chrSize != cartridge_.chr().size()) {
                    return false;
                }
                chr.resize(chrSize);
                if (chrSize > 0) {
                    chunk.bytes(chr.data(), chrSize);
                }
            } else if (chrSize != 0) {
                return false;
            }
            if (!chunk.ok() || !chunk.exhausted()) {
                return false;
            }
            cartridge_.prgRam() = std::move(prg);
            if (chrRam) {
                cartridge_.chr() = std::move(chr);
            }
            return true;
        })) {
        return false;
    }
    if (!apply("TIME", [&](ByteReader& chunk) {
            const std::uint64_t cycles = chunk.u64();
            const std::uint32_t ppuAccum = chunk.u32();
            const std::uint32_t dmcStall = chunk.u32();
            const bool nmi = chunk.boolean();
            const bool paused = chunk.boolean();
            if (!chunk.ok() || !chunk.exhausted() || ppuAccum > 16 || dmcStall > 32) {
                return false;
            }
            cpuCycles_ = cycles;
            ppuAccum_ = static_cast<int>(ppuAccum);
            dmcStall_ = static_cast<int>(dmcStall);
            nmiLevel_ = nmi;
            paused_ = paused;
            return true;
        })) {
        return false;
    }
    return true;
}

bool NesSystem::saveState(std::vector<std::uint8_t>& bytes, std::string_view description, std::string& error) {
    if (!loaded_ || mapper_ == nullptr) {
        error = "No game is loaded.";
        return false;
    }
    if (description.size() > kMaxTextBytes || title_.size() > kMaxTextBytes) {
        error = "The description is too long.";
        return false;
    }
    std::vector<std::uint8_t> components;
    if (!exportComponents(components)) {
        error = "The state could not be created.";
        return false;
    }
    const auto thumbnail = thumbnailFrom(ppu_.pixels());
    std::uint8_t hash[32] = {};
    for (std::size_t index = 0; index < contentId_.size(); index += 2) {
        const auto nibble = [](char text) {
            if (text >= '0' && text <= '9') return text - '0';
            if (text >= 'a' && text <= 'f') return text - 'a' + 10;
            return 0;
        };
        hash[index / 2] = static_cast<std::uint8_t>((nibble(contentId_[index]) << 4) | nibble(contentId_[index + 1]));
    }
    const auto savedAt = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    ByteWriter writer;
    writer.bytes(reinterpret_cast<const std::uint8_t*>("IRS1"), 4);
    writer.u16(kStateVersion);
    writer.u16(kSystemId);
    writer.u16(static_cast<std::uint16_t>(kVersionMajor));
    writer.u16(static_cast<std::uint16_t>(kVersionMinor));
    writer.u16(static_cast<std::uint16_t>(kVersionPatch));
    writer.bytes(hash, 32);
    writer.u16(static_cast<std::uint16_t>(cartridge_.mapper()));
    writer.u16(static_cast<std::uint16_t>(cartridge_.submapper()));
    writer.u8(static_cast<std::uint8_t>(static_cast<int>(cartridge_.region())));
    writer.u8(0);
    writer.u64(savedAt);
    writer.u16(static_cast<std::uint16_t>(title_.size()));
    writer.u16(static_cast<std::uint16_t>(description.size()));
    writer.bytes(reinterpret_cast<const std::uint8_t*>(title_.data()), title_.size());
    writer.bytes(reinterpret_cast<const std::uint8_t*>(description.data()), description.size());
    writer.u16(kThumbnailWidth);
    writer.u16(kThumbnailHeight);
    writer.bytes(thumbnail.data(), thumbnail.size());
    writer.u32(static_cast<std::uint32_t>(components.size()));
    writer.bytes(components.data(), components.size());
    const std::uint32_t checksum = crc32(writer.data().data(), writer.data().size());
    writer.u32(checksum);
    bytes = writer.data();
    return true;
}

bool NesSystem::loadState(const std::uint8_t* data, std::size_t size, std::string& error) {
    if (!loaded_) {
        error = "No game is loaded.";
        return false;
    }
    Envelope envelope;
    if (!decodeEnvelope(data, size, envelope, error)) {
        return false;
    }
    if (hexEncode(envelope.hash.data(), envelope.hash.size()) != contentId_ ||
        envelope.mapper != cartridge_.mapper() || envelope.submapper != cartridge_.submapper() ||
        envelope.region != static_cast<std::uint8_t>(static_cast<int>(cartridge_.region()))) {
        error = "This state is for a different game.";
        return false;
    }
    std::vector<std::uint8_t> backup;
    if (!exportComponents(backup)) {
        error = "The state could not be loaded.";
        return false;
    }
    std::string importError;
    if (!importComponents(envelope.components.data(), envelope.components.size(), importError)) {
        std::string restoreError;
        if (!importComponents(backup.data(), backup.size(), restoreError)) {
            error = "The state could not be loaded.";
            return false;
        }
        error = importError.empty() ? "The state file is damaged or incomplete." : importError;
        return false;
    }
    return true;
}

void NesSystem::inspectState(const std::uint8_t* data, std::size_t size, StatePreview& preview) const {
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

bool NesSystem::copyBattery(std::vector<std::uint8_t>& bytes) const {
    if (!loaded_ || !cartridge_.battery()) {
        return false;
    }
    bytes = cartridge_.prgRam();
    return true;
}

bool NesSystem::restoreBattery(const std::uint8_t* data, std::size_t size, std::string& error) {
    if (!loaded_ || !cartridge_.battery()) {
        error = "This cartridge has no battery-backed RAM.";
        return false;
    }
    if (data == nullptr || size != cartridge_.prgRam().size()) {
        error = "The battery save does not match this cartridge and was not loaded.";
        return false;
    }
    std::memcpy(cartridge_.prgRam().data(), data, size);
    return true;
}

}  // namespace iris::systems::nes
