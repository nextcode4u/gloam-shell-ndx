# Build checks and releases

`.github/workflows/ci.yml` runs the sanitizer/model/render suites, Python tests
and public-tree checks on pull requests and main pushes. A separate native job
builds with the checksum-pinned official BlocksDS 1.25.0 slim image and packages
`BOOT.NDS`, `sd.zip`, `source.zip`, `licenses.zip` and `SHA256SUMS` together.
The image uses Wonderful GCC 16.2.0. See [BlocksDS Docker setup](https://blocksds.skylyrac.net/docs/setup/options/docker/).
A native Windows job exercises the standalone Python/PowerShell menus and CMD
launcher with original fixtures, including confirmation, settings preservation
and update backups. It does not use an SD card, game data or emulator.
Actions are pinned to reviewed release commits. Update those pins deliberately.

The native image has no Python interpreter by default; the workflow installs
Python and GitHub CLI as packaging tools. Frontend execution does not depend on
Python. Neither job uses private firmware, NAND, commercial titles or emulator
runtime data. Downloaded dependencies and build output remain ignored.

## Version numbering

Use Semantic Versioning: `MAJOR.MINOR.PATCH`, with Git tags such as `v1.1.0`.
Choose the highest required increment across all changes since the last release;
release size, time spent and number of commits do not determine the version.

| Increment | Gloam policy | Example after 1.1.0 |
| --- | --- | --- |
| Patch | Backward-compatible bug fixes, performance fixes, documentation or asset corrections; no new features. | `1.1.1` |
| Minor | New features that preserve existing supported configurations and interfaces. Reset patch to zero. | `1.2.0` |
| Major | Incompatible changes to supported public contracts, without a compatible migration or fallback. Reset minor and patch to zero. | `2.0.0` |

Gloam's public compatibility surface includes documented settings/configuration,
SD paths, theme formats, file-association profiles, argv and loader/forwarder
interfaces. Internal source refactors alone do not require a major release.
An automatic migration that preserves existing behavior can keep an update
backward-compatible; requiring users to rewrite configurations or convert all
existing themes is a breaking change.

Compatibility is assessed in the upgrade direction: existing supported packs
and settings must continue working in the newer frontend. A newly authored theme
requiring a new renderer feature does not itself make that frontend release
breaking, provided older supported themes still load. State its minimum frontend
version separately. Theme-format versions and upstream dependency/fork versions
are independent of Gloam's release number.

Use `v1.2.0-preview.1` or `v1.2.0-rc.1` for prereleases of the intended version.
Publish the final release as `v1.2.0`, without a suffix. Never replace an already
published tag or change its artifacts; corrections receive a new version.
Set `GLOAM_VERSION` to the exact intended tag before building, and explain any
migration in the release notes. Apply this policy to future releases; existing
published versions remain unchanged.

## Preparing a release

Run **Prepare release** manually from the main branch to prepare a new
`vX.Y.Z`, `vX.Y.Z-preview.N` or `vX.Y.Z-rc.N` tag. It first runs all check jobs, optionally
checks and packages reviewed add-ons, downloads the resulting artifact and
verifies it again before creating a **draft release**. Preview/rc tags are marked as prereleases;
stable tags are not. Review the draft's
notes, source commit, compatibility limits and complete artifact set before
publishing. Existing release tags are not replaced automatically.

Enable the **noncommercial** input when the release should also include reviewed
apps with explicit no-profit distribution grants. It defaults off and passes the
existing `--noncommercial` flag to both preflight and packaging. Keep the generated
noncommercial notices with that package; download-only/unconfirmed apps stay in
the shared downloader regardless of this input.

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
