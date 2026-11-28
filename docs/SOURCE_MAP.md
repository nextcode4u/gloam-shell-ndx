# Source map

Start at `arm9/source/main.c`, then follow the controller for the feature you
are changing. Public ARM9 interfaces live in `arm9/include`; shared hardware
protocol definitions live in `common/include`.

| Area | Main files |
| --- | --- |
| Frame loop and hardware | `arm9/source/main.c`, `arm7/source/main.c`, `common/include/hardware_protocol.h`. |
| App state and actions | `types.h`, `app.c`, `startup.c`, `storage_state.c`. |
| Library search | `library_search.c`, `ui/library_search_view.c`; optional PC index, bounded queries and keyboard/results UI. `browser.c` locates explicit results across folder batches. |
| Browser and navigation | `browser.c`, `directory_scan.c`, `entry_sort.c`, `tabs.c`, `grid.c`, `layout.h`, `paths.c`. |
| Settings and persistence | `settings.c`, `preferences.c`, `configuration.c`, `configuration_reset.c`, `appearance.c`. |
| Home and associations | `dashboard.c`, `collections.c`, `associations.c`, `association_editor.c`, `item_preferences.c`, `picker.c`. `inspection.c` and `ui/inspection_view.c` prepare/display read-only resolver and launch-preflight explanations. `associations.c` resolves suffix/folder specificity and deduplicates registered handlers; `picker.c` owns the explicit handler chooser and `launch.c` revalidates its selected profile. |
| Launch preparation | `launch.c`, `arguments.c`, `homebrew.c`, `bootstrap.c`, `dsiware.c`; `scummvm.c` adds configured native collection-discovery options for argument-free ScummVM launches. |
| Platform handoff | `launch_platform.c`, `loader_data.s`, `third_party/nds-bootloader/`. |
| Optional DSi Menu return tile | `forwarder/arm9/`, shared `system_routes.c`, `tools/build_forwarder.py`. |
| Native system titles | `system_catalog.c` reads built-in NAND and Launcher metadata; `installed_titles.c` cooperatively discovers and revalidates installed DSiWare; `system_routes.c` builds warmboot/autoload records. Slot-1 helper discovery lives in `launch.c`. |
| Game options, saves and cheats | `game_options.c`, `game_editor.c`, `game_save_table.c`, `cheats.c`, `cheat_editor.c`, `cheat_backend.c`. |
| Artwork | `artwork.c`, `artwork_cache.c`, `artwork_disk_cache.c`, `album_art.c`, `image_decode.c`, `image_input.c`, `image_job.c`. |
| Icons and title metadata | `icons.c`, `animated_icon.c`, `system_icons.c`, `slot1.c`, `title_metadata.c`; `file_types.c` classifies fallbacks and `ui/item_icons.c` draws their shared symbol family. System icons use discovered native banners with original built-in fallbacks. `slot1.c` reads cartridge metadata cooperatively through bounded hardware callbacks in `main.c`. |
| Themes, fonts and text | `themes.c`, `theme_backgrounds.c`, `theme_icons.c`, `theme_components.c`, `font_pack.c`, `text.c`, `ui_strings.c`, `ui/font*.h`. |
| Startup splashes | `splash.c` validates/draws prepared layers; `splash_preferences.c` resolves theme/custom/default packages and thumbnails. `main.c` owns boot checkpoints, skip/recovery and sound lifetime. The [theme directory](https://github.com/nextcode4u/gloam-shell-theme-directory) owns the converter and graphical builder; `tests/splash_fixture.py` supplies independent native test data. |
| Launcher audio and reports | `audio.c`, `diagnostics.c`. |
| Drawing and screen composition | `ui/drawing.c`, `ui/renderer.c`, and the individual `ui/*_view.c` files. |

Except where a path is stated, filenames in the table are under `arm9/source/`
or `arm9/include/`. Rendering consumes state: it does not access the filesystem,
change settings or perform hardware calls. The frame loop owns those operations.

Launch preflight also takes binary sizes from open-file metadata. Unknown-ID
homebrew detection reads ARM9 sequentially, retaining overlap bytes in memory
so DLDI records spanning blocks remain detectable without backwards seeks.
Header reads verify open-file metadata, read status and close results through
`launch_read_header`. Homebrew probing distinguishes an inspection failure from
a valid non-homebrew result; an I/O failure never selects a different backend.

Banner icons read size metadata from the open file, without seeking to EOF.
The browser publishes static icons first, then loads animation in a later idle
turn after visible static icons are ready. Corrupt or truncated animation retains
the static image. A bounded packed-banner cache retains visited native entries
in the current folder, using about 300 KiB instead of retaining every decoded
animation. Title names and installed-title identity checks also avoid EOF seeks,
keeping FAT cluster-chain traversal out of size validation.
Navigation reconnects known icon assignments from the bounded banner cache
without reads; refresh and SD removal clear it. New banners start on the first
released frame and take priority over other optional reads. Selected covers wait
only two released frames (one for a remembered source); metadata, neighboring
covers and autosaves retain their 12-frame delay. Held navigation starts no reads.
Fast held scrolling repeats every frame after a six-frame hold; Normal and Slow
retain their existing timings. `navigation.h` keeps value editors, tabs and
letter jumps at Normal repeat. Held navigation starts no optional reads.
Display-only item assignments share a four-entry cache; launch/edit checks stay uncached.
Pending artwork polls perform no source metadata reads; completion validates the
source before publishing the decoded cover. Icon scaling reuses a bounded
horizontal coordinate map instead of dividing in every pixel.
Per-entry artwork hints retain source precedence within the current folder
snapshot. Refreshes and assignment edits discard them; source pixels still get
metadata validation, and missing hinted images restart the full lookup chain.
Transient metadata read errors do not create hints.

Native title discovery publishes boot-content IDs only after complete metadata
reads and a successful close. Patched-menu and installed-title header checks use
open-file metadata and reject read/close errors before accepting a launch target.

`theme_assets.c` accepts legacy sprite bundles and prepared screen/material
records. `theme_prepared_surfaces.py` reproduces native integer blending at
authoring time; mismatched personal overrides and invalid bundles retain PNG
fallback. Temporary bundle data is released after populating the existing banks.

`tabs.c` resolves library/custom-tab browsing roots before preferences load.
Saved locations, directory entry handoff and scan startup enforce those roots;
Back stops at each assigned root. Files retains SD-wide browsing.

`tabs.c` also retains per-tab view, icon size and icon-label visibility. The
controller snapshots outgoing display state before changing tabs and restores
the incoming tab before scanning. `preferences.c` stores bounded `tab_display`
records by stable ID; legacy files seed all tabs from their global display values.

Theme navigation selects entries without decoding assets. X explicitly requests
preview through the shared cooperative decoder worker. Both preview tasks and
selected-cover decoding target roughly half a frame of work per slice.
Navigation and Cancel unwind superseded work; repeated X leaves the current load running. Preview banks
stay hidden until complete, and A only applies a ready preview. Returning to the
applied theme reuses its banks; X can reload it to support on-disk theme editing.

Directory and picker scans run in bounded steps. Picker Back restores its folder
across batches in one pass, with a first-batch fallback if that folder disappeared.
Empty saved browser batches restart at batch zero after library or filter changes.
Image jobs yield cooperatively and cancel when navigation changes. Selected overflow text uses the shared
pixel-clipped `draw_text_scrolling` helper: settings and tabs own their start
frames, while browser/picker views use selection-idle counters. Repetitions
follow each other through a three-space gap measured with the active font without stopping at the end. Reduced
motion keeps text static.

`settings.c` owns both settings catalogs: Simple references a bounded subset of
the same option definitions and handlers used by Advanced. Its app-aware catalog
functions enforce `simple_settings_lock` for controller navigation and rendering;
`preferences.c` persists the mode and config-only lock. Fresh installs use Simple,
while existing state files without a mode retain Advanced. Hidden technical
values are preserved rather than reset.

`paths.c` also owns the shared FAT-safe file replacement routine used by settings,
per-game options, cheats, item overrides, dashboard records and exports. It stops
on backup-rotation errors and preserves recovery copies across failed retries.

ScummVM's optional startup config is read completely before adding discovery
arguments. Missing configs permit ordinary startup; unreadable, truncated or
invalid configs stop preparation without changing the launch plan.

`bootstrap.c` preserves shared INI settings only after both input passes reach
EOF and close successfully. Replacement requires a regular current file or
confirmed absence; inspection failures preserve the current INI and its backup.

`audio.c` checks open-file metadata before reading WAVs, avoiding EOF seeks on
FAT. PCM reads, chunk boundaries and close results must pass before publishing
a new sample buffer; a failed load preserves the caller's clip.

Appearance presets, named collections and cheat selections verify the complete
opened record and close result before applying it. Incomplete reads and embedded
NUL bytes reject the record; presets and cheats may use a valid recovery copy.

`game_options_resolve` checks both defaults and title overrides before launch.
Missing records may inherit; unreadable or damaged layers require a valid backup.
Failed resolution leaves the caller's options unchanged.
`game_options_load` provides a verified sparse snapshot for editing.

`item_preferences_resolve` applies the same missing/error distinction to launch,
handler selection and inspection. `item_preferences_read_for_edit` additionally
requires inspecting the primary before assignment changes. Display-only artwork/icon
lookups retain their ordinary fallbacks through `item_preferences_load`.

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

Theme-directory browsing lives in `tools/gloam_setup_themes.ps1` (immutable
GitHub catalog/tree identity, bounded preview/runtime downloads, PC cache and
transactional installation) and `tools/gloam_setup_themes_ui.ps1` (WPF selection,
preview windows and worker completion). Preview/catalog jobs never write to SD
or reload pending editor controls. The worker installs complete validated packs;
release/bootstrap inventories include both modules.

The desktop settings backend is `tools/gloam_setup_settings.ps1`; its WPF
bindings are in `tools/gloam_setup_settings_ui.ps1`. It edits selected flat
state records while retaining repeated favourites/navigation and unknown keys,
checks the loaded revision before saving, and leaves the lock off by default.
Desktop view/icon/label edits update that field in every per-tab display record;
unrelated per-tab fields remain unchanged.
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
fingerprints and strongest-hash indexes, avoiding a full metadata scan for every
ROM; external XML resolution is disabled. It is compiled in memory by
Windows PowerShell and packaged as `gloam-dat.cs`. No external runtime is needed.

Library Index groups are cached in `App` and rebuilt by `grid.c` after directory
publication or displayed-title changes. They cover only the current batch;
rendering consumes counts without reading files. Selection changes reuse the
counts, and Index skips cover decoding.

Theme UI components are prepared in `theme_components.c`; `ui/theme_components.c`
clips and draws nine-slice images. They share the theme preview/apply/cancel
lifecycle with icons and never change layout or input bounds.
`theme_assets.c` validates an optional compressed sprite bundle only during
theme loading, copies its records into those normal caches and frees the temporary
data. `tools/pack_theme_assets.py` is the matching PNG authoring converter.
`theme_motion.c` owns the delayed, bounded ambient cache and frame scheduling;
`app_tick` advances it. Rendering only reads its current frame. Optional controls
and selection feedback use the existing clipped UI drawing helpers.
Version-2 coverage planes are retained only for components with translucent
pixels. Opaque components and native icons keep their direct drawing paths.
`font_pack.c` validates fixed-cell and proportional faces; `ui/drawing.c` shares
the same glyph advances for measurement, wrapping, clipping and looping.
Controllers select the body face; headings read their explicit package role.

Top-screen theme compositions live in `ui/renderer.c`: content and navigation
use independent offsets for left/right rails; Poster has a separate artwork/tab
composition and preserves the existing information and Index views. Wallpaper
strength and the reading-surface opacity are preblended by `theme_backgrounds.c`.
Theme, native and associated icons share each layout's preview footprint. Preview
images reserve authored frame borders; sidebar captions also reserve the active
font's full height inside their card. Browser-cell sizing stays independent.

Theme header labels and battery/disconnected-wireless sprites use prepared theme
state. `theme_components.c` maps cached battery readings to artwork states;
`ui/theme_components.c` draws those sprites. The update checker temporarily owns a network connection; idle artwork does not
infer signal strength or radio power from the wireless LED.

`tools/make_theme_scenes.py` prepares eleven independent scene compositions.
Scene layout in `ui/renderer.c` keeps art separate from the item inspector.
`theme_backgrounds.c` retains only a 94-line unshaded strip for each loaded Scene
package, alongside the ordinary shaded dialog background.

Tab presentation helpers in `ui/renderer.c` draw labels, icon docks/tiles, numbered
selectors and pagers from the existing tab model. Theme parsing validates bounded
style, highlight, gap and visible-count settings; rendering never changes navigation.

`tools/make_theme_icons.py` prepares independent per-theme icon atlases from
`assets/theme-art/*-icons.png`, including navigation objects at dock resolution.
`make_theme_navigation.py` supplies shared supersampling primitives for components.
The optional tab PNGs share the bounded theme icon cache, with independent fallback
to existing file-type symbols. `make_theme_icons.py` exports both asset families.

Release checks use `update_checker.c` for bounded HTTP/JSON parsing, numeric
version ordering, daily cache and scheduling gates. `update_transport.c` owns
nonblocking DNS/TCP/TLS operations and yields through a platform callback.
`main.c` owns delayed Wi-Fi initialization, the cooperative worker, cancellation
and radio disconnection before a launch or application-state reset.
The initialized SDK stack is retained: its 1.25.0 Internet teardown is unsupported. Rendering only reads
`UpdateState`. `update-available.png` is an optional status component.

`theme_composition.h` owns the bounded Spotlight, Gallery and Journal regions,
including heading clearance and configurable top/bottom navigation. Drawing and
geometry regression checks share those regions; browser touch geometry stays in
`layout.h`.

`splash.c` validates the prepared splash formats. Version 3 prepares independently
compressed frames through `image_decode_zlib_frame` before drawing, with one
reusable buffer per compressed layer. The source GIFs and conversion recipe for
the shared default live in `assets/`; its production package lives under
`sdroot/_nds/dsi-frontend/themes/default/`, without a `theme.ini`.
Version 2 keeps motion in
bounded per-layer metadata and uses fixed-point transforms with screen-local
clipping; it does not bake effects into frame sequences or allocate while drawing.
The contributor UI, project migration and matching preview stay in the theme repo.

Bundled font authoring uses `config/theme-fonts.json` and
`tools/theme_font_sources.py` for immutable, checksum-verified OFL sources.
`make_theme_samples.py --download-fonts` explicitly populates an ignored cache;
ordinary firmware builds consume the prepared per-theme DSFONT03 files.

`tools/theme_materials.py` prepares full-image material artwork from the original
`assets/theme-art/materials.png` studies. Scene generation bakes complete reading
bands into background PNGs. Optional component `material`/`focus_material` PNGs
are decoded into screen-anchored four-bit fields by `image_decode_material`.
`theme_components.c` owns up to two 24-KiB fields and prepared shade tables per
bank; `ui/theme_components.c` applies them to flat fills while preserving trim,
coverage and cutouts. Missing fields retain plain fills. Banks become visible
only after all sprites, fields and palettes finish preparation. Scene item
summaries use `panel.png`, with a solid fallback when absent.

`tools/theme_selections.py` owns each bundled pack's full focus silhouette and
trim. `make_theme_components.py` exports it into the existing selected-row/card
slots; `theme_materials.py` prepares separate focus samples. Runtime rendering
and touch geometry stay shared. `make_screenshots.py --theme-selections` compares
focused and ordinary rows through the host renderer at native size.

Update checks load their bounded PEM trust bundle in the main context after the
idle gate. The detached worker owns network/TLS work only and returns a prepared
tag; the main context publishes the result and writes the cache. Cancellation
joins that work before releasing its trust data and discards any pending result.
This keeps updater I/O from overlapping SD/NAND operations on SDKs with shared
disk-cache scratch storage. Image jobs use the main context's cooperative decoder
and yield between complete filesystem operations.
Diagnostics record read-only ARM9 cache/clock observations and the VBlank/browser
frame span of the last pending-image period, including cancelled work.

`boot_profile.c` collects bounded startup regions against a clock supplied by
`main.c`. ARM9 timers 0/1 are released after the first completed browser scan and
frame, or canceled before an earlier handoff/reload. Diagnostics export the observations on demand; startup writes no timing
file. Host tests inject a deterministic clock. Theme preparation includes splash
checkpoints, and splash-wait/folder regions can overlap; their values must not be
summed. Total time starts at frontend entry, excluding the previous loader's read
of BOOT.NDS. Boot profiles over 120 seconds are discarded to avoid timer wrap.

`tools/gloam_setup_health.ps1` owns read-only SD health rows, checked script-font/
catalog imports, scoped repair and human-readable diagnostic timings. The worker
writes health JSON into its PC job folder; the settings UI displays it without
reloading unsaved settings. Repair validates incoming assets before writes and
uses the common backup writer for replacements. New installer modules belong in
the add-on installer map, source inventory and bootstrap allow-list together.

Desktop search-index preparation lives in `tools/gloam_setup_search.ps1` and
`tools/gloam_library_index.cs`. The setup worker runs the cancellable metadata
scan; only a completed cache is installed. The GUI reads a small status header.
Native search allocates one page on demand, closes its stream on cancellation or
SD loss, and never opens the index during initialization. The format and limits
are in [Library search](guides/LIBRARY_SEARCH.md).

`top_content.c` prepares optional context, folder summaries and Home activity
after selection settles. `ui/top_content_view.c` consumes the snapshot and the
existing three-image Shelf cache; the renderer supplies its native/icon drawing
callback and shared tab dock. Theme content defaults and user overrides preserve
legacy compositions when omitted. Metadata sidecars are bounded UTF-8 records,
independent of launch settings; desktop authoring lives alongside artwork helpers.
