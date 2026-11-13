# Current support

Gloam Shell NDX is a development project. The browser, settings and launch adapters
are implemented; physical DSi validation remains pending. A listed route is not
a guarantee for every title, boot method or application build.

| Area | Support and limits |
| --- | --- |
| Browser | Classic, Extended, List, Grid and a centered horizontal cover Shelf and Library Index; small/large icons, folder tabs, touch, looping labels, native banners, custom icons and covers. |
| Themes | Versioned palette/font packs, wallpapers, distinct icon packs with native-banner priority, modular nine-slice UI components, audio cues/music, bounded layout defaults and personal presets. Fixed-cell ASCII/Latin-1 faces with shared Latin Extended-A, Greek and Cyrillic fallback; Partial UTF-8 interface catalogs cover navigation, settings labels/help, common values, sections and overlay headings/controls with English fallback and a Spanish settings catalog; CJK/RTL and full UI localization remain future work. |
| Homebrew | Native DSi-SD handoff and argv; return requires a compatible application. Flashcart/DLDI and DS-mode behavior are not general compatibility claims. |
| Retail DS | nds-bootstrap adapter, options, saves and cheats. The unmodified official backend has reached retail gameplay, its menu, Quit, warm relaunch and cold save readback in a development emulator composition. This does not establish stock-emulator or physical-console compatibility. The packaged official binary contains no private experimental patches. |
| SD DSiWare | Separate nds-bootstrap adapter with public/private save preparation and short aliases. Broad title, private-save and return compatibility remain open. Repeated in-game-menu entry for Dr. Mario Express can leave black screens in the development emulator composition; reliable return to Gloam with the bundled official backend remains unconfirmed. |
| Installed DSiWare | Read-only NAND discovery, localized banners and native title requests; needs an appropriate boot environment. No installation or NAND save manipulation. |
| System apps | Native Settings, Camera, Sound, PictoChat and Download Play requests, plus system-menu return. Available titles, warm launches and wireless behavior depend on the environment. |
| Slot-1 | Cartridge banner/title discovery and external helper launch. Boot-chain SCFG/key availability can limit icons; physical cards remain unverified. |
| Add-ons | Shared Python/PowerShell installers, reviewed update pins, and read-only BIOS/support-file checks. GameYob argv launch, state save, cold state restore and Quit-to-launcher pass development-emulator checks with the tested build/title. Experimental ScummVM remains an explicit selection; its dynamic Lure and Beneath a Steel Sky save, cold restore and Quit/return routes pass development-emulator checks, with broader engines and physical devices unverified. |
| Save/return | No universal return hook. Backends and applications own their save/exit contracts; configuration alone does not establish successful persistence. |
| Home collections | Up to eight named shortcut groups sharing 32 targets; desktop editing, Home/back navigation and configuration interchange. No content moves or invented playtime. |
| Configuration | On-console editors, checked imports/exports, scoped reset, early recovery and diagnostics. |

The release script packages unmodified upstream nds-bootstrap and Slot-1 Launcher
releases identified in [the dependency lock](../config/release-dependencies.json).
Hash verification establishes artifact identity, not launch compatibility.

Before a stable release, validate boot performance, display readability, peak
memory, sleep, SD removal, power controls, audio/microphone, cartridges and
wireless on physical DSi hardware. Broader title and save/return coverage is also
needed. Arbitrary dashboard widgets, 3D/coverflow and CJK/RTL fonts are
potential future work, not supported package features.

Theme packages support right/left navigation rails, artwork-led Poster and
illustrated Scene layouts. Navigation can use labels, numbered tabs, icon docks,
icon tiles or a category pager, with independent tab artwork. Full-strength
wallpapers, a preblended reading surface, optional minimal chrome and separate
selected-label/tab text colors are supported. See [theme packages](guides/THEME_PACKAGES.md).

### Background release notices

Gloam can check the latest published GitHub release (including previews) using
the console's saved Wi-Fi configuration. Checks are enabled by default and
can be disabled in Settings. They start only in DSi mode, at least 30 seconds
after startup and after two seconds without selection movement. No network
connection is initialized during startup or in recovery mode. A successful
check is cached for 24 hours; offline failures are quiet and get no automatic
retry in the same session. Settings can queue another check.

A status-bar arrow means a newer version is available. Settings shows its tag;
use the PC setup tool or the project's GitHub Releases page to update. Nothing
is automatically installed or launched. Missing trust certificates, incorrect
console dates, network errors and oversized/unsupported responses fail closed.
The cache is disposable `_nds/dsi-frontend/update-cache.txt`.

The transport uses nonblocking sockets, a 96 KiB response ceiling and 512-byte
read steps, yielding between TLS handshake steps. Network waiting is bounded
by a 45-second deadline after SDK initialization; the radio disconnects and
TLS/response buffers are released afterward and before launching a title.
The SDK network context remains allocated for reuse because BlocksDS 1.25.0
does not support tearing down its Internet stack. SDK initialization/shutdown
still relies on the ARM7 service responding; this is not a hard real-time bound.
Cryptographic steps still consume CPU, and adding network libraries increases ROM size. Zero boot-time or frame-time cost is not
claimed: real DSi timings, WPA2 connectivity and power use need hardware checks.

Preference-save failures identify the operation and filesystem error on the
console (for example, opening `state.tmp` or rotating `state.ini`). Failed backup
rotation stops the save; failed installation attempts restore the previous
state, and a lone `state.bak` recovery copy survives retries. A failed save still
blocks launching rather than silently losing changes. These checks do not
establish the cause of an individual SD write failure without its error details.
