# Setup and controls

## Install or upgrade

Build with the [build guide](development/BUILDING.md), then merge `dist/sd.zip`
onto a backed-up SD card. `/BOOT.NDS` is the entry point. Keep existing libraries,
saves and application support files; replace Gloam's assets deliberately without
replacing the whole `_nds` folder or your edited configuration.

For a frontend-only update, copy the standalone release `BOOT.NDS` to the SD
root instead of extracting `sd.zip`. This preserves your settings, themes,
backends and libraries. Check release notes for required asset/backend updates.
Each release also supplies `licenses.zip`, a combined `source.zip` and checksums.
The SD ZIP contains runtime files only. Documentation is available in this
repository and the source archive; licenses and credits accompany the download
in `licenses.zip` and do not need to be copied onto the SD card.

The package includes pinned official nds-bootstrap and Slot-1 Launcher builds.
The optional [add-on overlay](guides/ADDONS.md) supplies the reviewed subset of
emulators/players; merge `addons.zip` after `sd.zip` if wanted. Keep customized
shim INI files when upgrading. Other applications, proprietary BIOS files and
games are separate. The supplied
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
| Y (release) | Overview / details |
| Hold Y + Up / Down | Jump between initial-letter groups in the current folder batch |
| X | Focus the tab rail |

Touch selects an entry; tap again to open. Overflow text loops while selected,
unless Reduced motion is enabled. Frontend settings are separate from Nintendo
DSi System Settings in the System tab.

Choose **Shelf** under START → Appearance → Browser view for a horizontal shelf
with the selected cover centered between its neighbors. Left/right moves one
item, up/down skips three, and L/R switches tabs. Covers and missing-cover icons
keep a consistent size; Reduced motion disables the horizontal slide. The launcher remembers eight recently visited
folder positions during the session; Back returns to the folder you entered.
Current per-tab locations still persist across restarts.


## Simple and Advanced settings

Press START to open settings. **Simple** has four pages: Appearance, Browsing,
Audio and Console. **Advanced** retains all seven pages, including emulator
profiles, game defaults, boot setup, configuration import/reset and diagnostics.
L/R cycles through the pages available in the current mode.

Choose **Appearance → Settings mode** to switch. In Advanced mode this row is at
the end of Appearance. Switching views preserves existing settings, including
technical choices hidden by Simple mode. New installations start in Simple mode;
existing state files without a mode entry keep the previous Advanced view.

For a console you are setting up for someone else, the [Windows setup tool](guides/WINDOWS_SETUP.md#launcher-settings)
provides a Simple-mode lock checkbox and a Simple locked preset. You can also close Gloam and edit
`/_nds/dsi-frontend/state.ini` on the SD card, keeping its other entries:

```ini
settings_mode=simple
simple_settings_lock=1
```

If the file does not exist yet, create it with `version=1` on the first line,
followed by these entries.

The lock forces Simple mode even if `settings_mode=advanced` is stored, and
removes the mode switch from the UI. It survives normal preference saves and
recovery startup. To unlock, close Gloam and change `simple_settings_lock` back
to `0`. The lock is **off by default**; saved preferences include
`simple_settings_lock=0` until explicitly configured otherwise.

This limits Gloam's frontend settings screen; it does not lock the SD card,
per-title menus, Nintendo system apps or other applications. Appearance and
ordinary browsing/audio controls remain available while locked.

## Recovery and diagnostics

Hold SELECT at startup to bypass external appearance, audio and automatic launch.
Recovery preserves normal saved preferences. Maintenance provides configuration
export/import and scoped resets. Exports exclude games, saves and source artwork.

`/_nds/dsi-frontend/launch-report.txt` records the last launch preparation;
Maintenance can export diagnostics. A successful handoff does not prove a title
started. Clearing the artwork cache never removes source images or game saves.

### Library Index

Choose **Index** under START → Appearance → Browser view, or use `view=index`
in a theme. The bottom screen shows a title list with small (16-pixel) or large (32-pixel)
icons and matching row spacing; the top
screen shows folders and alphabetical groups. Left/Right jumps to the first
item in the next populated group, wrapping past empty groups. Up/Down browses
normally; L/R changes tabs and Y switches to item details.

Groups count the current directory batch, not the entire SD card. Existing
sorting stays intact. Common Latin-1 accented initials join their base letter;
other scripts and punctuation appear under `*` (Other). Genre metadata is not
required. Index honors the icon-size setting and theme row padding.
