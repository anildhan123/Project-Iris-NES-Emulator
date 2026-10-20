#pragma once

#include "iris/snes/cartridge.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace iris::systems::snes {

// Commercial Super NES catalogue helpers. No ROM data is stored here — only
// board classes and public title metadata for the compatibility matrix.

enum class CatalogueStatus : std::uint8_t {
    // Map/chip path exists and the suite smokes that board class.
    HardwarePath = 0,
    // Path exists, but a known stand-in or incomplete silicon edge remains.
    Partial = 1,
    // Board class is refused or missing a required feature for boot.
    Blocked = 2,
};

enum class BoardClass : std::uint8_t {
    LoRom,
    HiRom,
    ExHiRom,
    FastRom,
    Sa1,
    SuperFx,
    Dsp1,
    Dsp2,
    Dsp3,
    Dsp4,
    Cx4,
    Sdd1,
    Spc7110,
    Obc1,
    St010,
    St011,
    St018,
    Bsx,
};

struct CatalogueEntry {
    std::string_view title;
    BoardClass board;
    CatalogueStatus status;
    std::string_view notes;
};

[[nodiscard]] inline CatalogueStatus statusForChip(Chip chip) {
    switch (chip) {
    case Chip::None:
    case Chip::Dsp1:
    case Chip::Dsp2:
    case Chip::Dsp3:
    case Chip::Dsp4:
    case Chip::SuperFx:
    case Chip::Sa1:
    case Chip::Cx4:
    case Chip::Obc1:
    case Chip::St010:
    case Chip::St011:
    case Chip::St018:
    case Chip::Bsx:
        return CatalogueStatus::HardwarePath;
    case Chip::Sdd1:
    case Chip::Spc7110:
        // Decompression is a sequential ROM stream stand-in (no proprietary tables).
        return CatalogueStatus::Partial;
    }
    return CatalogueStatus::Blocked;
}

[[nodiscard]] inline const char* statusLabel(CatalogueStatus status) {
    switch (status) {
    case CatalogueStatus::HardwarePath:
        return "hardware-path";
    case CatalogueStatus::Partial:
        return "partial";
    case CatalogueStatus::Blocked:
        return "blocked";
    }
    return "blocked";
}

[[nodiscard]] inline const char* boardClassLabel(BoardClass board) {
    switch (board) {
    case BoardClass::LoRom:
        return "LoROM";
    case BoardClass::HiRom:
        return "HiROM";
    case BoardClass::ExHiRom:
        return "ExHiROM";
    case BoardClass::FastRom:
        return "FastROM";
    case BoardClass::Sa1:
        return "SA-1";
    case BoardClass::SuperFx:
        return "Super FX";
    case BoardClass::Dsp1:
        return "DSP-1";
    case BoardClass::Dsp2:
        return "DSP-2";
    case BoardClass::Dsp3:
        return "DSP-3";
    case BoardClass::Dsp4:
        return "DSP-4";
    case BoardClass::Cx4:
        return "Cx4";
    case BoardClass::Sdd1:
        return "S-DD1";
    case BoardClass::Spc7110:
        return "SPC7110";
    case BoardClass::Obc1:
        return "OBC-1";
    case BoardClass::St010:
        return "ST010";
    case BoardClass::St011:
        return "ST011";
    case BoardClass::St018:
        return "ST018";
    case BoardClass::Bsx:
        return "BS-X";
    }
    return "Unknown";
}

// Representative commercial titles used only as a public catalogue matrix.
// Status is board-class derived. Title-level playthrough is not claimed.
inline constexpr CatalogueEntry kCommercialCatalogue[] = {
    {"Super Mario World", BoardClass::LoRom, CatalogueStatus::HardwarePath, "Plain LoROM; suite class A"},
    {"The Legend of Zelda: A Link to the Past", BoardClass::LoRom, CatalogueStatus::HardwarePath, "Plain LoROM"},
    {"Super Metroid", BoardClass::LoRom, CatalogueStatus::HardwarePath, "Plain LoROM"},
    {"Chrono Trigger", BoardClass::HiRom, CatalogueStatus::HardwarePath, "HiROM; suite class A"},
    {"Final Fantasy VI", BoardClass::HiRom, CatalogueStatus::HardwarePath, "HiROM + Akao APU path (M29–30)"},
    {"Final Fantasy V", BoardClass::HiRom, CatalogueStatus::HardwarePath, "HiROM"},
    {"Secret of Mana", BoardClass::HiRom, CatalogueStatus::HardwarePath, "HiROM"},
    {"EarthBound", BoardClass::HiRom, CatalogueStatus::HardwarePath, "HiROM"},
    {"Donkey Kong Country", BoardClass::LoRom, CatalogueStatus::HardwarePath, "DSP mix polish class (M31)"},
    {"Donkey Kong Country 2", BoardClass::LoRom, CatalogueStatus::HardwarePath, "DSP mix polish class"},
    {"Super Mario Kart", BoardClass::Dsp1, CatalogueStatus::HardwarePath, "DSP-1 host math"},
    {"Pilotwings", BoardClass::Dsp1, CatalogueStatus::HardwarePath, "DSP-1 host math"},
    {"Ballz 3D", BoardClass::Dsp1, CatalogueStatus::HardwarePath, "DSP-1 host math"},
    {"Dungeon Master", BoardClass::Dsp2, CatalogueStatus::HardwarePath, "DSP-2"},
    {"SD Gundam GX", BoardClass::Dsp3, CatalogueStatus::HardwarePath, "DSP-3"},
    {"Top Gear 3000", BoardClass::Dsp4, CatalogueStatus::HardwarePath, "DSP-4"},
    {"Star Fox", BoardClass::SuperFx, CatalogueStatus::HardwarePath, "Super FX / GSU"},
    {"Star Fox 2", BoardClass::SuperFx, CatalogueStatus::HardwarePath, "Super FX / GSU"},
    {"Vortex", BoardClass::SuperFx, CatalogueStatus::HardwarePath, "Super FX / GSU"},
    {"Super Mario World 2: Yoshi's Island", BoardClass::Sa1, CatalogueStatus::HardwarePath, "SA-1 type-1/2 conversion"},
    {"Kirby Super Star", BoardClass::Sa1, CatalogueStatus::HardwarePath, "SA-1"},
    {"Super Mario RPG", BoardClass::Sa1, CatalogueStatus::HardwarePath, "SA-1"},
    {"Mega Man X2", BoardClass::Cx4, CatalogueStatus::HardwarePath, "Cx4"},
    {"Mega Man X3", BoardClass::Cx4, CatalogueStatus::HardwarePath, "Cx4"},
    {"Street Fighter Alpha 2", BoardClass::Sdd1, CatalogueStatus::Partial, "S-DD1 stream stand-in"},
    {"Star Ocean", BoardClass::Sdd1, CatalogueStatus::Partial, "S-DD1 stream stand-in"},
    {"Far East of Eden Zero", BoardClass::Spc7110, CatalogueStatus::Partial, "SPC7110 stream/RTC stand-in"},
    {"Momotarou Dentetsu Happy", BoardClass::Spc7110, CatalogueStatus::Partial, "SPC7110"},
    {"Metal Combat: Falcon's Revenge", BoardClass::Obc1, CatalogueStatus::HardwarePath, "OBC-1"},
    {"F1 ROC II: Race of Champions", BoardClass::St010, CatalogueStatus::HardwarePath, "ST010"},
    {"Hayazashi Nidan Morita Shogi", BoardClass::St011, CatalogueStatus::HardwarePath, "ST011"},
    {"Hayazashi Nidan Morita Shogi 2", BoardClass::St018, CatalogueStatus::HardwarePath, "ST018"},
    {"F-Zero", BoardClass::LoRom, CatalogueStatus::HardwarePath, "Mode 7 class"},
    {"Tales of Phantasia", BoardClass::ExHiRom, CatalogueStatus::HardwarePath, "ExHiROM"},
};

inline constexpr std::size_t kCommercialCatalogueCount =
    sizeof(kCommercialCatalogue) / sizeof(kCommercialCatalogue[0]);

[[nodiscard]] inline CatalogueStatus statusForCartridge(const Cartridge& cartridge) {
    if (cartridge.chip != Chip::None) {
        return statusForChip(cartridge.chip);
    }
    return CatalogueStatus::HardwarePath;
}

[[nodiscard]] inline const CatalogueEntry* findCatalogueEntry(std::string_view title) {
    for (std::size_t index = 0; index < kCommercialCatalogueCount; ++index) {
        if (kCommercialCatalogue[index].title == title) {
            return &kCommercialCatalogue[index];
        }
    }
    return nullptr;
}

}  // namespace iris::systems::snes
