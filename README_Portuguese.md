[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

O jogo de computador [Blitzkrieg](https://wikipedia.org/wiki/Blitzkrieg_(video_game)) é o primeiro título da lendária série de jogos de estratégia em tempo real, desenvolvido pela [Nival Interactive](http://nival.com/) e lançado em 28 de março de 2003.

O jogo ainda está disponível na [Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) e [GOG.com](https://www.gog.com/en/game/blitzkrieg_anthology).

Em 2025, o código-fonte do modo de um jogador foi lançado sob uma [licença especial](LICENSE.md) que proíbe o uso comercial, mas está completamente aberto para a comunidade do jogo, educação e pesquisa.
Por favor, leia atentamente os termos do [acordo de licença](LICENSE.md) antes de usar.

# O que está neste repositório
- `Data` - dados do jogo
- `Soft` e `Tools` - ferramentas auxiliares de desenvolvimento
- `Versions` - versões compiladas do jogo, incluindo editores de mapas
- `Sources` - código-fonte e ferramentas

# Preparação

Requisitos:

- Windows 10 ou 11.
- CMake 4.2 ou mais recente.
- Visual Studio 2026 com a carga de trabalho **Desenvolvimento para desktop com C++** e um Windows SDK.
- Git e [vcpkg](https://github.com/microsoft/vcpkg). Defina `VCPKG_ROOT` para o diretório do vcpkg.
- FMOD Engine 2.01.x para Windows. O FMOD tem licença separada e não está incluído neste repositório.

Os fontes de compatibilidade Direct3D estão incluídos em `Sources/sdk`. Coloque o SDK do FMOD nesta estrutura sem espaços:

```text
Sources/sdk/FMOD/Include/fmod.hpp
Sources/sdk/FMOD/lib/x64/fmod_vc.lib
Sources/sdk/FMOD/lib/x64/fmod.dll
```

Execute `build.bat`. O manifesto vcpkg instala automaticamente FFmpeg, libpng, libsquish, pugixml e zlib. O SDK incluído em `Sources/src/GameSpy` é compilado a partir do código-fonte; não é necessário baixar o GameSpy separadamente. Bink, STLPort, Stingray e o antigo SDK do DirectX 8 não são necessários para o alvo atual do jogo.

O executável Release é gravado em `build/bin/Release`. Para usar os dados de uma instalação comercial:

```bat
build\bin\Release\Game.exe -datadir "C:\path\to\Blitzkrieg"
```

---

# Ferramentas adicionais

- O diretório **tools** contém utilitários usados durante o processo de compilação.
- Os recursos são armazenados no formato **zip (deflate)** e empacotados/desempacotados usando **zip/unzip**.
- **Não use pkzip** — ele corta nomes de arquivos e não utiliza o algoritmo deflate.
- Parte dos dados é editada manualmente usando um **editor de XML**, já que edições frequentes não eram necessárias e criar um editor separado não era viável.

---

# Arquivos em `data`

No diretório do jogo, na subpasta **data**, existem arquivos que são editados manualmente ou simplesmente colocados lá:

- `sin.arr` — arquivo binário com uma tabela de seno (apenas coloque, não altere).
- `objects.xml` — registro de objetos do jogo (editado manualmente).
- `consts.xml` — constantes do jogo para designers (editado manualmente).
- `MusicSettings.xml` — configurações de música (editado manualmente).
- `partys.xml` — dados dos países (qual squad usar para equipe de canhão, modelo de paraquedista, etc.).

## Arquivos em `medals`

Na subpasta **medals**, organizados por país, estão os arquivos `ranks.xml`, que contêm as patentes e a **experiência** necessária para obtê-las.
