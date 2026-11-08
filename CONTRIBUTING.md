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
with matching source archives and SHA256SUMS. Never include games, BIOS/NAND,
keys, saves or private test data. This repository does not contain emulator
forks or environment-specific debugging history.
