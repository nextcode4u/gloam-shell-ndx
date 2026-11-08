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
| Home and associations | `dashboard.c`, `associations.c`, `association_editor.c`, `item_preferences.c`, `picker.c`. |
| Launch preparation | `launch.c`, `arguments.c`, `homebrew.c`, `bootstrap.c`, `dsiware.c`. |
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

Directory and picker scans run in bounded steps. Image jobs yield cooperatively
and cancel when navigation changes. Selected overflow text uses the shared
pixel-clipped `draw_text_scrolling` helper: settings and tabs own their start
frames, while browser/picker views use selection-idle counters. Repetitions
follow each other through a three-cell gap without stopping at the end. Reduced
motion keeps text static.

Configuration import and reset retain rollback data before changing settings.
Do not replace them with recursive deletion of frontend folders: installed
assets and game saves are outside the reset scope.

## Tools and assets

| Path | Purpose |
| --- | --- |
| `sdroot/` | Reviewed distributable SD template. |
| `config/emulator-presets.json` | Default association and ROM-folder catalog. |
| `config/package-inputs.json` | Explicit source/SD package allowlist. |
| `tools/` | Maintained build, authoring and packaging tools. |
| `tests/` | Model/render sanitizer checks, synthetic fixtures and Python tool tests. |
| `third_party/` | Required dependency sources and notices; see [attribution](../THIRD_PARTY.md). |

Use [building](development/BUILDING.md), [SDK integration](development/TOOLCHAIN.md)
and [contribution conventions](../CONTRIBUTING.md) when developing. Contracts and
source details are indexed in [the documentation index](README.md).
