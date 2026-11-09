# Build checks and prereleases

`.github/workflows/ci.yml` runs the sanitizer/model/render suites, Python tests
and public-tree checks on pull requests and main pushes. A separate native job
builds with the checksum-pinned official BlocksDS 1.25.0 slim image and packages
`BOOT.NDS`, `sd.zip`, `source.zip`, `licenses.zip` and `SHA256SUMS` together.
The image uses Wonderful GCC 16.2.0. See [BlocksDS Docker setup](https://blocksds.skylyrac.net/docs/setup/options/docker/).
Actions are pinned to reviewed release commits. Update those pins deliberately.

The native image has no Python interpreter by default; the workflow installs
Python and GitHub CLI as packaging tools. Frontend execution does not depend on
Python. Neither job uses private firmware, NAND, commercial titles or emulator
runtime data. Downloaded dependencies and build output remain ignored.

Run **Prepare prerelease** manually from the main branch to prepare a new
`vX.Y.Z-preview.N` or `vX.Y.Z-rc.N` tag. It first runs both check jobs, optionally
checks and packages reviewed add-ons, downloads the resulting artifact and
verifies it again before creating a **draft prerelease**. Review the draft's
notes, source commit, compatibility limits and complete artifact set before
publishing. Existing release tags are not replaced automatically.

When add-ons are requested, upstream checks must report unchanged reviewed pins.
A changed recipe or newer release stops preparation: review and commit the
catalog update first. This avoids creating release artifacts from uncommitted
pin changes. Download-only applications remain installer instructions; their
binary/source/notice payloads never enter the bundled collections.

Local equivalent:

```sh
FRONTEND_TEST_PYTHON=.venv/bin/python tools/check-all.sh
python3 tools/check_tree.py
# Run the add-on preflight and review its report before using a pinned recipe.
.venv/bin/python tools/prepare_addons_release.py --check-only
.venv/bin/python tools/build_addons.py --pinned --skip-build
python3 tools/verify_release.py --addons
```

`verify_release.py` checks the complete companion set and checksums, rejects
unsafe or duplicate ZIP paths, checks matching standalone/SD frontend bytes,
required loaders/forwarder/TMD and source/notices, and rejects personal state or
repository docs in the SD runtime. Use it without `--addons` for a base release.
It verifies packaging integrity, not game compatibility or physical hardware.
Keep the standalone binary and both source/license archives available together;
see [building](BUILDING.md) and [current support](../CURRENT_SUPPORT.md).
