# SD card layout

Gloam coexists with established DS homebrew folders. The `_nds/dsi-frontend`
folder was established early in development, before the Gloam name was chosen.
We kept it after naming the project so existing settings, themes, artwork,
associations and loader paths would continue to work without moving user files.
References to it in configuration JSON and source code describe the active SD
layout; they are intentional compatibility paths.

```text
/
├── BOOT.NDS
├── _nds/
│   ├── dsi-frontend/
│   │   ├── state.ini             preferences and navigation
│   │   ├── tabs.ini              custom folder tabs
│   │   ├── themes/               palette/font/background/audio packs
│   │   ├── plugins/              argv association profiles
│   │   │   ├── apps/             emulator/player executables and companion files
│   │   │   └── startup/scummvm.ini optional native collection discovery
│   │   ├── covers/               optional artwork
│   │   ├── cache/                disposable thumbnails
│   │   ├── loaders/slot1launch.nds
│   │   └── install/              optional DSi Menu forwarder
│   ├── nds-bootstrap-release.nds
│   └── nds-bootstrap-hb-release.nds
├── _gba/                         application-owned support files
├── apps/Homebrew Apps/           utilities and applications
├── roms/
│   ├── Homebrew Games/
│   ├── Nintendo DS/
│   ├── DSiWare/
│   ├── ScummVM/
│   │   ├── scummvm.nds           optional native launcher from add-on installer
│   │   └── titles/              one complete game-data folder per title
│   └── ...                       system collections
├── media/
│   ├── books/
│   ├── music/
│   └── videos/
└── data/                         application-owned data
```

Games defaults to `/roms`, Apps to `/apps`, and Media to `/media`. These tabs
and custom folder tabs stay inside their assigned trees; Back stops at the
assigned root. Configure library roots in `[roots]` in `tabs.ini`. Root `/music`
and arbitrary existing folders remain accessible. [Custom tabs](TABS.md) can
point anywhere on SD. Folder names do not select an emulator automatically;
[profiles](EMULATORS.md) associate extensions and applications.

The [release builder](../development/BUILDING.md) produces `dist/sd.zip` with
backend/helper executables. The checked-in template contains only authored
assets/configuration and empty collection placeholders. No games, BIOS, NAND or
user saves are included. Cartridge icons are read directly; no SD icon file is
needed. Preserve other launchers' directories and each emulator's support files.

`python3 tools/stage_sd.py` stages the template and frontend locally without
downloading backends. It preserves existing assets/configuration while replacing
`BOOT.NDS`; use a dedicated output directory. Release extraction should likewise
preserve personal settings and existing libraries.
