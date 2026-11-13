#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host

common_flags=(
    -std=c11 -D_DEFAULT_SOURCE -DHOST_PREVIEW -DFRONTEND_DECODE_PROGRESS
    -DLODEPNG_NO_COMPILE_ENCODER -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
    -Iarm9/include -Ithird_party/lodepng -Ithird_party/stb
)
model_sources=(
    arm9/source/app.c arm9/source/update_checker.c arm9/source/startup.c arm9/source/appearance.c arm9/source/diagnostics.c arm9/source/configuration.c arm9/source/configuration_reset.c arm9/source/title_metadata.c arm9/source/text.c arm9/source/ui_strings.c arm9/source/cheats.c arm9/source/cheat_editor.c arm9/source/cheat_backend.c arm9/source/game_options.c arm9/source/game_editor.c arm9/source/game_save_table.c arm9/source/bootstrap.c arm9/source/dsiware.c arm9/source/storage_state.c arm9/source/grid.c arm9/source/audio.c arm9/source/browser.c arm9/source/file_types.c arm9/source/directory_scan.c arm9/source/entry_sort.c arm9/source/paths.c arm9/source/tabs.c arm9/source/settings.c arm9/source/arguments.c arm9/source/launch.c arm9/source/scummvm.c arm9/source/homebrew.c arm9/source/system_routes.c arm9/source/system_catalog.c arm9/source/installed_titles.c arm9/source/picker.c arm9/source/item_preferences.c arm9/source/dashboard.c arm9/source/collections.c arm9/source/association_editor.c arm9/source/themes.c arm9/source/theme_backgrounds.c arm9/source/theme_icons.c arm9/source/theme_components.c arm9/source/font_pack.c
    arm9/source/preferences.c arm9/source/artwork.c arm9/source/artwork_cache.c arm9/source/artwork_disk_cache.c arm9/source/image_decode.c arm9/source/image_job.c arm9/source/image_input.c arm9/source/album_art.c arm9/source/icons.c arm9/source/slot1.c arm9/source/system_icons.c arm9/source/animated_icon.c arm9/source/associations.c third_party/lodepng/lodepng.c
)
ui_sources=(arm9/source/ui/theme_components.c arm9/source/ui/item_icons.c arm9/source/ui/cheat_view.c arm9/source/ui/storage_view.c arm9/source/ui/game_view.c arm9/source/ui/renderer.c arm9/source/ui/drawing.c arm9/source/ui/settings_view.c arm9/source/ui/picker_view.c arm9/source/ui/theme_view.c arm9/source/ui/association_view.c arm9/source/ui/dashboard_view.c)
cc "${common_flags[@]}" -O2 "${model_sources[@]}" \
    "${ui_sources[@]}" tools/preview.c -o build/host/preview
# Reuse sanitized application objects across suites. Each run recompiles them,
# so header/config changes cannot accidentally reuse an older test model.
mkdir -p build/host/check-objects
model_objects=()
ui_objects=()
for source in "${model_sources[@]}" "${ui_sources[@]}"; do
    object="build/host/check-objects/${source//\//_}.o"
    cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined -c "$source" -o "$object"
    if [[ "$source" == arm9/source/ui/* ]]; then
        ui_objects+=("$object")
    else
        model_objects+=("$object")
    fi
done
cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/model_test.c -o build/host/model-test

fixture=$(mktemp -d /tmp/gloam-shell-ndx-test.XXXXXX)
trap 'rm -rf "$fixture"' EXIT
mkdir -p "$fixture/roms/Folder" "$fixture/media/books" "$fixture/media/music" "$fixture/media/videos" "$fixture/_nds/dsi-frontend" "$fixture/_gba"
printf 'Universal artwork fixture. Not a ROM.\n' > "$fixture/roms/Artwork demo.txt"
cp "tests/fixtures/cover-example.png" "$fixture/roms/Artwork demo.txt.png"
ASAN_OPTIONS=detect_leaks=1 build/host/model-test "$fixture"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" \
    tests/icons_test.c -o build/host/icons-test
mkdir "$fixture/icon-test"
cp -r sdroot/_nds/dsi-frontend/themes/copper/icons "$fixture/icon-test/"
ASAN_OPTIONS=detect_leaks=1 build/host/icons-test "$fixture/icon-test"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/slot1_test.c -o build/host/slot1-test
ASAN_OPTIONS=detect_leaks=1 build/host/slot1-test

python3 - "$fixture/presets" <<'PY'
import json
import shutil
import sys
from pathlib import Path

root = Path(sys.argv[1])
shutil.copytree("sdroot", root)
catalog = json.loads(Path("config/emulator-presets.json").read_text())
with (root / "expected.tsv").open("w") as output:
    for mode in (0, 1):
        for profile in catalog["profiles"]:
            settings = profile["settings"]
            application = settings.get("dsipath" if mode else "ntrpath", settings["path"])
            for extension in profile["extensions"]:
                folder = settings.get("content_root", "").rstrip("/")
                content = (folder + "/" if folder else "") + "Content with spaces." + extension.upper()
                output.write(f"{mode}\t{content}\t{application}\n")
PY
cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/presets_test.c -o build/host/presets-test
ASAN_OPTIONS=detect_leaks=1 build/host/presets-test "$fixture/presets" "$fixture/presets/expected.tsv"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/association_resolver_test.c -o build/host/association-resolver-test
mkdir "$fixture/association-resolver"
ASAN_OPTIONS=detect_leaks=1 build/host/association-resolver-test "$fixture/association-resolver"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" \
    tests/tabs_test.c -o build/host/tabs-test
mkdir "$fixture/tabs"
ASAN_OPTIONS=detect_leaks=1 build/host/tabs-test "$fixture/tabs"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" \
    tests/settings_test.c -o build/host/settings-test
mkdir "$fixture/settings"
ASAN_OPTIONS=detect_leaks=1 build/host/settings-test "$fixture/settings"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/launch_test.c -o build/host/launch-test
mkdir "$fixture/launch"
ASAN_OPTIONS=detect_leaks=1 build/host/launch-test "$fixture/launch"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/scummvm_test.c -o build/host/scummvm-test
mkdir "$fixture/scummvm"
ASAN_OPTIONS=detect_leaks=1 build/host/scummvm-test "$fixture/scummvm"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/picker_test.c -o build/host/picker-test
mkdir -p "$fixture/picker/roms/images"
cp "tests/fixtures/cover-example.png" "$fixture/picker/roms/images/Good.png"
ASAN_OPTIONS=detect_leaks=1 build/host/picker-test "$fixture/picker"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/themes_test.c -o build/host/themes-test
mkdir -p "$fixture/themes/_nds/dsi-frontend/themes/zzzbad"
cp -r sdroot/_nds/dsi-frontend/themes/copper sdroot/_nds/dsi-frontend/themes/slate "$fixture/themes/_nds/dsi-frontend/themes/"
printf '[theme]\nversion=1\n[colors]\ntext=#0c1018\n' > "$fixture/themes/_nds/dsi-frontend/themes/zzzbad/theme.ini"
ASAN_OPTIONS=detect_leaks=1 build/host/themes-test "$fixture/themes"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/theme_samples_test.c -o build/host/theme-samples-test
mkdir "$fixture/theme-samples"
cp -r sdroot/. "$fixture/theme-samples/"
ASAN_OPTIONS=detect_leaks=1 build/host/theme-samples-test "$fixture/theme-samples"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/association_dashboard_test.c -o build/host/association-dashboard-test
mkdir "$fixture/association-dashboard"
ASAN_OPTIONS=detect_leaks=1 build/host/association-dashboard-test "$fixture/association-dashboard"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/audio_grid_test.c -o build/host/audio-grid-test
mkdir -p "$fixture/audio-grid/roms" "$fixture/audio-grid/media/music"
for number in {1..17}; do touch "$fixture/audio-grid/roms/Entry $number.txt"; done
python3 - "$fixture/audio-grid/media/music/loop.wav" <<'PYFIXTURE'
import sys
from pathlib import Path
sys.path.insert(0, "tools")
from make_audio_samples import test_music_loop
Path(sys.argv[1]).write_bytes(test_music_loop())
PYFIXTURE
ASAN_OPTIONS=detect_leaks=1 build/host/audio-grid-test "$fixture/audio-grid" "$fixture/audio-grid/media/music/loop.wav"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/album_art_test.c -o build/host/album-art-test
python3 tools/make_artwork_test_fixtures.py "$fixture/album-art"
ASAN_OPTIONS=detect_leaks=1 build/host/album-art-test "$fixture/album-art"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/bootstrap_test.c -Wl,--wrap=stat -o build/host/bootstrap-test
mkdir "$fixture/bootstrap"
ASAN_OPTIONS=detect_leaks=1 build/host/bootstrap-test "$fixture/bootstrap"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/artwork_cache_test.c -o build/host/artwork-cache-test
mkdir "$fixture/artwork-cache"
ASAN_OPTIONS=detect_leaks=1 build/host/artwork-cache-test "$fixture/artwork-cache"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/storage_state_test.c -o build/host/storage-state-test
mkdir "$fixture/storage-state"
ASAN_OPTIONS=detect_leaks=1 build/host/storage-state-test "$fixture/storage-state"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/cheats_test.c -o build/host/cheats-test
python3 tools/make_cheat_test_fixture.py "$fixture/cheats"
ASAN_OPTIONS=detect_leaks=1 build/host/cheats-test "$fixture/cheats"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/animated_icon_test.c -o build/host/animated-icon-test
python3 tools/make_banner_test_fixture.py "$fixture/animated.nds"
ASAN_OPTIONS=detect_leaks=1 build/host/animated-icon-test "$fixture/animated.nds"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/system_catalog_test.c -o build/host/system-catalog-test
mkdir "$fixture/system-catalog"
ASAN_OPTIONS=detect_leaks=1 build/host/system-catalog-test "$fixture/system-catalog"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/text_test.c -o build/host/text-test
ASAN_OPTIONS=detect_leaks=1 build/host/text-test

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/title_metadata_test.c -o build/host/title-metadata-test
mkdir "$fixture/title-metadata"
ASAN_OPTIONS=detect_leaks=1 build/host/title-metadata-test "$fixture/title-metadata"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/background_test.c -o build/host/background-test
mkdir -p "$fixture/background/media"
cp tests/fixtures/cover.jpg "$fixture/background/media/cover.jpg"
ASAN_OPTIONS=detect_leaks=1 build/host/background-test "$fixture/background"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/browser_jobs_test.c -o build/host/browser-jobs-test
mkdir "$fixture/browser-jobs"
ASAN_OPTIONS=detect_leaks=1 build/host/browser-jobs-test "$fixture/browser-jobs"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/configuration_test.c -o build/host/configuration-test
mkdir "$fixture/configuration"
PYTHONPATH=tools python3 - "$fixture/configuration/python.dsconfig" <<'PYCODE'
import sys
from pathlib import Path
from native_configuration import encode_records
Path(sys.argv[1]).write_bytes(encode_records({"state.ini": b"version=1\ntheme=1\n"}))
PYCODE
ASAN_OPTIONS=detect_leaks=1 build/host/configuration-test "$fixture/configuration"
PYTHONPATH=tools python3 - "$fixture/configuration/_nds/dsi-frontend/configuration/export.dsconfig" <<'PYCODE'
import sys
from pathlib import Path
from native_configuration import decode_records
records = decode_records(Path(sys.argv[1]).read_bytes())
assert records["state.ini"] == b"version=1\ntheme=0\n"
assert "never-export.sav" not in records
PYCODE

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/custom_icons_test.c -o build/host/custom-icons-test
mkdir "$fixture/custom-icons"
python3 - "$fixture/custom-icons/Icon.png" <<'PYCODE'
import struct, sys, zlib
from pathlib import Path
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
rows = b"".join(b"\0" + b"".join(bytes((255, 0, 0, 0 if x == y == 0 else 255)) for x in range(32)) for y in range(32))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 32, 32, 8, 6, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
Path(sys.argv[1]).write_bytes(png)
PYCODE
ASAN_OPTIONS=detect_leaks=1 build/host/custom-icons-test "$fixture/custom-icons"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/dsiware_test.c -o build/host/dsiware-test
mkdir "$fixture/dsiware"
ASAN_OPTIONS=detect_leaks=1 build/host/dsiware-test "$fixture/dsiware"
if command -v fsck.fat >/dev/null; then
    for volume in "$fixture/dsiware"/volume-*.pub; do
        fsck.fat -n "$volume" >> build/host/dsiware-fsck.log
    done
fi

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/startup_test.c -o build/host/startup-test
mkdir "$fixture/startup"
ASAN_OPTIONS=detect_leaks=1 build/host/startup-test "$fixture/startup"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/picker_jobs_test.c -o build/host/picker-jobs-test
mkdir "$fixture/picker-jobs"
ASAN_OPTIONS=detect_leaks=1 build/host/picker-jobs-test "$fixture/picker-jobs"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    arm9/source/entry_sort.c tests/entry_sort_test.c -o build/host/entry-sort-test
ASAN_OPTIONS=detect_leaks=1 build/host/entry-sort-test

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/homebrew_test.c -o build/host/homebrew-test
mkdir "$fixture/custom-homebrew"
ASAN_OPTIONS=detect_leaks=1 build/host/homebrew-test "$fixture/custom-homebrew"

cc "${common_flags[@]}" -DFRONTEND_ASYNC_TEST -O2 "${model_sources[@]}" \
    tests/artwork_async_test.c -o build/host/artwork-async-test
build/host/artwork-async-test tests/fixtures/async.png tests/fixtures/async-progressive.jpg

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/configuration_reset_test.c -o build/host/configuration-reset-test
mkdir "$fixture/configuration-reset"
ASAN_OPTIONS=detect_leaks=1 build/host/configuration-reset-test "$fixture/configuration-reset"

cc "${common_flags[@]}" -Iforwarder/arm9/include -O1 -g -fsanitize=address,undefined \
    arm9/source/system_routes.c forwarder/arm9/source/target.c tests/forwarder_test.c \
    -o build/host/forwarder-test
ASAN_OPTIONS=detect_leaks=1 build/host/forwarder-test

cc "${common_flags[@]}" -Iarm9/source/ui -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/file_types_test.c -o build/host/file-types-test
ASAN_OPTIONS=detect_leaks=1 build/host/file-types-test

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/installed_titles_test.c -o build/host/installed-titles-test
mkdir "$fixture/installed-titles"
ASAN_OPTIONS=detect_leaks=1 build/host/installed-titles-test "$fixture/installed-titles"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/ui_strings_test.c -o build/host/ui-strings-test
mkdir "$fixture/ui-strings"
ASAN_OPTIONS=detect_leaks=1 build/host/ui-strings-test "$fixture/ui-strings"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" "${ui_objects[@]}" tests/collections_test.c -o build/host/collections-test
mkdir "$fixture/collections"
ASAN_OPTIONS=detect_leaks=1 build/host/collections-test "$fixture/collections"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    arm9/source/update_checker.c arm9/source/paths.c tests/update_checker_test.c \
    -Wl,--wrap=rename -o build/host/update-checker-test
mkdir -p "$fixture/update-checker/_nds/dsi-frontend"
ASAN_OPTIONS=detect_leaks=1 build/host/update-checker-test "$fixture/update-checker"

cc "${common_flags[@]}" -O1 -g -fsanitize=address,undefined \
    "${model_objects[@]}" tests/preferences_failure_test.c \
    -Wl,--wrap=fopen,--wrap=fclose,--wrap=rename,--wrap=remove \
    -o build/host/preferences-failure-test
mkdir -p "$fixture/preferences-failure"
ASAN_OPTIONS=detect_leaks=1 build/host/preferences-failure-test "$fixture/preferences-failure"
