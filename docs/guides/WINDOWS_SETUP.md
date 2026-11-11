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
  apps, use Recommended, or use Select all to include every app. Install selected can fetch
  any app in this list, including apps also bundled in `addons.zip`. It fills missing files;
  Update selected replaces changed application binaries while preserving personal
  configurations and saves. Check updates reports differences first.
- **Check installed BIOS / support** checks every installed app, independently
  of the selection checkboxes, using the console installer's read-only checks.
  Hover an add-on row for its version, terms and firmware requirements.
  Missing proprietary firmware is reported with lookup paths and documented
  sizes; see [firmware requirements](ADDONS.md#free-replacements-and-remaining-firmware).

Noncommercial packages need the explicit no-profit-use checkbox. Experimental
apps are included by Select all and can also be selected individually; their
compatibility notes appear in the activity log. Download-only applications come directly from their upstream releases.
The GUI uses the existing downloader and reviewed catalog rather than a second
set of install rules. Downloads and setup operations run in a separate process;
the activity log shows progress and errors. If one app fails to download, verify
or stage, the remaining selected apps are still attempted. The final report names
installed/updated, preserved/current and failed apps; retry the failed selection.
The window reports **Completed with failures** instead of discarding the rest of
the batch. Files already staged for an incomplete app remain available for retry.

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

## ROM organizer

Choose **ROM organizer**, then one system folder such as `roms/Nintendo DS`.
Choose **A-Z / # folders** or **Back to system folder**, adjust the ROM extensions
if needed, and select **Preview moves**. Review the moves, reference updates and
skipped titles before selecting **Apply reviewed moves**. Close Gloam and any
emulator using this library before applying.

The organizer retains filenames and moves recognized adjacent saves, numbered
save slots, DSiWare public/private saves, patches and sidecar artwork with their
ROM. A title's matching files in a `saves/` subfolder retain that layout. It also
updates Gloam favourites, recent entries, selected-file paths, explicit artwork
and application references, and hashed per-title settings/cheat records.

Existing destinations are never overwritten or renamed around. A collision or
shared companion skips the whole title group. Multi-file game folders, BIOS,
support folders and system README files stay in place. Flatten reads only the
immediate `A`-`Z`/`#` buckets; it does not recursively unpack game folders. Empty
folders are retained. The SD root and Games library root cannot be organized.

**Undo selected** uses the recovery journal in
`/_nds/dsi-frontend/organizer/`. Undo requires the moved files' sizes/timestamps
and affected configuration to remain unchanged. After playing a game, use a
fresh flatten preview instead, so updated saves are retained. A stale preview
must be regenerated. Failed operations attempt rollback; keep the journal if
recovery is interrupted. Journals retain the original affected configuration.

Only recognized companions and Gloam references are updated. Arbitrary emulator
configurations, external shortcuts and `.argv` contents are not rewritten. Check
those separately when relocating an existing collection. The workflow is modeled
on [PowerShell File Organizer Scripts](https://github.com/nextcode4u/PowerShell-File-Organizer-Scripts),
with a separate ROM-aware implementation for the setup tool.

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

Select an application row on **Add-ons** to see its configured formats, default
content folders and BIOS guidance. The details distinguish built-in firmware,
optional replacements and user-supplied BIOS. Known BIOS paths are checked on the
selected SD folder and refreshed after operations. “Found” checks presence and
known sizes, not authenticity; firmware chosen inside an emulator may need checking
there. These formats describe the configured routes, not every upstream format.

The ROM organizer reuses the selected SD/staging folder. Its system dropdown
automatically lists folders under `roms` and any configured Games library,
including the default Atari subcollections. BIOS, saves and ScummVM game data
are excluded. Use **Other subfolder** only for a custom nested collection; the
browser opens at the current system folder. Preview remains required before moves.

**Open ROMs folder** on the organizer page opens `roms` on the selected SD/staging
folder in File Explorer, creating the folder if it does not exist yet.

When adding a folder tab, type its SD-relative path or use **Choose folder**.
The picker starts at the current folder when it exists, otherwise at the selected
SD root. Choosing a folder fills the path; **Save folder and tab** applies it.

The add-on list shows **Installation** and **BIOS** status for the selected card.
Installation checks nonempty executable files at the configured package paths;
it does not verify their version or guarantee every support file is installed.
BIOS status distinguishes embedded firmware, optional files, missing required
files and requirements for particular systems. Firmware selected within an
emulator must be checked there. Columns refresh after selecting a card and after
install, update or BIOS checks. Version and license information remain in the
selected-app details.

## Artwork

Choose **Artwork**, select a collection on the current SD card and click **Scan
folder**. Enable **Subfolders** for a recursive scan (up to 5,000 items; choose a
smaller collection for larger libraries). The list includes files and folders,
shows individual, shared folder and embedded MP3 art, and recognizes configured
association defaults. Save/configuration files and support directories are skipped.

- **Select missing** checks files without artwork. Check individual rows to include
  folders or replace a cover. Selecting a row shows its current image and prepared preview.
- **Import image** accepts PNG/JPEG for the selected row. **Download previews**
  fetches up to 50 checked items without writing to the SD card. Failed matches
  appear in the Result column and do not stop other downloads.
- **Download options** defaults to automatic system detection from the standard
  folder names (or DS file extension). DS uses its four-character header ID with
  [GameTDB](https://www.gametdb.com/DS/Downloads); other configured systems use the
  filename and [Libretro's box-art naming convention](https://github.com/libretro-thumbnails/libretro-thumbnails).
  Choose a system or correct the exact title for a single checked item when needed.
  Region/revision suffixes matter; DAT identification can resolve renamed files
  before the filename fallback. There is no fuzzy title guessing.
  DS cover language can be selected explicitly. A missing match is not an error
  in the ROM, and these services do not have covers for every title.
- A **direct HTTPS image URL** works for one checked item, including books, music
  or custom folders. Use the final PNG/JPEG URL; redirects and non-image pages are
  rejected. Music/book database searches are not provided. Existing MP3 APIC art
  is read using the same bounded ID3v2.3/v2.4 subset as the launcher.
- Review the previews, then **Apply checked previews**. Existing artwork is kept
  unless **Replace existing artwork** is enabled. Prepared PNGs retain aspect ratio
  and fit within 256 pixels and 1 MiB. Version-1 per-item records point to managed
  images under `/_nds/dsi-frontend/covers/`; application/icon preferences are retained.
- **Clear assignment** restores automatic lookup. It never deletes sidecars,
  embedded art, shared images or old managed covers. Previous item records are
  backed up under `setup-backups`; native `.bak` records are left intact.

Downloads send only a game ID/title to the selected provider, never ROM contents.
Images remain on the user's PC/SD card and are not included in Gloam releases.
Downloaded images keep a source-URL note beside the managed cover. Name-based
matches should be visually reviewed for the correct game, region and edition.
Restart Gloam or use Refresh metadata if an old cover is still cached.

### DAT-assisted cover identification

**Identify with DAT checksums** is enabled in Download options. DS title-ID lookup
still runs first. Other systems use their cached Libretro No-Intro DAT when
available; a missing cache is fetched on demand from
[libretro-database](https://github.com/libretro/libretro-database/tree/master/metadat/no-intro).
When that system has no No-Intro mirror, its Libretro TOSEC mirror is tried.
Computer-system covers also include Commodore 64, Amstrad CPC, ZX Spectrum, MSX/MSX2
and Atari 8-bit Family. If a DS ID has no artwork, the Nintendo DS DAT/name route is also tried.
**Update DAT** refreshes the selected system's mirror. A missing mirror does not
prevent ordinary filename lookup or use of imported metadata.

For [TOSEC DATs](https://www.tosecdev.org/downloads), No-Intro or Redump metadata,
extract the downloaded pack, choose the matching system in Download options and
use **Import DAT** for its `.dat`/`.xml` files. XML and clrmamepro text formats are
accepted. Files must contain ROM sizes and hashes. The cache is PC-only under
`%LOCALAPPDATA%/Gloam/Setup/databases`; DATs and ROMs are not bundled in releases.
Imported DATs are added alongside the mirror; originals remain unchanged.

Identification reads the local file and compares its size plus SHA-1, MD5 or CRC32
(preferred in that order per record). It never falls back to a weaker hash after a
stronger hash in that record disagrees. CRC32-only matches are labeled weaker;
ambiguous CRC names fall back to filename lookup. Hash-derived aliases are tried
before the filename, with up to eight metadata names per item. The Result/details
show whether the preview came from an ID, a DAT hash or filename matching.

A DAT identifies content; it does not provide a cover image. TOSEC and artwork
providers may use different names, so identification can succeed without finding
art. Stronger aliases from the Libretro mirror are preferred, since those names
are closer to its thumbnail names. Review every preview before applying it.
No ROM renaming, patching, archive extraction, region guessing or content upload
is performed. ZIP identification streams a single recognized ROM member without
extracting it. Archives with multiple ROM candidates, more than 256 members,
a ROM above 1 GiB or a compression ratio above 1000:1 are refused for identification;
ordinary filename lookup remains available. Other archive formats and multi-file
sets still need manual title/image selection.

Complete-file hashes are tried alongside explicitly labeled payload variants for
[iNES headers/trainers](https://www.nesdev.org/wiki/INES) and possible
[512-byte SNES copier headers](https://snes.nesdev.org/wiki/ROM_file_formats).
The payload must still match the DAT's size and hash; detecting a possible header
alone does not identify a game. No header is removed from the user's file.
Patches or other header formats may need a matching DAT or manual selection.
DATs are limited to 32 MiB/250,000 entries each and ROM identification to 1 GiB.
Metadata source notes remain beside the cache.

Artwork scans and preview downloads offer **Cancel artwork** while working.
Cancellation waits for the current file/request, retains completed previews,
and leaves SD artwork unchanged. Applying covers and other SD writes must finish
normally; they do not expose this cancellation control. Failed preview retries
clear the previous candidate and identification label.

### Inspecting theme packs

The Themes page previews a selected pack's 32×32 icons and reports missing or
malformed icons as built-in fallbacks. **Preview external theme** inspects a
folder without copying it. Imports reject unknown manifest fields, invalid
layout values and paths escaping the package before writing files; existing
packs are never overwritten. Missing optional images do not block import.
This desktop check is not a substitute for Gloam's font, wallpaper/audio decoder
and colour-contrast checks or viewing the pack on the console.
