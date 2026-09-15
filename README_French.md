[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

[![Bande-annonce de Blitzkrieg](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Le jeu vidéo [Blitzkrieg](https://wikipedia.org/wiki/Blitzkrieg_(video_game)) est le premier opus de la légendaire série de jeux de stratégie en temps réel, développé par [Nival Interactive](http://nival.com/) et sorti le 28 mars 2003.

Le jeu est toujours disponible sur [Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) et [GOG.com](https://www.gog.com/en/game/blitzkrieg_anthology).

En 2025, le code source du mode solo du jeu a été publié sous une [licence spéciale](LICENSE.md) qui interdit l'utilisation commerciale mais reste totalement ouverte à la communauté du jeu, à l'éducation et à la recherche.
Veuillez lire attentivement les termes du [contrat de licence](LICENSE.md) avant toute utilisation.

# Contenu de ce dépôt
- `Data` - données du jeu
- `Soft` et `Tools` - outils et utilitaires pour le développement
- `Versions` - versions compilées du jeu, y compris les éditeurs de cartes
- `Sources` - code source et outils

# Préparation

Prérequis :

- Windows 10 ou 11.
- CMake 4.2 ou version ultérieure.
- Visual Studio 2026 avec la charge de travail **Développement Desktop en C++** et un SDK Windows.
- Git et [vcpkg](https://github.com/microsoft/vcpkg). Définissez `VCPKG_ROOT` sur le dossier de vcpkg.
- FMOD Engine 2.01.x pour Windows. FMOD possède une licence distincte et n'est pas inclus dans ce dépôt.

Les sources de compatibilité Direct3D sont incluses dans `Sources/sdk`. Placez le SDK FMOD dans cette arborescence sans espaces :

```text
Sources/sdk/FMOD/Include/fmod.hpp
Sources/sdk/FMOD/lib/x64/fmod_vc.lib
Sources/sdk/FMOD/lib/x64/fmod.dll
```

Exécutez `build.bat`. Le manifeste vcpkg installe automatiquement FFmpeg, libpng, libsquish, pugixml et zlib. Le SDK `Sources/src/GameSpy` inclus est compilé depuis ses sources ; aucun téléchargement GameSpy supplémentaire n'est nécessaire. Bink, STLPort, Stingray et l'ancien SDK DirectX 8 ne sont plus requis pour la cible actuelle du jeu.

L'exécutable Release est créé dans `build/bin/Release`. Pour utiliser les données d'une installation commerciale :

```bat
build\bin\Release\Game.exe -datadir "C:\path\to\Blitzkrieg"
```

---

# Outils supplémentaires

- Le répertoire **tools** contient des utilitaires utilisés lors de la compilation.
- Les ressources sont stockées au format **zip (deflate)** et sont compressées/décompressées à l'aide de **zip/unzip**.
- **N'utilisez pas pkzip** — il tronque les noms de fichiers et n'utilise pas l'algorithme deflate.
- Certaines données sont éditées manuellement via un **éditeur XML**, car des modifications fréquentes n'étaient pas nécessaires et l'écriture d'un éditeur séparé n'était pas judicieuse.

---

# Fichiers dans `data`

Dans le répertoire du jeu, sous **data**, se trouvent les fichiers qui sont édités manuellement ou simplement placés :

- `sin.arr` — fichier binaire avec une table de sinus (à placer tel quel, ne pas toucher).
- `objects.xml` — registre des objets du jeu (édité manuellement).
- `consts.xml` — constantes du jeu pour les designers (édité manuellement).
- `MusicSettings.xml` — paramètres de musique (édité manuellement).
- `partys.xml` — données des pays (quel squad utiliser pour l’équipage des canons, quel modèle de parachutiste, etc.).

## Fichiers dans `medals`

Dans le sous-répertoire **medals**, les fichiers `ranks.xml` contiennent les grades et l'**expérience** requise pour les obtenir, organisés par pays.
