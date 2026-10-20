# Super NES commercial game catalogue

This matrix tracks **board-class** expectations for public Super NES titles against the Iris hardware path (milestones 29–48) and the commercial playability programme (milestones 61–68).

## Legal

- Iris does **not** ship commercial ROMs, BIOS files, or proprietary coefficient tables.
- Titles are listed by public name only so you can match boards you legally own.
- CI never loads a commercial dump. Status is derived from map/chip coverage and suite smokes.
- A title marked `hardware-path` is **not** a verified full playthrough.

## Status legend

| Status | Meaning |
|---|---|
| `hardware-path` | Iris loads that map/chip class; suite smoke exists. Untested with a commercial dump in CI. |
| `partial` | Path exists, but a known stand-in remains (for example S-DD1 / SPC7110 decompression without Nintendo tables). |
| `blocked` | Required board feature is refused or missing. |
| `untested` | Reserved for a local user worksheet after you run a legal dump (not used in the shipped matrix rows). |

## How to use a legal dump locally

1. Open the ROM you are entitled to use in Iris.
2. Note the status line in the window detail (S-DD1 / SPC7110 append `catalogue: partial`).
3. Optionally record the 64-character content id (SHA-256) and your result in a private worksheet. Do not commit ROM files or dumps to this repository.

## Matrix

| Title | Board class | Status | Notes |
|---|---|---|---|
| Super Mario World | LoROM | hardware-path | Mainstream LoROM class |
| The Legend of Zelda: A Link to the Past | LoROM | hardware-path | Mainstream LoROM class |
| Super Metroid | LoROM | hardware-path | Mainstream LoROM class |
| Chrono Trigger | HiROM | hardware-path | Mainstream HiROM class |
| Final Fantasy VI | HiROM | hardware-path | HiROM + Akao APU path (M29–30) |
| Final Fantasy V | HiROM | hardware-path | HiROM |
| Secret of Mana | HiROM | hardware-path | HiROM |
| EarthBound | HiROM | hardware-path | HiROM |
| Donkey Kong Country | LoROM | hardware-path | DSP mix polish class (M31) |
| Donkey Kong Country 2 | LoROM | hardware-path | DSP mix polish class |
| F-Zero | LoROM | hardware-path | Mode 7 class |
| Super Mario Kart | DSP-1 | hardware-path | DSP-1 host math |
| Pilotwings | DSP-1 | hardware-path | DSP-1 host math |
| Ballz 3D | DSP-1 | hardware-path | DSP-1 host math |
| Dungeon Master | DSP-2 | hardware-path | DSP-2 |
| SD Gundam GX | DSP-3 | hardware-path | DSP-3 |
| Top Gear 3000 | DSP-4 | hardware-path | DSP-4 |
| Star Fox | Super FX | hardware-path | GSU path |
| Star Fox 2 | Super FX | hardware-path | GSU path |
| Vortex | Super FX | hardware-path | GSU path |
| Super Mario World 2: Yoshi's Island | SA-1 | hardware-path | SA-1 type-1/2 conversion |
| Kirby Super Star | SA-1 | hardware-path | SA-1 |
| Super Mario RPG | SA-1 | hardware-path | SA-1 |
| Mega Man X2 | Cx4 | hardware-path | Cx4 |
| Mega Man X3 | Cx4 | hardware-path | Cx4 |
| Street Fighter Alpha 2 | S-DD1 | partial | Stream stand-in; no Nintendo coefficient table |
| Star Ocean | S-DD1 | partial | Stream stand-in |
| Far East of Eden Zero | SPC7110 | partial | Stream + RTC stand-in |
| Momotarou Dentetsu Happy | SPC7110 | partial | Stream + RTC stand-in |
| Metal Combat: Falcon's Revenge | OBC-1 | hardware-path | OBC-1 remap |
| F1 ROC II: Race of Champions | ST010 | hardware-path | ST010 helpers |
| Hayazashi Nidan Morita Shogi | ST011 | hardware-path | ST011 helpers |
| Hayazashi Nidan Morita Shogi 2 | ST018 | hardware-path | ST018 helpers |
| Tales of Phantasia | ExHiROM | hardware-path | ExHiROM |

The same rows are mirrored in `src/systems/snes/iris/snes/catalogue.hpp` for the suite.

## What “100% commercial compatibility” means here

The roadmap programme (M61–68) freezes:

1. A public catalogue matrix for representative titles.
2. Suite coverage that every listed board class **loads**.
3. Honest `partial` marking where decompression or RTC is a host stand-in.
4. Status reporting in the Super NES detail string for partial boards.

It does **not** mean every commercial cartridge has been playtested end-to-end in CI. That claim requires legal dumps and human or private automated runs outside this repository.

## Known remaining gaps for catalogue titles

- S-DD1 / SPC7110: sequential ROM stream instead of proprietary decompression tables.
- DSP-1…4: host arithmetic (documented where possible; no Nintendo coefficient ROM).
- Super FX / SA-1 / Cx4: suite-proven command paths; not every commercial edge case.
- Title-level audio/visual glitches may still appear until verified against a legal dump.
