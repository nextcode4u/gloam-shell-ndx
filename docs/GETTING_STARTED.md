# Setup and controls

## Install or upgrade

Build with the [build guide](development/BUILDING.md), then merge `dist/sd.zip`
onto a backed-up SD card. `/BOOT.NDS` is the entry point. Keep existing libraries,
saves and application support files; replace Gloam's assets deliberately without
replacing the whole `_nds` folder or your edited configuration.

The package includes pinned official nds-bootstrap and Slot-1 Launcher builds.
Other emulators/players, their BIOS files and games are separate. The supplied
[association profiles](guides/EMULATORS.md) can be edited on the console or in
INI files to match an existing installation.

A homebrew boot method with DSi SD access is required. Unlaunch is needed for
routes that use its title/autoload support; it is not installed or configured
by Gloam. Read [current support](CURRENT_SUPPORT.md) before relying on a launch route.

| Library | Default folder |
| --- | --- |
| Games / homebrew games | `/roms/` / `/roms/Homebrew Games/` |
| Applications / utilities | `/apps/` / `/apps/Homebrew Apps/` |
| Media | `/media/books/`, `/media/music/`, `/media/videos/` |
| Frontend settings and assets | `/_nds/dsi-frontend/` |

These are defaults. Existing root `/music` and arbitrary folders remain accessible.
The legacy `dsi-frontend` asset namespace is intentional for compatibility.

## Controls

| Control | Action |
| --- | --- |
| D-pad | Navigate; Left/Right adjusts settings |
| A / B | Open or confirm / back |
| L / R | Change tab or settings page |
| START | Frontend settings |
| SELECT | Item actions |
| Y | Overview / details |
| X | Focus the tab rail |

Touch selects an entry; tap again to open. Overflow text loops while selected,
unless Reduced motion is enabled. Frontend settings are separate from Nintendo
DSi System Settings in the System tab.

## Recovery and diagnostics

Hold SELECT at startup to bypass external appearance, audio and automatic launch.
Recovery preserves normal saved preferences. Maintenance provides configuration
export/import and scoped resets. Exports exclude games, saves and source artwork.

`/_nds/dsi-frontend/launch-report.txt` records the last launch preparation;
Maintenance can export diagnostics. A successful handoff does not prove a title
started. Clearing the artwork cache never removes source images or game saves.
