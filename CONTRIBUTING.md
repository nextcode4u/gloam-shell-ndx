# Contributing

Start with [building](docs/development/BUILDING.md) and the [source map](docs/SOURCE_MAP.md).

Keep ARM9 application modules in `arm9/source`, hardware adaptation/frame-loop
work in `main.c`, shared protocols in `common/include` and ARM7 services in
`arm7/source`. Rendering consumes prepared state; it must not perform I/O.
Use small functions, descriptive names and the checked-in `.clang-format`.

Run `make` and `tools/host-check.sh` for C changes. Run the Python unit tests
for tool/configuration changes. `tools/check-all.sh` runs both. Keep automated
checks and synthetic fixtures reproducible, with generated output under `build/`.
Describe observed behavior and remaining limits separately.

Preserve conventional SD support directories and existing libraries. Changes
must not silently overwrite another application's configuration, BIOS or saves.
Use lazy reads, bounded memory/work and responsive cancellation for large libraries.

Keep dependency copyright/license notices intact, mark local modifications and
record exact upstream revisions. Describe design influence as influence; identify
reused source/data precisely in THIRD_PARTY.md. New original source is offered
under GPL-3.0-or-later unless an asset notice explicitly says otherwise.

Build output, downloads, runtime images, private references and machine-specific
configuration stay out of Git. Release ZIPs belong in GitHub Releases, together
with the matching source archive and SHA256SUMS. Never include games, proprietary BIOS/NAND,
keys, saves or private test data. Free replacement firmware needs explicit
license/source pins and emulator-specific compatibility review. This repository does not contain emulator
forks or environment-specific debugging history.

## Commits and releases

Commit each completed logical change after its relevant checks pass. Keep
features, fixes and unrelated cleanup in separate, descriptive commits so a
later push retains the development history. Do not squash independent changes
into one session-sized commit or include unrelated user edits.

Every release includes standalone `BOOT.NDS` for frontend-only updates, `sd.zip`
for full setup, required licenses/notices, a combined frontend/dependency source
archive and checksums. Keep `sd.zip` runtime-only: guides stay in the repository
and `source.zip`; notices and credits go in `licenses.zip`. The requested per-system ROM-folder
README.txt files accompany the SD runtime as setup notes. Keep the source and license downloads beside both binary
download options. Document required asset/backend changes when an update needs
more than replacing `BOOT.NDS`; never silently overwrite personal settings.

Keep only the current published Gloam preview release. After its complete asset
set passes verification, remove older published `-preview.N` releases and their
assets. Preserve Git tags/history, stable releases and release candidates. The
prerelease workflow applies this policy when a preview is published; preparing
a draft does not remove the current preview.

Image-generation prompts, prompt logs and draft design concepts are local working
artifacts. Keep them under ignored `build/` or outside the source tree; exclude
them from commits and release/source packages. Commit production assets only.
