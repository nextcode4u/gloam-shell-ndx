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
| `dist/SHA256SUMS` | SHA-256 of `BOOT.NDS` and all three archives. |

Publish `BOOT.NDS`, all three ZIPs and checksums together for every release. Keep
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
