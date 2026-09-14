[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Компьютерная игра [Blitzkrieg](https://wikipedia.org/wiki/Blitzkrieg_(video_game)) это первая часть легендарной серии военных стратегий в реальном времени, разработанная [Nival Interactive](http://nival.com/) и выпущенная 28 марта 2003 года.

Игра до сих пор доступна в [Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) и [GOG.com](https://www.gog.com/en/game/blitzkrieg_anthology).

В 2025 году исходный код одиночной игры был открыт под [специальной лицензией](LICENSE.md), запрещающей коммерческое использование, но полностью открытой для сообщества игры, образования и исследований. 
Перед использованием внимательно ознакомьтесь с условиями [лицензионного соглашения](LICENSE.md).

# Что находится в этом репозитории
- `Data` - данные игры
- `Soft` и `Tools` - сопутствующие инструменты для разработки
- `Versions` - собранные версии игры, тут же и редакторы карт
- `Sources` - исходный код и инструменты

# Подготовка

Требования:

- Windows 10 или 11.
- CMake 4.2 или новее.
- Visual Studio 2026 с компонентами **Разработка классических приложений на C++** и Windows SDK.
- Git и [vcpkg](https://github.com/microsoft/vcpkg). Задайте в `VCPKG_ROOT` путь к vcpkg.
- FMOD Engine 2.01.x для Windows. FMOD лицензируется отдельно и не входит в этот репозиторий.

Исходный код совместимости Direct3D включён в `Sources/sdk`. Разместите FMOD SDK по следующим путям без пробелов:

```text
Sources/sdk/FMOD/Include/fmod.hpp
Sources/sdk/FMOD/lib/x86/fmod_vc.lib
Sources/sdk/FMOD/lib/x86/fmod.dll
```

Запустите `build.bat`. Манифест vcpkg автоматически установит FFmpeg, libpng, libsquish, pugixml и zlib. Включённый в репозиторий SDK `Sources/src/GameSpy` собирается из исходного кода; отдельно загружать GameSpy не нужно. Bink, STLPort, Stingray и старый DirectX 8 SDK для текущей цели игры не требуются.

Исполняемый файл Release создаётся в `build/bin/Release`. Чтобы использовать данные установленной розничной версии игры:

```bat
build\bin\Release\Game.exe -datadir "C:\path\to\Blitzkrieg"
```

---

# Дополнительные инструменты

- В директории **tools** находятся утилиты, используемые при сборке.
- Ресурсы хранятся в формате **zip (deflate)** и упаковываются/распаковываются с помощью **zip/unzip**.
- **Не используйте pkzip** — он обрезает имена файлов и не использует алгоритм deflate.
- Часть данных редактируется вручную через **XML-редактор**, так как частое редактирование не требовалось, а писать отдельный редактор было нецелесообразно.

---

# Файлы в `data`

В директории игры, в поддиректории **data**, находятся файлы, редактируемые вручную или требующие простого размещения:

- `sin.arr` — бинарный файл с таблицей синусов (просто положить, не трогать).
- `objects.xml` — реестр игровых объектов (редактируется вручную).
- `consts.xml` — игровые константы для дизайнеров (редактируется вручную).
- `MusicSettings.xml` — настройки музыки (редактируется вручную).
- `partys.xml` — данные по странам (какой squad использовать для gun crew, какую модель парашютиста и т. д.).

## Файлы в `medals`

В поддиректории **medals**, по странам, расположены файлы `ranks.xml`, содержащие звания и **experience** для их получения.
