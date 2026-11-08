# SD card layout

Gloam coexists with established DS homebrew folders. Its compatibility asset
namespace remains `_nds/dsi-frontend`, independent of the project name.

```text
/
├── BOOT.NDS
├── _nds/
│   ├── dsi-frontend/
│   │   ├── state.ini             preferences and navigation
│   │   ├── tabs.ini              custom folder tabs
│   │   ├── themes/               palette/font/background/audio packs
│   │   ├── plugins/              argv association profiles
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
│   └── ...                       system collections
├── media/
│   ├── books/
│   ├── music/
│   └── videos/
└── data/                         application-owned data
```

Games defaults to `/roms`, Apps to `/apps`, and Media to `/media`. Root `/music`
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
