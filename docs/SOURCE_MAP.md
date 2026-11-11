# Source map

Start at `arm9/source/main.c`, then follow the controller for the feature you
are changing. Public ARM9 interfaces live in `arm9/include`; shared hardware
protocol definitions live in `common/include`.

| Area | Main files |
| --- | --- |
| Frame loop and hardware | `arm9/source/main.c`, `arm7/source/main.c`, `common/include/hardware_protocol.h`. |
| App state and actions | `types.h`, `app.c`, `startup.c`, `storage_state.c`. |
| Browser and navigation | `browser.c`, `directory_scan.c`, `entry_sort.c`, `tabs.c`, `grid.c`, `layout.h`, `paths.c`. |
| Settings and persistence | `settings.c`, `preferences.c`, `configuration.c`, `configuration_reset.c`, `appearance.c`. |
| Home and associations | `dashboard.c`, `associations.c`, `association_editor.c`, `item_preferences.c`, `picker.c`. `associations.c` resolves suffix/folder specificity and deduplicates registered handlers; `picker.c` owns the explicit handler chooser and `launch.c` revalidates its selected profile. |
| Launch preparation | `launch.c`, `arguments.c`, `homebrew.c`, `bootstrap.c`, `dsiware.c`; `scummvm.c` adds configured native collection-discovery options for argument-free ScummVM launches. |
| Platform handoff | `launch_platform.c`, `loader_data.s`, `third_party/nds-bootloader/`. |
| Optional DSi Menu return tile | `forwarder/arm9/`, shared `system_routes.c`, `tools/build_forwarder.py`. |
| Native system titles | `system_catalog.c` reads built-in NAND and Launcher metadata; `installed_titles.c` cooperatively discovers and revalidates installed DSiWare; `system_routes.c` builds warmboot/autoload records. Slot-1 helper discovery lives in `launch.c`. |
| Game options, saves and cheats | `game_options.c`, `game_editor.c`, `game_save_table.c`, `cheats.c`, `cheat_editor.c`, `cheat_backend.c`. |
| Artwork | `artwork.c`, `artwork_cache.c`, `artwork_disk_cache.c`, `album_art.c`, `image_decode.c`, `image_input.c`, `image_job.c`. |
| Icons and title metadata | `icons.c`, `animated_icon.c`, `system_icons.c`, `slot1.c`, `title_metadata.c`; `file_types.c` classifies fallbacks and `ui/item_icons.c` draws their shared symbol family. System icons use discovered native banners with original built-in fallbacks. `slot1.c` reads cartridge metadata cooperatively through bounded hardware callbacks in `main.c`. |
| Themes, fonts and text | `themes.c`, `theme_backgrounds.c`, `font_pack.c`, `text.c`, `ui/font*.h`. |
| Launcher audio and reports | `audio.c`, `diagnostics.c`. |
| Drawing and screen composition | `ui/drawing.c`, `ui/renderer.c`, and the individual `ui/*_view.c` files. |

Except where a path is stated, filenames in the table are under `arm9/source/`
or `arm9/include/`. Rendering consumes state: it does not access the filesystem,
change settings or perform hardware calls. The frame loop owns those operations.

`tabs.c` resolves library/custom-tab browsing roots before preferences load.
Saved locations, directory entry handoff and scan startup enforce those roots;
Back stops at each assigned root. Files retains SD-wide browsing.

Directory and picker scans run in bounded steps. Image jobs yield cooperatively
and cancel when navigation changes. Selected overflow text uses the shared
pixel-clipped `draw_text_scrolling` helper: settings and tabs own their start
frames, while browser/picker views use selection-idle counters. Repetitions
follow each other through a three-cell gap without stopping at the end. Reduced
motion keeps text static.

`settings.c` owns both settings catalogs: Simple references a bounded subset of
the same option definitions and handlers used by Advanced. Its app-aware catalog
functions enforce `simple_settings_lock` for controller navigation and rendering;
`preferences.c` persists the mode and config-only lock. Fresh installs use Simple,
while existing state files without a mode retain Advanced. Hidden technical
values are preserved rather than reset.

Configuration import and reset retain rollback data before changing settings.
Do not replace them with recursive deletion of frontend folders: installed
assets and game saves are outside the reset scope.

## Tools and assets

| Path | Purpose |
| --- | --- |
| `sdroot/` | Reviewed distributable SD template. |
| `config/emulator-presets.json` | Default association and ROM-folder catalog. |
| `config/addon-dependencies.json` | Audited add-on releases, argv/BIOS evidence and distribution decisions. |
| `tools/prepare_addons_release.py` | Upstream release preflight, compatible pin refresh, validation and release build. |
| `tools/build_addons.py` | Permitted binary overlay and download-manifest collector; matching bundled source/licenses. |
| `tools/download_addons.py`, `tools/download_addons.ps1`, `tools/start_addon_installer.cmd` | Interactive menus and equivalent CLI downloaders/updaters; reviewed catalog refresh, file checks and backups; narrowly scoped reader fonts and inline profile defaults preserve existing customizations. |
| `tools/make_emulator_presets.py` | Generates profiles and per-system ROM-folder format/BIOS readmes. The DSLibris profiles are also preserved as install-if-missing defaults in the add-on catalog. |
| `config/package-inputs.json` | Explicit source/SD package allowlist. |
| `tools/verify_release.py`, `.github/workflows/` | Complete artifact-set verification, CI checks and reviewed draft-prerelease preparation. |
| `tools/` | Maintained build, authoring and packaging tools. |
| `tests/` | Model/render sanitizer checks, synthetic fixtures and Python tool tests. |
| `third_party/` | Required dependency sources and notices; see [attribution](../THIRD_PARTY.md). |

Use [building](development/BUILDING.md), [SDK integration](development/TOOLCHAIN.md)
and [contribution conventions](../CONTRIBUTING.md) when developing. Contracts and
source details are indexed in [the documentation index](README.md).

Windows SD setup lives in `setup.ps1` (checked-release bootstrap),
`tools/gloam_setup.ps1` / `.xaml` (WPF interface),
`tools/gloam_setup_core.ps1` (SD operations and configuration), and
`tools/gloam_setup_worker.ps1` (background process). The worker delegates add-on
downloads/updates to the existing PowerShell installer. Selected installs use a
snapshot of the full GUI catalog (`updates`), including bundled apps; the default
console `applications` list contains only the separate downloads. Packaging gives these
files hyphenated names under `addon-installer/`. Native Windows fixture tests
cover installation, preservation, path validation, GUI navigation and workers.

The desktop settings backend is `tools/gloam_setup_settings.ps1`; its WPF
bindings are in `tools/gloam_setup_settings_ui.ps1`. It edits selected flat
state records while retaining repeated favourites/navigation and unknown keys,
checks the loaded revision before saving, and leaves the lock off by default.
`tools/gloam_setup_configuration.ps1` uses the same DSCFG001 codec, whitelist
and pending/rollback archives as the native importer. Its CRC32 helper is
authored C# compiled in memory, with no additional installed dependency.

ROM organization lives in `tools/gloam_setup_organizer.ps1` (preview planner,
companion grouping, Gloam identity migration and journaled apply/undo), with WPF
bindings in `tools/gloam_setup_organizer_ui.ps1`. Previews stay on the PC; apply
revalidates the full plan before writing an SD recovery journal and moving files.
Filename collisions skip whole title groups, without renaming or overwriting.

Artwork management lives in `tools/gloam_setup_artwork.ps1` (bounded scans,
ID3 APIC reads, image conversion, provider URLs and sparse item assignments), with
WPF bindings in `tools/gloam_setup_artwork_ui.ps1`. The setup worker runs scans,
download preparation and batch apply; results are returned per item so one
failure does not abort later items. Downloads/imports prepare PC-side candidates;
apply revalidates their hashes and uses backed-up item writes without deleting
original or shared artwork. Providers are read-only, on-demand sources, not build
inputs. Windows tests use authored content and image fixtures.

`tools/gloam_setup_identification.ps1` manages PC-side DAT imports, on-demand
Libretro metadata and cover-name fallbacks. `tools/gloam_dat.cs` is an authored
.NET helper for bounded XML/clrmamepro parsing and streaming size/SHA-1/MD5/CRC32
fingerprints; external XML resolution is disabled. It is compiled in memory by
Windows PowerShell and packaged as `gloam-dat.cs`. No external runtime is needed.
