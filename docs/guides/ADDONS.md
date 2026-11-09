# Optional emulator and media add-ons

`addons.zip` contains only applications with established redistribution terms,
plus one shared Python downloader and one equivalent standalone PowerShell
script. Applications with unconfirmed permission are downloaded directly from
upstream by those scripts; their binaries are not included in Gloam's archives.
Both scripts target an explicitly selected existing SD root or staging folder.

## Build and install

```sh
python3 tools/build_addons.py
python3 tools/build_addons.py --skip-build --offline
python3 tools/build_addons.py --noncommercial
python3 tools/build_addons.py --list
```

Normal online builds first run `tools/prepare_addons_release.py`. This checks
all 27 pinned applications, updates compatible version/source/member hashes,
regenerates profiles/readmes, runs the Python suite and public-tree check, then
builds the add-on ZIP and matching companions. It uses the project virtualenv
for checks when available; otherwise install `requirements-dev.txt` in the
interpreter running the build. GitHub CLI authentication is used when available;
the stdlib HTTP fallback uses GitHub's public rate limits.

```sh
make addons-check                          # report/proposals only
python3 tools/prepare_addons_release.py --check-only
python3 tools/prepare_addons_release.py --skip-build --noncommercial
python3 tools/build_addons.py --pinned     # reproduce existing reviewed pins
```

Current assets are checked against GitHub digests, or downloaded and hashed
when upstream provides no digest. Replaced/removed assets stop the build.
The preflight compares publication dates rather than tag strings, avoids
replacing a stable pin with a prerelease, and preserves the current prerelease
channel when one is already pinned. Pinned host snapshots are not replaced by
guessed branch builds; an unversioned host download must still match its hash.
The SNEmulDS pair remains explicitly matched. Specialized source bundles,
changed submodules, notices, asset layouts or missing argv evidence stop the
build for recipe review instead of silently changing those contracts.
Redistribution statuses and permission flags are never promoted by this script.
Preflight success verifies recipes and pins; new-build hardware compatibility
still needs testing. Apparent backports to an older version track require review.

Reports, proposed pins and notice diffs stay under ignored
`build/addon-updates/`. `--check-only` writes those files without changing tracked
pins or building; it exits 2 when compatible updates are proposed. Errors or
review-required changes stop preparation before applying any proposed pins.
After a successful preflight, compatible updates are written to the lock and
audit table and generated collection notes are refreshed. Validation logs are
in `build/addon-updates/validation.log`. If validation fails, the tracked pin
changes remain available for review and the archive build does not run. Review
and commit dependency changes separately; this script never commits or pushes.

`make addons`, `tools/build_addons.py`, and `tools/build_release.py --addons`
use this online preparation by default. Explicit `--pinned`,
`--pinned-addons` on the release builder, or `--offline` reuse existing pins
without claiming a fresh upstream check. `--only` restricts the bundled output,
while release preparation still checks the complete updater catalog.

The default bundles DSXP (nextcode4u), GameYob, A7800DS, A8DS, StellaDS,
PokeMini, snemulds-shim and its matching SNEmulDS payload. `--noncommercial`
also bundles A5200DS, DracoDS, MicroDS, SpeccySE and ColecoDS.
No-profit conditions still apply to those five applications. DS994a and
PicoDriveTWL are download-only because complete component redistribution
coverage remains unconfirmed.

Merge `addons.zip` after `sd.zip`. Application binaries use
`/_nds/dsi-frontend/plugins/apps/`, matching the association profiles. Preserve
custom shim settings when merging an update. The pack includes no games or
proprietary external BIOS. Keep the matching `licenses.zip`, `source.zip` and
`SHA256SUMS` with bundled binaries when publishing them. Dependency source and
notices are collected only for applications actually bundled in this selection.

## Download and stage the remaining apps

Extract `addons.zip` on the computer. Its `addon-installer/` folder contains:

- `download-addons.py`: Python 3.11+ version.
- `download-addons.ps1`: PowerShell 5.1+ / PowerShell 7 version; no Python needed.
- `downloads.json`: generated URLs, download/member hashes and destination pins.
- `README.txt`: portable command reference.

Run either script from that folder, replacing `E:` with the SD card or using a
staging directory you created:

```sh
python download-addons.py --list
python download-addons.py --target E:/ --dry-run
python download-addons.py --target E:/
python download-addons.py --target E:/ --only gbarunner3 tuna-vids
```

```powershell
.\download-addons.ps1 -List
.\download-addons.ps1 -Target E:\ -DryRun
.\download-addons.ps1 -Target E:\
.\download-addons.ps1 -Target E:\ -Only gbarunner3,tuna-vids
```

The two scripts share the same manifest and install destinations. They check
SHA-256 for every download and installed member, read only allowlisted runtime
files from ZIPs and preserve existing files by default. `--overwrite` /
`-Overwrite` explicitly replaces matching application/config paths.
`--noncommercial` / `-Noncommercial` enables no-profit downloads, including
DS994a, PicoDriveTWL and any noncommercial apps omitted by the chosen bundled selection.

`--cache` / `-Cache` changes the computer-side cache, whose default is
`~/.gloam-addons-cache`. `--offline` / `-Offline` uses a populated cache only.
Normal install `--dry-run` / `-DryRun` performs no downloads or writes. A failed download or
hash mismatch stops the run; files already completed stay installed. Re-run to
continue. The scripts do not remove older installations, rewrite associations
or settings, or supply firmware. `--list` / `-List` identifies apps requiring
manual setup because a reviewed runtime download is not yet pinned.

The manifest depends on the selected bundle: already bundled apps are excluded
from its download list. SNEmulDS shim/payload recipes are always selected together. [Portable usage](ADDON_INSTALLER.txt) includes the
Windows per-process execution-policy command when needed.

## Check and stage updates

The same scripts update bundled applications as well as download-only ones.
The generated manifest carries all 27 reviewed runtime recipes, including the
matching SNEmulDS shim/payload pair. Check an SD card without changing it:

```sh
python download-addons.py --target E:/ --check-updates
python download-addons.py --target E:/ --update --dry-run
python download-addons.py --target E:/ --update
```

```powershell
.\download-addons.ps1 -Target E:\ -CheckUpdates
.\download-addons.ps1 -Target E:\ -Update -DryRun
.\download-addons.ps1 -Target E:\ -Update
```

Update modes refresh the reviewed dependency catalog from Gloam's public GitHub
repository. An unavailable or older catalog leaves the packaged pins in use,
with an explicit message. The updated catalog must be published before remote
users can receive its new pins. `--catalog FILE` / `-Catalog FILE` uses a local
reviewed dependency lock or generated manifest instead; `--offline` / `-Offline`
uses packaged pins and cached runtime downloads without network access.

By default `--update` / `-Update` selects only installed apps. `--only` / `-Only`
can select an app explicitly, including staging it into a new directory.
Noncommercial selections still require the noncommercial option. Selecting
either SNEmulDS component automatically selects its matching partner. ZIP and
tar.gz payload members are read without extracting other archive members.

Executables already matching reviewed hashes are left alone. Differing `.nds`
and `.srl` files are replaced only in update/overwrite mode; a difference may
also be a custom or newer build, so inspect the check output first. Update mode
backs up each replaced file under the computer-side cache's
`previous-files/<old-sha256>/<filename>`. Copy that file back to its original SD
path to restore it. Custom `.ini`/`.json` files remain untouched unless overwrite
is requested. No ROM, save or BIOS is updated. A failed run can leave completed
files updated; keep the backups and re-run to finish the reviewed pair/selection.

Add `--upstream` / `-Upstream` to an online update check to report other upstream
GitHub stable-release tags. A different tag can be older than a pinned prerelease;
it is not automatically treated as a newer compatible build. Unreviewed upstream
assets are never silently installed: version/member mappings and hashes must be
reviewed in the catalog first. API limits/errors are reported separately from
the installed-file check. Unversioned host snapshots use reviewed pins.

Update dry runs may read the online catalog, but do not download runtime files
or write SD/cache data. Use offline mode to avoid all network access. Cache
entries include their expected hash so newer bytes can reuse an upstream asset
name without conflicting with older cached versions.

## Permission and source coverage

Only `approved` entries and explicit opt-in `noncommercial` entries may be
bundled. `download-only`, legacy `assumed`, and `permission_confirmed: false`
entries cannot bypass this through `--only`. No permission is inferred from a
public repository, program age or an absent prohibition. The [download-only
list](DOWNLOAD_ONLY_ADDONS.md) records unresolved grants and component issues.

Source and notice archives for download-only applications are not bundled
alongside their absent binaries. The lock and installer retain source/release
links and compatibility findings. Direct downloading is not permission to
redistribute the resulting SD setup. The [GBARunner3](GBARUNNER3_NOTICES.md) and
[tuna-viDS](TUNA_VIDS_NOTICES.md) guides retain their specific source/notice gaps.

## Release and argv audit

The following catalog records audited source behavior, not a physical-DSi
compatibility guarantee. `direct` means the content-loading path consumes argv;
it does not mean that just declaring argc/argv was counted as support.

| App / pinned version | Argv | Delivery / terms |
| --- | --- | --- |
| [a5200ds 3.9](https://github.com/wavemotion-dave/A5200DS/releases/tag/3.9) | [direct](https://github.com/wavemotion-dave/A5200DS/blob/b7bba6ca9ea2a4e411753ed2d54e33c8bc93fd1a/arm9/source/main.c) | Noncommercial opt-in; Custom noncommercial redistribution |
| [a7800ds 5.2c](https://github.com/wavemotion-dave/A7800DS/releases/tag/5.2c) | [direct](https://github.com/wavemotion-dave/A7800DS/blob/32b7b1ed78f5edbcd29a797a1376e25a9bd9d265/arm9/source/main.c) | Bundled when pinned; GPL-2.0-or-later |
| [a8ds 4.2](https://github.com/wavemotion-dave/A8DS/releases/tag/4.2) | [direct](https://github.com/wavemotion-dave/A8DS/blob/b5718df173e0b4037bcb8414640af26458c0bade/arm9/source/main.c) | Bundled when pinned; GPL-2.0-or-later + Altirra notices |
| [colecods 11.1](https://github.com/wavemotion-dave/ColecoDS/releases/tag/11.1) | [direct](https://github.com/wavemotion-dave/ColecoDS/blob/29596955995526a10c8a9ca8c17d39b642f54dbf/arm9/source/colecoDS.c) | Noncommercial opt-in; Noncommercial redistribution with retained author/component notices |
| [dracods 1.9](https://github.com/wavemotion-dave/DracoDS/releases/tag/1.9) | [direct](https://github.com/wavemotion-dave/DracoDS/blob/03cddfbddf93056291c69fd512a8d8db0bbb2caa/arm9/source/DracoDS.c) | Noncommercial opt-in; MIT base + README noncommercial condition |
| [ds994a 2.9](https://github.com/wavemotion-dave/DS994a/releases/tag/2.9) | [direct](https://github.com/wavemotion-dave/DS994a/blob/85098fcfcaa176a919fa4f2162706ac7814ea263/arm9/source/DS99.c) | Direct upstream download; Noncommercial redistribution with constituent permissions/notices |
| [dslibris v1.6.0-blocksds-argv.1](https://github.com/nextcode4u/dslibris/releases/tag/v1.6.0-blocksds-argv.1) | [direct](https://github.com/nextcode4u/dslibris/blob/0fcd8dc8b18a9a0698a28241a430fd6c003f9bb9/ARGUMENT_LAUNCH.md) | Bundled when pinned; GPL-3.0-or-later combined reader; SIL OFL-1.1 Liberation Fonts; dependency notices retained |
| [dsxp v1.1.0-sd-argv.1](https://github.com/nextcode4u/DSXP/releases/tag/v1.1.0-sd-argv.1) | [direct](https://github.com/nextcode4u/DSXP/blob/485805cffae5ad2cfa32efd6946cc7ec6aa94ae0/source/main.c) | Bundled when pinned; GPL-2.0-or-later |
| [fastvideods v0.0.1](https://github.com/Gericom/FastVideoDSPlayer/releases/tag/v0.0.1) | [direct](https://github.com/Gericom/FastVideoDSPlayer/blob/62c909ebf6e509b11eedc2ad4e4549a717f0f6ab/arm9/source/main.cpp) | Direct upstream download; Project license not stated |
| [gameyob-dsi v0.5.3b-mavica](https://github.com/Lana-chan/GameYob/releases/tag/v0.5.3b-mavica) | [direct](https://github.com/Lana-chan/GameYob/blob/04f88d96ca653c60fd1b110ce7011db7e8e906a3/arm9/source/main.cpp) | Bundled when pinned; MIT |
| [gbarunner3 hicode (third-party snapshot)](https://files.deletecat.com/GBARunner3-hicode.zip) | [direct](https://github.com/Gericom/GBARunner3/blob/ecaa817815d9761745606592f04affe5ee9c3731/code/core/arm9/source/main.cpp) | Direct upstream download; Project-wide license not established |
| [gimlids v1.7a-argv.1](https://github.com/nextcode4u/GimliDS/releases/tag/v1.7a-argv.1) | [direct](https://github.com/nextcode4u/GimliDS/blob/40c406980b5e53914cc0ade6881300c1833f52d0/ARGUMENT_LAUNCH.md) | Bundled when pinned; GPL-2.0-or-later |
| [hachibitto v1.9-argv.1](https://github.com/nextcode4u/Hachibitto/releases/tag/v1.9-argv.1) | [direct](https://github.com/nextcode4u/Hachibitto/blob/f0198347abf0c5e0317c3bd32af171e37bc74ec6/ARGUMENT_LAUNCH.md) | Direct upstream download; Noncommercial core; firmware/asset rights unresolved |
| [jenesisdsplus v0.8.1](https://github.com/DS-Homebrew/jEnesisDSPlus/releases/tag/v0.8.1) | [direct](https://github.com/DS-Homebrew/jEnesisDSPlus/blob/ca3bb43d14f8d78ccb6546d753817defaac25937/newCode/source/main.s) | Direct upstream download; No explicit Plus redistribution license; closed-source base terms unconfirmed |
| [lodjurds v0.1.5](https://github.com/FluBBaOfWard/LodjurDS/releases/tag/v0.1.5) | [direct](https://github.com/FluBBaOfWard/LodjurDS/blob/1e8fe92f82bb0c8926e7fc6212139630f46e2e7b/source/Main.c) | Direct upstream download; Project-wide redistribution grant not found |
| [microds 1.1](https://github.com/wavemotion-dave/MicroDS/releases/tag/1.1) | [direct](https://github.com/wavemotion-dave/MicroDS/blob/9a74db2c6aa58df1cfe530f52e723d2aa2e74e09/arm9/source/MicroDS.c) | Noncommercial opt-in; MIT base + README noncommercial condition |
| [nesds v2.1](https://github.com/DS-Homebrew/NesDS/releases/tag/v2.1) | [direct](https://github.com/DS-Homebrew/NesDS/blob/928afd9ca3c846f06264dfb1be6a2edf44575b66/arm9/source/romloader_frontend.c) | Direct upstream download; Legacy public-domain statement; newer component grants unconfirmed |
| [ngpds v0.6.0](https://github.com/FluBBaOfWard/NGPDS/releases/tag/v0.6.0) | [direct](https://github.com/FluBBaOfWard/NGPDS/blob/4dbcc210cdb519414ff20d2886645a530e298206/source/Main.c) | Direct upstream download; Project-wide redistribution grant not found |
| [nintv-ds 6.4](https://github.com/wavemotion-dave/NINTV-DS/releases/tag/6.4) | [direct](https://github.com/wavemotion-dave/NINTV-DS/blob/c03e2ab9f052013d0328d1ccd3144ed07a8b9ae6/arm9/source/main.cpp) | Manual setup; no reviewed runtime pin; Custom redistribution notice; restricted extras |
| [nitrografx v0.9.1](https://github.com/FluBBaOfWard/NitroGrafx/releases/tag/v0.9.1) | [direct](https://github.com/FluBBaOfWard/NitroGrafx/blob/62920a05e9c7e31144d7e16b58797cdd8bc3789a/source/Main.c) | Direct upstream download; Project-wide redistribution grant not found |
| [nitroswan v0.7.7](https://github.com/FluBBaOfWard/NitroSwan/releases/tag/v0.7.7) | [direct](https://github.com/FluBBaOfWard/NitroSwan/blob/564e171fb93c2c11f98f38607493675727676c2c/source/Main.c) | Direct upstream download; Project-wide redistribution grant not found |
| [picodrivetwl v2.1.0](https://github.com/DS-Homebrew/PicoDriveTWL/releases/tag/v2.1.0) | [direct](https://github.com/DS-Homebrew/PicoDriveTWL/blob/4fcdb1f0bef46a4704b1870d787b362afa7bfec0/arm9/source/main.cpp) | Direct upstream download; Noncommercial Pico/DrZ80; complete redistribution coverage unconfirmed |
| [pokemini 0.60-arg](https://github.com/DS-Homebrew/PokeMini/releases/tag/0.60-arg) | [direct](https://github.com/DS-Homebrew/PokeMini/blob/f8eb158f7082d80739075baa3653afc14fdf3d70/platform/nds/PokeMini_NDS.c) | Bundled when pinned; GPL-3.0-or-later + zlib notices |
| [rocketvideoplayer v2.3.1](https://github.com/RocketRobz/RocketVideoPlayer/releases/tag/v2.3.1) | [direct](https://github.com/RocketRobz/RocketVideoPlayer/blob/50803b9027ca43ab278c1486442fcfc70c90b309/nds/arm9/source/main.cpp) | Manual setup; no reviewed runtime pin; MIT code; Nintendo assets separately credited |
| [s8ds v1.1.8](https://github.com/FluBBaOfWard/S8DS/releases/tag/v1.1.8) | [direct](https://github.com/FluBBaOfWard/S8DS/blob/90f2c13368f0b2ceac7887802e77d516d6337436/source/Main.c) | Direct upstream download; Project-wide redistribution grant not found |
| [scummvm v2.9.1 (published DSi build)](https://www.scummvm.org/downloads/) | [native game-ID and discovery options; not a ROM-file association](https://github.com/scummvm/scummvm/blob/9d9b9e93108a276c551aeffa390169ccc5148e15/backends/platform/ds/dsmain.cpp) | Manual setup; no reviewed runtime pin; GPL-3.0-or-later + constituent notices |
| [snemulds TGDS1.65 (shim-compatible snapshot)](https://github.com/cotodevel/SnemulDS/tree/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd) | [TGDS-only](https://github.com/cotodevel/SnemulDS/blob/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd/arm9/source/main.c) | Bundled when pinned; GPL-2.0-or-later |
| [snemulds-shim v0.15](https://github.com/Sanrax/snemulds-shim/releases/tag/v0.15) | [shim](https://github.com/Sanrax/snemulds-shim/blob/2a7b3ed08a72d28529662cfc9ede8d3e83c2095b/source/arguments.c) | Bundled when pinned; GPL-3.0-or-later + vendored notices |
| [speccyse 2.2](https://github.com/wavemotion-dave/SpeccySE/releases/tag/2.2) | [direct](https://github.com/wavemotion-dave/SpeccySE/blob/85c85862470b69b75f6e49476a9c4ce992064bb3/arm9/source/SpeccySE.c) | Noncommercial opt-in; Custom noncommercial redistribution + core notices |
| [stellads 8.4a](https://github.com/wavemotion-dave/StellaDS/releases/tag/8.4a) | [direct](https://github.com/wavemotion-dave/StellaDS/blob/1ff41dc60c4debacd4493bae5bb045cdbbef7800/arm9/source/main.cpp) | Bundled when pinned; GPL-2.0-or-later + BSD notices |
| [sugards 1.6](https://github.com/wavemotion-dave/SugarDS/releases/tag/1.6) | [direct](https://github.com/wavemotion-dave/SugarDS/blob/ca4b95960d55e24d1d5bf815f5fffcacad8e4704/arm9/source/SugarDS.c) | Manual setup; no reviewed runtime pin; GPL-2.0-or-later code; separately copyrighted firmware |
| [tuna-vids v1.2-clut](https://github.com/DS-Homebrew/tuna-vids/releases/tag/v1.2-clut) | [direct](https://github.com/DS-Homebrew/tuna-vids/blob/6781a1a7ac723cb1274de6bd8734b21a9fbbebc5/arm9/source/main.c) | Direct upstream download; GPL-2.0-or-later ARM9 + Helix RPSL/RCSL ARM7 |
| [wasabids v0.2.6](https://github.com/FluBBaOfWard/WasabiDS/releases/tag/v0.2.6) | [direct](https://github.com/FluBBaOfWard/WasabiDS/blob/b074708a4c0b670e9f3e7d520a776f2bad2695c1/source/Main.c) | Direct upstream download; Project-wide redistribution grant not found |

Shared extension setup is described in [folder-aware associations](EMULATORS.md#shared-extensions-and-folder-rules).
The reader overlay includes the two DSLibris profiles and the paired PicoDriveTWL
folder-rule template; it does not distribute the PicoDriveTWL executable through
that entry. Install the matching frontend `BOOT.NDS` with these templates. Existing
custom profiles remain preserved by both installers.

## Conditions and exceptions

- **[a5200ds](https://github.com/wavemotion-dave/A5200DS/tree/b7bba6ca9ea2a4e411753ed2d54e33c8bc93fd1a):** Distribution without profit is explicitly permitted with complete notice and thanks to Alekmaul and wavemotion-dave.
- **[a7800ds](https://github.com/wavemotion-dave/A7800DS/tree/32b7b1ed78f5edbcd29a797a1376e25a9bd9d265):** README explicitly adopts ProSystem GPLv2-or-later; supply full GPL text and matching source.
- **[a8ds](https://github.com/wavemotion-dave/A8DS/tree/b5718df173e0b4037bcb8414640af26458c0bade):** Retain Atari800, Alekmaul, wavemotion-dave and Avery Lee notices.
- **[colecods](https://github.com/wavemotion-dave/ColecoDS/tree/29596955995526a10c8a9ca8c17d39b642f54dbf):** README explicitly permits no-profit distribution of the emulator with notices and credits to wavemotion-dave, Alekmaul and Marat Fayzullin. Package the unchanged release only; preserve its ColEM contact request and FluBBa notices. No external proprietary BIOS is included.
- **[dracods](https://github.com/wavemotion-dave/DracoDS/tree/03cddfbddf93056291c69fd512a8d8db0bbb2caa):** Root LICENSE covers the MIT base; README adds a noncommercial condition for the derivative. Preserve both; do not label the whole app MIT.
- **[ds994a](https://github.com/wavemotion-dave/DS994a/tree/85098fcfcaa176a919fa4f2162706ac7814ea263):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs. The application has no-profit terms; its embedded speech samples have no separately established grant.
- **[dslibris](https://github.com/nextcode4u/dslibris/tree/argv-launch):** The BlocksDS fork accepts selected-book argv and supports the [document formats listed upstream](https://github.com/nextcode4u/dslibris/blob/argv-launch/DOCUMENT_FORMATS.md). The runtime includes five unmodified Liberation Fonts. GPLv3, OFL and dependency notices and matching source accompany the add-on in the companion archives.
- **[dsxp](https://github.com/nextcode4u/DSXP/tree/485805cffae5ad2cfa32efd6946cc7ec6aa94ae0):** Use nextcode4u/DSXP SD-argv prerelease; matching release source includes libmad.
- **[fastvideods](https://github.com/Gericom/FastVideoDSPlayer/tree/62c909ebf6e509b11eedc2ad4e4549a717f0f6ab):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[gameyob-dsi](https://github.com/Lana-chan/GameYob/tree/04f88d96ca653c60fd1b110ce7011db7e8e906a3):** Retain the Drenn/contributor notice and both DS/DSi variants.
- **[gbarunner3](https://github.com/Gericom/GBARunner3/tree/ecaa817815d9761745606592f04affe5ee9c3731):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs. The exact corresponding source revision of the supplied hicode build is unidentified. Direct argv handling is statically verified.
- **[gimlids](https://github.com/nextcode4u/GimliDS/tree/argv-launch):** The v1.7a-argv.1 BlocksDS release opens selected PRG, CRT and D64 files directly. It is bundled with matching source/notices and available through the shared downloader. External C64 firmware remains user-supplied.
- **[hachibitto](https://github.com/nextcode4u/Hachibitto/tree/argv-launch):** The v1.9-argv.1 fork release is download-only through the shared Python/PowerShell installer, with noncommercial opt-in. It inherits embedded MSX firmware whose redistribution permission is unconfirmed; its binary/source/notices remain excluded from Gloam companion collections.
- **[jenesisdsplus](https://github.com/DS-Homebrew/jEnesisDSPlus/tree/ca3bb43d14f8d78ccb6546d753817defaac25937):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[lodjurds](https://github.com/FluBBaOfWard/LodjurDS/tree/1e8fe92f82bb0c8926e7fc6212139630f46e2e7b):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[microds](https://github.com/wavemotion-dave/MicroDS/tree/9a74db2c6aa58df1cfe530f52e723d2aa2e74e09):** Root LICENSE covers the MIT base; README adds a noncommercial condition for the derivative. Preserve both; do not label the whole app MIT.
- **[nesds](https://github.com/DS-Homebrew/NesDS/tree/928afd9ca3c846f06264dfb1be6a2edf44575b66):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[ngpds](https://github.com/FluBBaOfWard/NGPDS/tree/4dbcc210cdb519414ff20d2886645a530e298206):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[nintv-ds](https://github.com/wavemotion-dave/NINTV-DS/tree/c03e2ab9f052013d0328d1ccd3144ed07a8b9ae6):** README permits emulator redistribution with attribution, but extras/overlays are explicitly personal-use-only. A source/runtime subset excluding those extras needs a separate completed audit.
- **[nitrografx](https://github.com/FluBBaOfWard/NitroGrafx/tree/62920a05e9c7e31144d7e16b58797cdd8bc3789a):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[nitroswan](https://github.com/FluBBaOfWard/NitroSwan/tree/564e171fb93c2c11f98f38607493675727676c2c):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[picodrivetwl](https://github.com/DS-Homebrew/PicoDriveTWL/tree/4fcdb1f0bef46a4704b1870d787b362afa7bfec0):** Not bundled: Pico and DrZ80 headers permit noncommercial use, but complete redistribution coverage of the inherited components/source is not confirmed. The pinned configuration selects DrZ80, not MZ80; retaining that distinction does not establish a complete distribution grant. The shared installer downloads the unchanged upstream binary with the noncommercial option.
- **[pokemini](https://github.com/DS-Homebrew/PokeMini/blob/f8eb158f7082d80739075baa3653afc14fdf3d70/source/PokeMini.c):** FreeBIOS is included; retain its source and upstream licensing notices.
- **[rocketvideoplayer](https://github.com/RocketRobz/RocketVideoPlayer/tree/50803b9027ca43ab278c1486442fcfc70c90b309):** README credits Nintendo DSi fonts/graphical assets and borrowed FastVideoDS code. MIT on project code does not establish all asset/component redistribution rights.
- **[s8ds](https://github.com/FluBBaOfWard/S8DS/tree/90f2c13368f0b2ceac7887802e77d516d6337436):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.
- **[scummvm](https://github.com/scummvm/scummvm/blob/v2.9.1/COPYING):** DS main forwards argc/argv to scummvm_main. Stock 2.9.1 supports --add --recursive --path and --no-exit. Gloam requests discovery under /roms/ScummVM/titles when opening /roms/ScummVM/scummvm.nds without explicit arguments. Set paths in plugins/startup/scummvm.ini; ScummVM owns detection, entries and saves. Official downloads retain 2.9.1 for DSi. Manual installation until the complete platform/resource/source package is pinned. DSi launch compatibility remains unverified.
- **[snemulds](https://github.com/cotodevel/SnemulDS/tree/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd):** Pinned older TGDS1.65 commit matches both hashes documented by shim v0.15; do not track the moving branch.
- **[snemulds-shim](https://github.com/Sanrax/snemulds-shim/tree/2a7b3ed08a72d28529662cfc9ede8d3e83c2095b):** Retain third_party/NOTICE.md and all vendored license texts.
- **[speccyse](https://github.com/wavemotion-dave/SpeccySE/tree/85c85862470b69b75f6e49476a9c4ce992064bb3):** No-profit redistribution permitted with full author/core notices. Preserve the Z80, sound and lzav terms.
- **[stellads](https://github.com/wavemotion-dave/StellaDS/tree/1ff41dc60c4debacd4493bae5bb045cdbbef7800):** Retain LICENSE and Copyright.txt, including the Aaron Giles notice.
- **[sugards](https://github.com/wavemotion-dave/SugarDS/tree/ca4b95960d55e24d1d5bf815f5fffcacad8e4704):** Built-in Amstrad ROMs rely on the quoted conditional permission; ParaDOS is separately copyrighted. Record the firmware grants before redistributing the prebuilt package.
- **[tuna-vids](https://github.com/DS-Homebrew/tuna-vids/tree/6781a1a7ac723cb1274de6bd8734b21a9fbbebc5):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs. The executable Helix notice gap and GPL/RPSL question remain documented; this is not a confirmed distribution grant.
- **[wasabids](https://github.com/FluBBaOfWard/WasabiDS/tree/b074708a4c0b670e9f3e7d520a776f2bad2695c1):** Not bundled: redistribution permission or complete component coverage remains unconfirmed. Use the shared downloader to obtain the pinned files directly from their recorded upstream/host URLs.

## SNEmulDS handoff

Keep `snemulds-shim.nds`, `snemulds-shim.ini`, `SNEmulDS.nds` and `SNEmulDS.srl`
together. The pinned TGDS1.65 payload matches the reviewed shim, including its
NTR/TWL route; moving branch builds are not interchangeable. The legacy optional
payload and DSP/S-DD1 variants are not silently substituted.

## BIOS and firmware

- **[a5200ds](https://github.com/wavemotion-dave/A5200DS/blob/b7bba6ca9ea2a4e411753ed2d54e33c8bc93fd1a/readme.md):** Altirra 5200 replacement is already compiled in; some games need user-supplied original 5200.rom in /roms/bios or /data/bios.
- **[a7800ds](https://github.com/wavemotion-dave/A7800DS/blob/32b7b1ed78f5edbcd29a797a1376e25a9bd9d265/README.md):** No BIOS required normally. Optional 7800.rom and highscore.rom go in /roms/bios, /data/bios or beside emulator.
- **[a8ds](https://github.com/wavemotion-dave/A8DS/blob/b5718df173e0b4037bcb8414640af26458c0bade/readme.md):** Built-in Altirra OS 3.41 and BASIC 1.58. Optional originals: atarixl.rom, atariosb.rom, ataribas.rom, a5200.rom in /roms/bios or /data/bios.
- **[colecods](https://github.com/wavemotion-dave/ColecoDS/blob/29596955995526a10c8a9ca8c17d39b642f54dbf/README.md):** ColecoVision requires coleco.rom; Sord M5 requires sordm5.rom (JP) or sordm5p.rom (PAL). MSX1 has built-in C-BIOS for cartridges, without BASIC/cassette/disk support. Additional machine BIOS files are user-supplied.
- **[dracods](https://github.com/wavemotion-dave/DracoDS/blob/03cddfbddf93056291c69fd512a8d8db0bbb2caa/README.md):** User supplies dragon32.rom or bas12.rom/extbas11.rom; disk11.rom for CoCo disks. Look in /roms/bios or beside emulator.
- **[ds994a](https://github.com/wavemotion-dave/DS994a/blob/85098fcfcaa176a919fa4f2162706ac7814ea263/README.md):** Requires 994aROM.bin and 994aGROM.bin in /roms/bios or /roms/ti99; 994aDISK.bin is needed for disk support.
- **[dslibris](https://github.com/nextcode4u/dslibris/blob/argv-launch/README.md):** No BIOS required. Supplied fonts use `/data/dslibris/font/`; books default to `/media/books/`. New settings use `/data/dslibris/dslibris.xml`; existing legacy settings remain in use unless a managed file exists.
- **[dsxp](https://github.com/nextcode4u/DSXP/blob/485805cffae5ad2cfa32efd6946cc7ec6aa94ae0/README.md):** No emulator BIOS required.
- **[fastvideods](https://github.com/Gericom/FastVideoDSPlayer/blob/62c909ebf6e509b11eedc2ad4e4549a717f0f6ab/readme.md):** No BIOS required; convert videos with the matching FastVideoDS encoder.
- **[gameyob-dsi](https://github.com/Lana-chan/GameYob/blob/04f88d96ca653c60fd1b110ce7011db7e8e906a3/README.txt):** No BIOS required for normal GB/GBC play. Optional GBC boot ROM affects boot/palettes; not included.
- **[gbarunner3](https://github.com/Gericom/GBARunner3/blob/ecaa817815d9761745606592f04affe5ee9c3731/README.md):** Requires original 16 KiB GBA BIOS at /_gba/bios.bin. Fixed-offset relocation/patches prevent assuming a replacement BIOS is compatible.
- **[gimlids](https://github.com/wavemotion-dave/GimliDS/blob/e99a7e8b2ad65b7dcb93dd5d9feb217eaccc5740/README.md):** Requires KERNAL.ROM, BASIC.ROM, CHAR.ROM and 1541.ROM in /roms/bios or /roms/c64. MEGA65 Open ROMs are not a verified complete replacement set for GimliDS.
- **[hachibitto](https://github.com/wavemotion-dave/Hachibitto/blob/a388a295945d3929ac41a6369e40ce1c0afefa7e/arm9/source/MSX_BIOS.c):** Stock binary embeds MSX/MSX2/MSX2+ and disk firmware. Optional Kanji font is user-supplied. C-BIOS is not a verified drop-in full replacement.
- **[jenesisdsplus](https://github.com/DS-Homebrew/jEnesisDSPlus/blob/ca3bb43d14f8d78ccb6546d753817defaac25937/README.md):** No external console BIOS needed for Genesis cartridge ROMs. Plus supports up to 7.5 MiB on DSi (4 MiB on DS debug hardware), with mapper support for ROMs above 4 MiB. Native console-SD compatibility is not yet validated in Gloam.
- **[lodjurds](https://github.com/FluBBaOfWard/LodjurDS/blob/1e8fe92f82bb0c8926e7fc6212139630f46e2e7b/README.md):** Requires a user-supplied Atari Lynx BIOS, selected inside LodjurDS. Upstream explicitly says it is needed to run games; not included.
- **[microds](https://github.com/wavemotion-dave/MicroDS/blob/9a74db2c6aa58df1cfe530f52e723d2aa2e74e09/README.md):** Requires MC10.BIN or MC10.ROM; optional MCX.BIN/MCX.ROM. Put them in /roms/bios or beside emulator.
- **[nesds](https://github.com/DS-Homebrew/NesDS/blob/928afd9ca3c846f06264dfb1be6a2edf44575b66/README.TXT):** NES cartridges need no external BIOS. FDS content requires disksys.rom; user-supplied.
- **[ngpds](https://github.com/FluBBaOfWard/NGPDS/blob/4dbcc210cdb519414ff20d2886645a530e298206/README.md):** Upstream says a real NGP/NGPC BIOS is needed for in-game saving. Select it in emulator settings; not included.
- **[nintv-ds](https://github.com/wavemotion-dave/NINTV-DS/blob/c03e2ab9f052013d0328d1ccd3144ed07a8b9ae6/README.md):** Requires exec.bin and grom.bin; optional ivoice.bin, ecs.bin and Tutorvision firmware. Configure BIOS location in emulator; /roms/bios is the recommended collection.
- **[nitrografx](https://github.com/FluBBaOfWard/NitroGrafx/blob/62920a05e9c7e31144d7e16b58797cdd8bc3789a/README.md):** PCE cartridge route needs no BIOS. CD-ROM games require a user-supplied CD system BIOS; CD files are not default argv associations.
- **[nitroswan](https://github.com/FluBBaOfWard/NitroSwan/blob/564e171fb93c2c11f98f38607493675727676c2c/README.md):** Built-in startup handling permits games without original firmware; optional WS/WSC/Crystal BIOS can be selected in the emulator.
- **[picodrivetwl](https://github.com/DS-Homebrew/PicoDriveTWL/blob/4fcdb1f0bef46a4704b1870d787b362afa7bfec0/README.md):** Genesis cartridges need no separate BIOS; Sega CD is not a configured route.
- **[pokemini](https://github.com/DS-Homebrew/PokeMini/blob/f8eb158f7082d80739075baa3653afc14fdf3d70/readme.txt):** Upstream replacement FreeBIOS is already compiled in; original Nintendo BIOS is not needed for basic operation.
- **[rocketvideoplayer](https://github.com/RocketRobz/RocketVideoPlayer/blob/50803b9027ca43ab278c1486442fcfc70c90b309/README.md):** No BIOS required; use matching .rvid conversion.
- **[s8ds](https://github.com/FluBBaOfWard/S8DS/blob/90f2c13368f0b2ceac7887802e77d516d6337436/README.md):** Configured SMS/GG/SG-1000 cartridge routes normally bypass BIOS. Other emulated machines can require their own BIOS selected inside S8DS.
- **[scummvm](https://docs.scummvm.org/en/latest/other_platforms/nintendo_ds.html):** No general console BIOS. Supply the required data files for each supported game; do not bundle commercial game data.
- **[snemulds](https://github.com/cotodevel/SnemulDS/blob/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd/README.md):** No separately installed SNES BIOS. This pack selects the normal/CX4 build, not DSP or S-DD1 variants.
- **[snemulds-shim](https://github.com/Sanrax/snemulds-shim/blob/2a7b3ed08a72d28529662cfc9ede8d3e83c2095b/README.md):** No console BIOS needed for the selected SNEmulDS builds. Keep shim INI and NTR/TWL executables together.
- **[speccyse](https://github.com/wavemotion-dave/SpeccySE/blob/85c85862470b69b75f6e49476a9c4ce992064bb3/readme.md):** Requires 48.ROM and 128.ROM beside emulator or in /roms/bios. OpenSE BASIC is a candidate for 48K only, not a verified full replacement for this required pair.
- **[stellads](https://github.com/wavemotion-dave/StellaDS/blob/1ff41dc60c4debacd4493bae5bb045cdbbef7800/README.md):** No separate Atari 2600 BIOS required.
- **[sugards](https://github.com/wavemotion-dave/SugarDS/blob/ca4b95960d55e24d1d5bf815f5fffcacad8e4704/readme.md):** Amstrad CPC firmware and ParaDOS are embedded in the stock binary, with separate terms; no extra download is needed, but embedding is not a free-BIOS license.
- **[tuna-vids](https://github.com/DS-Homebrew/tuna-vids/blob/6781a1a7ac723cb1274de6bd8734b21a9fbbebc5/README.md):** No BIOS required; AVI must contain Xvid/MPEG-4 video and MP3 audio in the player-specific format.
- **[wasabids](https://github.com/FluBBaOfWard/WasabiDS/blob/b074708a4c0b670e9f3e7d520a776f2bad2695c1/README.md):** No separate Supervision BIOS required.

## Free replacements

A8DS/A5200DS carry their documented Altirra replacements, ColecoDS has C-BIOS
for supported MSX1 cartridge use, and PokeMini has its replacement BIOS. Their
source and notices accompany permitted bundled selections. Those replacements
do not cover other machines' firmware. A free GBA BIOS is not assumed compatible
with GBARunner3's original-BIOS offset patches. Never distribute proprietary
external BIOS or games through the package or installer manifest.
