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
- Keep image-generation prompts, prompt logs and draft design concepts out of
  Git and release/source packages. Store them under ignored `build/` or in the
  private workspace. Only production assets belong in version control.
- Report compatibility limits honestly. Do not turn successful preparation into
  an unverified claim of application boot, save or return support.
- Commit each completed logical change separately after its relevant checks pass.
  Keep fixes, features and unrelated cleanup in focused commits; do not accumulate
  a session's work into one large commit or squash independent changes together.
  Do not commit user changes as part of an unrelated task.
- Every release must include standalone `BOOT.NDS`, `sd.zip`, required licenses
  and notices, a combined `source.zip` (including dependency source) and checksums.
  Include the forwarder and matching TMD in `sd.zip`; no separate dependency-source
  artifact. Keep `sd.zip` runtime-only; documentation belongs in the repository
  and `source.zip`, with license/attribution copies in `licenses.zip`.
  Include the requested per-system `roms/**/README.txt` setup notes in the SD ZIP.
  Keep update-only downloads separate from full SD setup; document any required asset/backend migration.
- Optional `addons.zip` must use reviewed release/hash pins and distribution terms;
  update the same combined `source.zip`, `licenses.zip` and checksums with it.
  Keep noncommercial-only apps behind the explicit build flag; never silently
  bundle held apps, games or proprietary external BIOS. The specifically listed
  `download-only` entries and entries with `permission_confirmed: false` must
  never enter binary/source/notice companion collections. Publish only their
  pinned download instructions through the shared Python/PowerShell installer;
  it downloads directly from upstream onto the user's chosen SD root.
- Do not push source or publish artifacts without user authorization.
