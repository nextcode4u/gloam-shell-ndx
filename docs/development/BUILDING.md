# Building and packaging

## Requirements

- BlocksDS **1.25.0 or later**, with Wonderful Toolchain and ARM tools on PATH.
- GNU Make, Git and Python **3.11+**.
- For checks/asset authoring: a host C compiler, Bash and the Python dependencies
  in `requirements-dev.txt`. Pillow is not needed for a normal frontend build.

Follow the [official BlocksDS setup](https://blocksds.skylyrac.net/docs/setup/).
SDK-specific integration details are in [TOOLCHAIN.md](TOOLCHAIN.md).

```sh
make -j4
python3 tools/build_release.py
# Optional audited emulator/player overlay:
python3 tools/build_addons.py  # checks/refreshes upstream pins before building
```

The release script builds by default. `--skip-build` packages an already built
frontend after verifying its build manifest. `--offline` prohibits downloads and
requires all verified dependencies/source checkouts in the cache.

## Outputs

| File | Contents |
| --- | --- |
| `dist/BOOT.NDS` | Frontend-only update, byte-identical to `BOOT.NDS` in the SD ZIP. |
| `dist/licenses.zip` | Required license texts, dependency notices, credits and asset attribution. |
| `dist/sd.zip` | Root `BOOT.NDS`, themes/profiles, official nds-bootstrap, Slot-1 helper, forwarder and matching TMD; SD runtime files and per-collection README.txt files. |
| `dist/source.zip` | Gloam source, build tools, docs, licenses and tests; exact backend/helper sources, submodules and revision manifest under `dependencies/`. |
| `dist/addons.zip` (optional) | Reviewed emulator/player binaries and collection setup notes; produced by the add-on builder. |
| `dist/SHA256SUMS` | SHA-256 of `BOOT.NDS` and every produced release archive. |

Publish `BOOT.NDS`, the three base ZIPs and checksums together for every release.
When including `addons.zip`, publish its matching updated `licenses.zip`,
`source.zip` and checksums too. Keep
source and license downloads beside both binary download options. The forwarder
and its TMD are included in `sd.zip` under `/_nds/dsi-frontend/install/`;
installation is optional. Guides and development docs stay in the repository
and `source.zip`. `sd.zip` contains no `docs/`, root documentation, package
manifest or license/credit copies; those notices accompany it in `licenses.zip`.
Requested per-system `roms/**/README.txt` files are the setup-note exception;
they list formats, argv behavior and BIOS requirements beside the content.
The manifest is retained in `source.zip`. Dependency source is part of `source.zip`, so there
is no separate dependency-source artifact. Replacing only `BOOT.NDS` preserves
personal settings, themes, backends and libraries; document any required
asset/backend migrations in the release notes. A standalone binary
does not remove the requirement to distribute its notices and matching source.
Source for linked SDK/runtime libraries is identified under
`third_party/blocksds`; modified bundled libraries/loaders are included in Gloam source. Upstream backend build instructions remain in each
source tree: nds-bootstrap uses devkitARM, while Gloam and Slot-1 use BlocksDS.

`config/release-dependencies.json` pins official release URLs, tags, exact source
commits, ZIP hashes and installed file hashes. The builder fails on mismatches,
missing inputs or unsafe archive paths. It installs only listed backend members;
no unrelated CIA/TAD variants enter the SD ZIP. It does not patch downloaded
backends or modify an SD card/emulator. Cache lives under `build/release-cache`
and `build/release-sources`; outputs and downloads are ignored by Git.

Online add-on release builds run `tools/prepare_addons_release.py` before
packaging. It checks upstream releases, refreshes unchanged-layout recipes,
validates source/notices and hashes, regenerates collection notes, runs the
Python checks and then builds. `make addons-check` prepares a report only;
`make addons-pinned` reproduces the existing lock. Complex/changed recipes stop
for review. Reports and validation logs are under ignored `build/addon-updates/`.
Use the project virtualenv or install `requirements-dev.txt` for the checks.
GitHub CLI is optional but avoids unauthenticated API limits when authenticated.

The optional [add-on builder](../guides/ADDONS.md) uses
`config/addon-dependencies.json`. `python3 tools/build_addons.py --list` lists
all audited entries without downloading. The default bundles ten applications
with established distribution terms. `--noncommercial` adds five with no-profit
terms. Apps with unconfirmed permission are excluded from every bundled
binary/source/notice collection, including explicit `--only` selections.

Every add-on ZIP also contains `addon-installer/`, with one Python script, one
standalone PowerShell script, a generated download manifest and usage notes.
The scripts download the remaining pinned applications directly from upstream
to an explicitly selected SD root. They support selection, dry runs, caching,
offline use, reviewed-catalog updates and preserving existing configs. Update
recipes also cover bundled apps, and pair the SNEmulDS shim/payload. See [the add-on guide](../guides/ADDONS.md).
`--only dsxp gameyob-dsi` restricts bundled applications; the installer catalog
is generated for everything outside that selection. Packaging never embeds download-only binaries/source archives. The online
preflight may fetch upstream files into the ignored cache to verify prospective
pins; those remain excluded from release payloads.

`--offline` and `--skip-build` retain the main builder meanings. `make addons`
is a convenience target; `tools/build_release.py --addons` supports the same
noncommercial option. Permitted bundled source stays under `dependencies/addons/`
in `source.zip`; cached build dependencies remain ignored under `build/`.

To update a dependency, review its upstream release, license and source revision,
refresh the lock and notices, and validate the actual launch route. Do not resolve
“latest” silently during a build. Build identity is distinct from compatibility;
see [current support](../CURRENT_SUPPORT.md).

Documentation screenshots can be regenerated with
`.venv/bin/python tools/make_screenshots.py` after the host renderer is built.
They use the actual renderer, supplied themes and authored example content.

`config/package-inputs.json` explicitly lists reviewed SD and source inputs.
New public files must be added there. `python3 tools/check_tree.py` checks the
catalog, local documentation links and unwanted public inputs. Git status is
not an inventory of ignored build output.

The frontend and forwarder share `forwarder/icon.png`. The approved full-size
artwork is retained as `forwarder/icon-source.png`. To regenerate the native
32×32, 16-color banner asset, run:

```sh
.venv/bin/python tools/pack_banner_icon.py forwarder/icon-source.png forwarder/icon.png
make -j4
```

The icon exporter trims transparent outer padding before scaling the tile to
fill the banner. The full-size source remains unchanged.

Rebuilding also regenerates the forwarder TMD to match the changed binary.

## Checks

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
FRONTEND_TEST_PYTHON=.venv/bin/python tools/check-all.sh
python3 tools/check_tree.py
```

Host suites use sanitizers and synthetic fixtures; they are developer tools,
not a shipped test environment or a substitute for physical hardware validation.
`tools/stage_sd.py` can merge the template and built frontend into `dist/sdroot`;
this lightweight staging command does not download the release backends.

## Source layout

`arm9`, `arm7`, `common` and `forwarder` hold application source. `sdroot` contains
checked-in assets/configuration, `third_party` holds dependency sources/notices,
and `tools`/`tests` support building and checking contributions. The
[source map](../SOURCE_MAP.md) describes ownership within these directories.

## Shared-extension profile migration

Folder-aware association releases require the matching frontend `BOOT.NDS` and
updated profile INIs. In particular, `dslibris-documents.ini` and
`picodrivetwl.ini` share `.md` with different `content_root` values. Older frontends
ignore this field and can select the first global match. The SD/add-on templates
carry the new rules; Python/PowerShell updates preserve existing custom profiles,
so merge these fields into preserved files rather than replacing user settings.
