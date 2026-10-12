# Legal compliance

Iris is an independently written emulator. Milestone 1 distributes no game, firmware, or copied interface artwork.

## Project rules

- Do not commit commercial ROMs, BIOS files, proprietary firmware, or copied cartridge data.
- Do not download games from ROM sites, and do not add a ROM search.
- Users supply files they are legally entitled to use. Owning a cartridge does not by itself make every dump or download lawful.
- Do not copy another emulator's branding, icons, or artwork.
- Do not vendor the Tahoma font. On Windows the shell may read the copy already installed with the operating system. Otherwise it uses Dear ImGui's built-in font.
- Do not collect ROM contents, filenames, or gameplay telemetry. Game files and saves stay local.
- Review dependency licenses again before any commercial distribution. This document is not that review.

## Dependency inventory

Fetched and pinned by `cmake/Dependencies.cmake`. Versions below were checked against upstream tags on 2026-10-12.

- **SDL 3.4.18** — zlib — https://github.com/libsdl-org/SDL — tag `release-3.4.18` — `829a65d769d935c4852f8159e964312c0957260a`
- **Dear ImGui 1.92.9b** — MIT — https://github.com/ocornut/imgui — tag `v1.92.9b` — `f1cc2ae15e53a861a874c3034aae6798fde194ab`
  - The OpenGL backend includes `imgui_impl_opengl3_loader` and stb headers distributed with Dear ImGui.
- **spdlog 1.17.0** — MIT — https://github.com/gabime/spdlog — tag `v1.17.0` — `79524ddd08a4ec981b7fea76afd08ee05f83755d`
  - Bundles fmt, which is also MIT: https://github.com/fmtlib/fmt
- **nlohmann/json 3.12.0** — MIT — https://github.com/nlohmann/json — tag `v3.12.0` — `65ee68451d8eb2b5f3a30b410476ab83deb3289b`
- **Catch2 3.16.1** — Boost Software License 1.0 — https://github.com/catchorg/Catch2 — tag `v3.16.1` — `5010924c4d8e912e64fd64a11819eecacf572c06`
  - Test builds only. Not linked into the `iris` application.

License texts for these packages are in `THIRD_PARTY_NOTICES.md`.

OpenGL is supplied by the graphics driver. It is not fetched as source.
