[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Game komputer [Blitzkrieg](https://wikipedia.org/wiki/Blitzkrieg_(video_game)) adalah seri pertama dari rangkaian game strategi perang waktu nyata legendaris, dikembangkan oleh [Nival Interactive](http://nival.com/) dan dirilis pada 28 Maret 2003.

Game ini masih tersedia di [Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) dan [GOG.com](https://www.gog.com/en/game/blitzkrieg_anthology).

Pada tahun 2025, kode sumber mode pemain tunggal game ini dirilis di bawah [lisensi khusus](LICENSE.md) yang melarang penggunaan komersial, namun sepenuhnya terbuka untuk komunitas game, pendidikan, dan riset.
Silakan baca dengan cermat syarat-syarat [perjanjian lisensi](LICENSE.md) sebelum menggunakannya.

# Isi repositori ini
- `Data` - data game
- `Soft` dan `Tools` - alat-alat pengembangan terkait
- `Versions` - versi game yang sudah dikompilasi, termasuk editor peta
- `Sources` - kode sumber dan alat-alatnya

# Persiapan

Persyaratan:

- Windows 10 atau 11.
- CMake 4.2 atau lebih baru.
- Visual Studio 2026 dengan workload **Desktop development with C++** dan Windows SDK.
- Git dan [vcpkg](https://github.com/microsoft/vcpkg). Atur `VCPKG_ROOT` ke direktori vcpkg.
- FMOD Engine 2.01.x untuk Windows. FMOD memiliki lisensi terpisah dan tidak disertakan dalam repositori ini.

Sumber kompatibilitas Direct3D disertakan di `Sources/sdk`. Letakkan FMOD SDK dengan susunan direktori tanpa spasi berikut:

```text
Sources/sdk/FMOD/Include/fmod.hpp
Sources/sdk/FMOD/lib/x86/fmod_vc.lib
Sources/sdk/FMOD/lib/x86/fmod.dll
```

Jalankan `build.bat`. Manifest vcpkg akan memasang FFmpeg, libpng, libsquish, pugixml, dan zlib secara otomatis. SDK `Sources/src/GameSpy` yang disertakan dibangun dari sumber; GameSpy tidak perlu diunduh terpisah. Bink, STLPort, Stingray, dan SDK DirectX 8 lama tidak diperlukan untuk target game saat ini.

Executable Release dibuat di `build/bin/Release`. Untuk memakai data dari instalasi game retail:

```bat
build\bin\Release\Game.exe -datadir "C:\path\to\Blitzkrieg"
```

---

# Alat Tambahan

- Direktori **tools** berisi utilitas yang digunakan selama proses build.
- Resource disimpan dalam format **zip (deflate)** dan dikemas/diekstrak menggunakan **zip/unzip**.
- **Jangan gunakan pkzip** — pkzip akan memotong nama file dan tidak menggunakan algoritma deflate.
- Beberapa data diedit manual menggunakan **editor XML**, karena pengeditan yang sering tidaklah diperlukan dan membuat editor terpisah dianggap tidak efisien.

---

# File di `data`

Pada direktori game, di subdirektori **data**, terdapat file-file yang diedit manual atau hanya perlu diletakkan:

- `sin.arr` — file biner dengan tabel sinus (cukup diletakkan, jangan diutak-atik).
- `objects.xml` — registry objek game (diedit manual).
- `consts.xml` — konstanta game untuk desainer (diedit manual).
- `MusicSettings.xml` — pengaturan musik (diedit manual).
- `partys.xml` — data negara (squad apa yang digunakan untuk kru senjata, model penerjun, dsb.).

## File di `medals`

Di subdirektori **medals**, per negara, terdapat file `ranks.xml` yang berisi daftar pangkat dan jumlah **experience** yang dibutuhkan untuk memperolehnya.
