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
the active theme on the console. Theme compatibility is enforced by Gloam when
loading it; copying a folder does not establish compatibility.

For local/offline preparation, the GUI accepts `-Offline -ReleaseDirectory <folder>`
with a complete, previously downloaded release set and `SHA256SUMS`. Add-on files
must also be present in the shared downloader cache. Online setup/update actions
look up the current published release rather than keeping an older cached version.
