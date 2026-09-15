[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

The computer game [Blitzkrieg](https://wikipedia.org/wiki/Blitzkrieg_(video_game)) is the first installment of the legendary series of real-time strategy war games, developed by [Nival Interactive](http://nival.com/) and released on March 28, 2003.

The game is still available on [Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) and [GOG.com](https://www.gog.com/en/game/blitzkrieg_anthology).

In 2025, the game's singleplayer source code was released under a [special license](LICENSE.md) that prohibits commercial use but is completely open for the game's community, education and research.
Please review the terms of the [license agreement](LICENSE.md) carefully before using it.

# What is in this repository
- `Data` - game data
- `Soft` and `Tools` - development tools
- `Versions` - compiled versions of the game, including map editors
- `Sources` - source code and tools

# Preparation

Requirements:

- Windows 10 or 11.
- CMake 4.2 or newer.
- Visual Studio 2026 with the **Desktop development with C++** workload and a Windows SDK.
- Git and [vcpkg](https://github.com/microsoft/vcpkg). Set `VCPKG_ROOT` to the vcpkg checkout.
- FMOD Engine 2.01.x for Windows. FMOD is separately licensed and is not included in this repository.

The Direct3D compatibility sources are included under `Sources/sdk`. Place the FMOD SDK files in this no-whitespace layout:

```text
Sources/sdk/FMOD/Include/fmod.hpp
Sources/sdk/FMOD/lib/x64/fmod_vc.lib
Sources/sdk/FMOD/lib/x64/fmod.dll
```

Run `build.bat`. The vcpkg manifest installs FFmpeg, libpng, libsquish, pugixml, and zlib automatically. The bundled `Sources/src/GameSpy` SDK is built from source; no separate GameSpy download is needed. Bink, STLPort, Stingray, and the legacy DirectX 8 SDK are not required for the current game target.

The Release executable is written to `build/bin/Release`. To use an installed copy of the retail data:

```bat
build\bin\Release\Game.exe -datadir "C:\path\to\Blitzkrieg"
```

---

# Additional Tools

- The **tools** directory contains utilities used during the build process.
- Resources are stored in **zip (deflate)** format and are packed/unpacked using **zip/unzip**.
- **Do not use pkzip** — it truncates file names and does not use the deflate algorithm.
- Some data is edited manually using an **XML-editor**, as frequent editing was not necessary and writing a separate editor was impractical.

---

# Files in `data`

In the game's directory, under **data**, there are files that are manually edited or simply placed:

- `sin.arr` — binary file with a sine table (just place it, do not touch).
- `objects.xml` — registry of game objects (edited manually).
- `consts.xml` — game constants for designers (edited manually).
- `MusicSettings.xml` — music settings (edited manually).
- `partys.xml` — country data (which squad to use for gun crew, parachutist model, etc.).

## Files in `medals`

In the **medals** subdirectory, files `ranks.xml` contain ranks and **experience** needed to obtain them, organized by country.
