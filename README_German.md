[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Das Computerspiel [Blitzkrieg](https://wikipedia.org/wiki/Blitzkrieg_(video_game)) ist der erste Teil der legendären Echtzeit-Strategiespielserie, entwickelt von [Nival Interactive](http://nival.com/) und am 28. März 2003 veröffentlicht.

Das Spiel ist weiterhin auf [Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) und [GOG.com](https://www.gog.com/en/game/blitzkrieg_anthology) erhältlich.

Im Jahr 2025 wurde der Singleplayer-Quellcode des Spiels unter einer [speziellen Lizenz](LICENSE.md) freigegeben, die die kommerzielle Nutzung untersagt, aber vollständig für die Community des Spiels, für Bildungszwecke und Forschung geöffnet ist. 
Bitte lesen Sie die Bedingungen des [Lizenzvertrags](LICENSE.md) sorgfältig durch, bevor Sie ihn verwenden.

# Inhalt dieses Repositories
- `Data` – Spieldaten
- `Soft` und `Tools` – begleitende Entwickler-Tools
- `Versions` – kompilierte Versionen des Spiels, einschließlich Karteneditoren
- `Sources` – Quellcode und Werkzeuge

# Vorbereitung

Voraussetzungen:

- Windows 10 oder 11.
- CMake 4.2 oder neuer.
- Visual Studio 2026 mit der Workload **Desktopentwicklung mit C++** und einem Windows SDK.
- Git und [vcpkg](https://github.com/microsoft/vcpkg). Setzen Sie `VCPKG_ROOT` auf das vcpkg-Verzeichnis.
- FMOD Engine 2.01.x für Windows. FMOD wird separat lizenziert und ist nicht in diesem Repository enthalten.

Die Direct3D-Kompatibilitätsquellen sind unter `Sources/sdk` enthalten. Legen Sie das FMOD SDK in dieser Verzeichnisstruktur ohne Leerzeichen ab:

```text
Sources/sdk/FMOD/Include/fmod.hpp
Sources/sdk/FMOD/lib/x64/fmod_vc.lib
Sources/sdk/FMOD/lib/x64/fmod.dll
```

Führen Sie `build.bat` aus. Das vcpkg-Manifest installiert FFmpeg, libpng, libsquish, pugixml und zlib automatisch. Das mitgelieferte SDK unter `Sources/src/GameSpy` wird aus dem Quellcode gebaut; ein separater GameSpy-Download ist nicht erforderlich. Bink, STLPort, Stingray und das alte DirectX-8-SDK werden für das aktuelle Spielziel nicht benötigt.

Die Release-Datei wird nach `build/bin/Release` geschrieben. So verwenden Sie die Daten einer installierten Verkaufsversion:

```bat
build\bin\Release\Game.exe -datadir "C:\path\to\Blitzkrieg"
```

---

# Zusätzliche Werkzeuge

- Im Ordner **tools** befinden sich die beim Kompilieren verwendeten Hilfsprogramme.
- Ressourcen werden im **zip (deflate)**-Format gespeichert und mit **zip/unzip** gepackt bzw. entpackt.
- **Verwenden Sie kein pkzip** — es kürzt Dateinamen und verwendet nicht den Deflate-Algorithmus.
- Einige Daten werden manuell mit einem **XML-Editor** bearbeitet, da häufiges Bearbeiten nicht nötig war und ein eigener Editor nicht sinnvoll gewesen wäre.

---

# Dateien in `data`

Im Verzeichnis des Spiels, unter **data**, befinden sich Dateien, die manuell bearbeitet oder einfach abgelegt werden müssen:

- `sin.arr` — Binärdatei mit Sinustabelle (einfach ablegen, nicht bearbeiten).
- `objects.xml` — Verzeichnis der Spielobjekte (manuell bearbeiten).
- `consts.xml` — Spielkonstanten für Designer (manuell bearbeiten).
- `MusicSettings.xml` — Musik-Einstellungen (manuell bearbeiten).
- `partys.xml` — Länderdaten (welcher Trupp für Geschützbedienung, Modell des Fallschirmspringers usw.).

## Dateien in `medals`

Im Unterverzeichnis **medals** befinden sich, nach Ländern sortiert, die Dateien `ranks.xml`, die Dienstgrade und die erforderliche **Erfahrung** zum Erreichen enthalten.
