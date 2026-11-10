# Windows setup tool

Open a normal Windows PowerShell window and run:

```powershell
irm https://raw.githubusercontent.com/nextcode4u/gloam-shell-ndx/main/setup.ps1 | iex
```

The command finds the current published Gloam release, checks `addons.zip`
against its release checksum, and opens Gloam Setup. Downloads and extracted
setup files stay in `%LOCALAPPDATA%\Gloam\Setup\releases\<version>`.
Windows PowerShell 5.1 and the Windows desktop are required; Python and
administrator privileges are not required.

Alternatively, extract the complete `addons.zip` and double-click
**addon-installer → Start Gloam Setup.cmd**. Keep the setup scripts, XAML,
`downloads.json` and shared downloader together. The existing console menus
and Python/PowerShell command-line installers remain available.

## Choose the destination

Choose the SD card's root folder, or an empty staging folder on your computer,
then select **Use folder**. All SD paths in the tool refer to that destination.
You can prepare a staging folder first and copy its contents to the SD later.
A DSi homebrew boot method with SD access is still required; see
[getting started](../GETTING_STARTED.md).

## Setup and updates

- **Install / repair setup** downloads the current checked `sd.zip`. It fills
  missing runtime files and replaces `BOOT.NDS`, while retaining existing
  configuration, themes, associations, BIOS, saves and other applications.
  The checked bundled add-on pack is included by default; uncheck it for a
  frontend-only setup.
- **Update Gloam + backends** refreshes the frontend, official nds-bootstrap,
  Slot-1 helper and forwarder/TMD. Existing settings, profiles, themes and saves
  are kept; replaced runtime files receive backups.
- **Update BOOT.NDS only** downloads the current frontend binary. Existing
  assets, backend versions, settings and content stay in place. Check release
  notes for updates that also require new runtime assets.
- **Add-ons** lists the complete reviewed application catalog. Choose individual
  apps, use Recommended, or select all regular apps. Install fills missing files;
  Update selected replaces changed application binaries while preserving personal
  configurations and saves. Check updates reports differences first.
- **Check installed BIOS / support** checks every installed app, independently
  of the selection checkboxes, using the console installer's read-only checks.
  Hover an add-on row for its version, terms and firmware requirements.
  Missing proprietary firmware is reported with lookup paths and documented
  sizes; see [firmware requirements](ADDONS.md#free-replacements-and-remaining-firmware).

Noncommercial packages need the explicit no-profit-use checkbox. Experimental
apps need individual selection; their compatibility notes appear in the activity
log. Download-only applications come directly from their upstream releases.
The GUI uses the existing downloader and reviewed catalog rather than a second
set of install rules. Downloads and setup operations run in a separate process;
the activity log shows progress and errors.

Changed frontend/configuration files receive unique backups under
`/_nds/dsi-frontend/setup-backups/`. Add-on binary updates use the downloader's
existing `%USERPROFILE%\.gloam-addons-cache\previous-files` backups.
Matching release licenses remain in the computer's release cache; source and
license downloads are linked from **Releases and licenses**. Keep the matching
companions when redistributing binaries.

## Launcher settings

The Settings page edits Simple/Advanced mode, the Simple settings lock, browser
view/icon size, sorting and scrolling, hidden/system/save/BIOS filters, covers,
internal game-title language, startup tab/recent countdown, theme/accent, reduced
motion, clock format, background music and music/UI sound volumes.

**Lock the console to Simple settings** hides the console's mode switch and
forces Simple mode. It is off by default. Clear the checkbox here and choose
Simple or Advanced to unlock it. This setting controls the console settings
pages; the PC can still manage the configuration.

Quick setup presets stage changes for review:

- **Everyday:** Simple, unlocked, Classic/small icons, normal scrolling and
  hidden support folders. Other personal choices are retained.
- **Simple locked:** locks Simple, starts at Home, filters recognized content,
  hides support/system files and disables recent auto-launch.
- **Quiet:** music/UI sounds off, reduced motion on and recent auto-launch off.
- **Large icons:** Classic view, large icons and slower scrolling.

Press **Save settings** to apply; **Reload from SD** discards pending edits.
Favourites, folder positions and unknown state records are retained. If the
state file changes outside the tool after loading, saving stops so you can
reload first. Background checks retain pending edits. Close Gloam/emulation
before PC edits and restart it afterwards.

Custom music can be selected from the SD or copied from the PC into
`media/music/`. It must match the launcher's format: mono PCM16 WAV, 8000-48000
Hz and at most 1 MiB of samples. Existing same-named files are kept.

## Backups and maintenance

Maintenance exports/imports the same `.dsconfig` format used by Gloam and the
Python configuration tool. The complete archive, checksums and destinations are
validated before import. Settings, favourites, tabs, appearance slots, per-title
preferences and personal extension overrides are portable; emulator/player profile definitions in `plugins/*.ini`,
applications, themes, music, BIOS and saves are separate. Save pending settings
before exporting or importing.

Imports use the launcher's `configuration/pending.dsconfig` and
`configuration/rollback.dsconfig` recovery pair. A terminated PC import can be
rolled back on the next launcher startup. Later external edits are preserved
rather than overwritten by automatic recovery. See [configuration recovery](CONFIGURATION_BACKUPS.md).

**Restore last console settings** validates `state.bak` and restores it after
confirmation; the replaced file receives its own setup backup. **Check setup**
is read-only and reports missing loaders, invalid settings, pending recovery,
missing themes and optional emulator paths. Firmware checks remain on Add-ons.

## Library and file handlers

The Library page can edit Games, Apps and Media roots; defaults are `roms`,
`apps` and `media`. Existing tabs and unrelated configuration fields are retained.
Changing a root creates its folder but does not move an existing library.

Choose an existing handler or **New emulator / player**. Enter a name, file
extensions, application and preferred content folder, then save. A PC-local
homebrew executable chosen through Browse / import is copied into
`/_nds/dsi-frontend/plugins/apps/`; an existing same-named application is kept.
Advanced options expose a separate DS-mode application, nds-bootstrap and the
homebrew argv-file option. Existing icon, cover and other metadata are retained.
The launcher currently supports 32 profile files and 42 custom folder tabs.

The preferred folder is created when saving the handler. For shared extensions,
its `content_root` rule lets Gloam choose that handler inside the folder and offer
its normal resolver elsewhere. Existing per-file and personal defaults retain
priority. See [association rules](EMULATORS.md#shared-extensions-and-folder-rules).
Use **Copy ROMs / media into folder** to copy selected files into this content
folder in the background; existing same-named files are kept.

The folder-tab editor creates or changes a named tab and its directory. Stable
IDs preserve existing tab identities. After changing folders, tabs or handlers,
restart Gloam on the console so it reloads the configuration.

## Themes

Open the themes folder or import a folder containing `theme.ini`. Import keeps
existing themes; rename the source folder to install a separate version. Choose
the active theme on the console, or select an installed theme and press **Apply
selected theme**. The optional layout checkbox also applies its suggested
browser view and icon size. Save/reload pending settings first; the console
validates the full package when it next loads. Theme compatibility is enforced by Gloam when
loading it; copying a folder does not establish compatibility.

For local/offline preparation, the GUI accepts `-Offline -ReleaseDirectory <folder>`
with a complete, previously downloaded release set and `SHA256SUMS`. Add-on files
must also be present in the shared downloader cache. Online setup/update actions
look up the current published release rather than keeping an older cached version.
