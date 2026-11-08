# Project conventions

Read [CONTRIBUTING.md](CONTRIBUTING.md), [the source map](docs/SOURCE_MAP.md)
and [the build guide](docs/development/BUILDING.md) before changing the project.

- Keep hardware calls and the frame loop in `arm9/source/main.c`; rendering must
  not read files or mutate application state.
- Use the checked-in `.clang-format` for authored C. Keep vendor changes marked,
  retain all notices and record exact source revisions under `third_party/`.
- Preserve `/BOOT.NDS`, the `_nds/dsi-frontend` compatibility namespace, `_gba`,
  `roms`, `apps`, `media`, `data` and existing homebrew support paths.
- Run `make` and `tools/host-check.sh` for C changes; Python unit tests for tools,
  SD staging and configuration. Prefer synthetic, reproducible fixtures.
- Keep documentation current and concise. Put contracts in guides and ownership
  details in the source map; do not add chronological test logs to public docs.
- Keep generated builds, release dependencies, archives and private runtime data
  ignored. Artifacts belong under `build/` or `dist/`, never source directories.
- Report compatibility limits honestly. Do not turn successful preparation into
  an unverified claim of application boot, save or return support.
- Do not push source or publish artifacts without user authorization.
