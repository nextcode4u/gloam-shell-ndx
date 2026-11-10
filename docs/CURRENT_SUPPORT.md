# Current support

Gloam Shell NDX is a development project. The browser, settings and launch adapters
are implemented; physical DSi validation remains pending. A listed route is not
a guarantee for every title, boot method or application build.

| Area | Support and limits |
| --- | --- |
| Browser | Classic, Extended, List and Grid; small/large icons, folder tabs, touch, looping labels, native banners, custom icons and covers. |
| Themes | Versioned palette/font packs, wallpapers, audio cues/music, bounded layout defaults and personal presets. Fixed-cell ASCII/Latin-1 faces with shared Latin Extended-A, Greek and Cyrillic fallback; CJK/RTL and full UI localization remain future work. |
| Homebrew | Native DSi-SD handoff and argv; return requires a compatible application. Flashcart/DLDI and DS-mode behavior are not general compatibility claims. |
| Retail DS | nds-bootstrap adapter, options, saves and cheats. The unmodified official backend has reached retail gameplay, its menu, Quit, warm relaunch and cold save readback in a development emulator composition. This does not establish stock-emulator or physical-console compatibility. The packaged official binary contains no private experimental patches. |
| SD DSiWare | Separate nds-bootstrap adapter with public/private save preparation and short aliases. Broad title, private-save and return compatibility remain open. |
| Installed DSiWare | Read-only NAND discovery, localized banners and native title requests; needs an appropriate boot environment. No installation or NAND save manipulation. |
| System apps | Native Settings, Camera, Sound, PictoChat and Download Play requests, plus system-menu return. Available titles, warm launches and wireless behavior depend on the environment. |
| Slot-1 | Cartridge banner/title discovery and external helper launch. Boot-chain SCFG/key availability can limit icons; physical cards remain unverified. |
| Save/return | No universal return hook. Backends and applications own their save/exit contracts; configuration alone does not establish successful persistence. |
| Configuration | On-console editors, checked imports/exports, scoped reset, early recovery and diagnostics. |

The release script packages unmodified upstream nds-bootstrap and Slot-1 Launcher
releases identified in [the dependency lock](../config/release-dependencies.json).
Hash verification establishes artifact identity, not launch compatibility.

Before a stable release, validate boot performance, display readability, peak
memory, sleep, SD removal, power controls, audio/microphone, cartridges and
wireless on physical DSi hardware. Broader title and save/return coverage is also
needed. Arbitrary dashboard widgets, 3D/coverflow and CJK/RTL fonts are
potential future work, not supported package features.
