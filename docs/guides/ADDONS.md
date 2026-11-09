# Optional emulator and media add-ons

Reviewed 2026-11-08 against the exact upstream revisions below. This is a
source/release and redistribution audit, not a physical-DSi compatibility claim.
All 29 existing association profiles are covered, plus the SNEmulDS payload,
ScummVM and DSLibris as manually launched applications, and jEnesisDS Plus as
a Genesis alternative.
The recorded argv evidence is the actual content-loading path, not just an argc
parameter in main().

## Component review and distribution scope

ColecoDS 11.1 and DS994a 2.9 have explicit no-profit distribution statements.
Their unmodified binaries are available through `--noncommercial`, with original
READMEs, component notices and matching published source. This preserves the
ColEM contact wording rather than treating attribution alone as a prohibition.
DS994a's embedded speech-sample permission remains unconfirmed under the
[project assumption](UNCONFIRMED_DISTRIBUTION.md); no separate samples or games
are added. ColecoDS's C-BIOS notice is included; external machine BIOS files are
not supplied.

PicoDriveTWL v2.1.0 is also a noncommercial option. Its pinned configuration uses
DrZ80. MZ80 is not selected, and its translation unit is disabled by the pinned
build's preprocessor conditions. Keep the inherited MZ80 notice with the
unchanged source; this is not permission to modify and redistribute MZ80. The
Pico and DrZ80 headers and all original source notices remain in `source.zip`.
This is a source-configuration review, not an independent reproduction of the
upstream binary.

nesDS v2.1 is included under the documented assumption for unconfirmed newer
component terms. Its legacy public-domain notice does not license later code.
The source companion includes both the pinned RP2A03 submodule and its nested
ARM6502 submodule, with their original READMEs and credits.

**tuna-viDS v1.2 remains pending an executable-notice fix.** Its README supports
GPL/RPSL distribution, but [Helix RPSL section 2.1(e)](https://github.com/chishm/tuna-vids/blob/f1f00243b2f969ebc9ded1a4942f610819f8c673/arm7/source/helix/RPSL.txt)
requires source-availability information in the executable and the Exhibit A
Helix object-code notice where copyright notices appear. The stock binary and
startup source lack that notice. A release-ready downstream build must add the
notice and source location, record the change, retain GPL/Helix license texts
and provide matching player/codec source. The RCSL text is an alternative for
holders of a valid agreement, not an automatic replacement for the RPSL. This
is a concrete packaging obligation, not a finding that all redistribution is
forbidden. The current builder excludes it until such a build is prepared.

## GBARunner3 core and optional browser

The [supplied hicode ZIP](https://files.deletecat.com/GBARunner3-hicode.zip)
contains `GBARunner3.nds` and 304 game-specific JSON files under `/_gba/configs/`.
It contains no BIOS, license notice or build/source manifest. Download and core
hashes are recorded in the [dependency lock](../../config/addon-dependencies.json).
The archive name alone does not establish the matching hicode source revision;
301 of its 304 configs match the audited develop snapshot, which also does not
establish core provenance. Automatic packaging remains pending identification
of corresponding source, independently of the recorded redistribution assumption.

For manual use, put the core at
`/_nds/dsi-frontend/plugins/apps/GBARunner3.nds` to match Gloam's `.gba`
association, preserve `/_gba/configs/`, and supply your own original BIOS at
`/_gba/bios.bin`. This is a setup route, not a new launch-compatibility result.

The [flashcarts frontend v0.10.0-fcnet2](https://github.com/flashcarts/gbar3-frontend/releases/tag/v0.10.0-fcnet2)
is a separate browser derived from nds-hb-menu, with GPL-2.0-or-later notices.
Its default binary expects `/GBARunner3.nds`; the AOS and TWiLight variants use
their own fixed paths. It opens its own browser instead of accepting an incoming
selected-ROM argument. Gloam should continue passing the ROM directly to the core;
replacing that association with this frontend would open another browser.
The frontend source revision, binary hash and exception-stub submodule pin are
recorded alongside the core candidate. Redistributing it requires its GPL
notices and corresponding source; its license does not cover the separate core.

TWiLight Menu++'s app-launcher credits are useful attribution and source links,
not a single license covering every emulator. The supplied Virtual Console
archive contains 22 emulator binaries; eight match our pinned binaries exactly.
It also contains older ColecoDS 11.0, NINTV-DS 6.3 and SugarDS 1.5a builds.
Its TWiLight-specific paths and SNEmulDS payloads are not substitutes for Gloam's
reviewed profiles and matching shim/payload pair. No binaries from that archive
are automatically imported into this package.

## Build and install

```sh
python3 tools/build_addons.py
python3 tools/build_addons.py --skip-build --offline
python3 tools/build_addons.py --noncommercial
python3 tools/build_addons.py --list
```

The default includes eight projects with reviewed distribution terms: the
requested nextcode4u DSXP fork, GameYob, A7800DS, A8DS, StellaDS, PokeMini,
snemulds-shim and its matching SNEmulDS snapshot. It also includes nine
available releases under an explicitly documented, unconfirmed redistribution
assumption: FastVideoDS, jEnesisDS Plus, S8DS, NitroGrafx, NitroSwan, NGPDS,
LodjurDS, WasabiDS and nesDS. GBARunner3 has the same assumption recorded but no
automatically packaged binary; the supplied third-party hicode build is recorded
above. See [the assumption document](UNCONFIRMED_DISTRIBUTION.md).

`--noncommercial` adds A5200DS, DracoDS, MicroDS, SpeccySE, ColecoDS, DS994a
and PicoDriveTWL. Their no-profit
conditions remain separate from Gloam's GPL. Other held entries remain excluded.

`dist/addons.zip` is an optional SD overlay. Merge it after `sd.zip`; it installs
only reviewed emulator/player files at the paths used by the associations.
Back up customized shim settings before replacing its supplied INI. It does
not install games or proprietary BIOS, rewrite your association profiles or
replace your settings. Existing prepared GimliDS/Hachibitto binaries are not
overwritten by stock releases. Use the console association editor if your
applications live elsewhere. The default application folder is Gloam's
`/_nds/dsi-frontend/plugins/apps/`; DSXP's upstream `dsxp.nds` is installed as
`DSXP.nds` to match the existing profile.

Add-ons use the same `licenses.zip`, combined `source.zip` and `SHA256SUMS` as
the release. Available upstream source archives, including DSXP's provided libmad
source tarball and pinned source submodules, are retained under
`dependencies/addons/`. jEnesisDS Plus provides patches/injected code, not the
unpublished original emulator source. There is no separate
dependency-source download. Publish the updated companion archives with
`addons.zip`; the package manifest records the included versions and hashes.
A regular release build without add-ons removes a stale generated addons.zip
so a previous optional pack is not accidentally published with the wrong source.

## Releases, argv and distribution

Approved means the recorded terms permit this packaging with the retained
notices/source. Assumed means inclusion under the documented project assumption,
with permission still unconfirmed. Noncommercial requires the explicit build
flag. Held entries have a concrete missing grant, asset/component review, release or integration
requirement; this does not assert that their authors cannot distribute them.

| Project / release | Argv in this build | Packaging / recorded terms |
| --- | --- | --- |
| [a5200ds 3.9](https://github.com/wavemotion-dave/A5200DS/releases/tag/3.9) | [direct](https://github.com/wavemotion-dave/A5200DS/blob/b7bba6ca9ea2a4e411753ed2d54e33c8bc93fd1a/arm9/source/main.c) | Opt-in noncommercial; Custom noncommercial redistribution |
| [a7800ds 5.2c](https://github.com/wavemotion-dave/A7800DS/releases/tag/5.2c) | [direct](https://github.com/wavemotion-dave/A7800DS/blob/32b7b1ed78f5edbcd29a797a1376e25a9bd9d265/arm9/source/main.c) | Included; GPL-2.0-or-later |
| [a8ds 4.2](https://github.com/wavemotion-dave/A8DS/releases/tag/4.2) | [direct](https://github.com/wavemotion-dave/A8DS/blob/b5718df173e0b4037bcb8414640af26458c0bade/arm9/source/main.c) | Included; GPL-2.0-or-later + Altirra notices |
| [colecods 11.1](https://github.com/wavemotion-dave/ColecoDS/releases/tag/11.1) | [direct](https://github.com/wavemotion-dave/ColecoDS/blob/29596955995526a10c8a9ca8c17d39b642f54dbf/arm9/source/colecoDS.c) | Noncommercial opt-in; Noncommercial redistribution with retained author/component notices |
| [dracods 1.9](https://github.com/wavemotion-dave/DracoDS/releases/tag/1.9) | [direct](https://github.com/wavemotion-dave/DracoDS/blob/03cddfbddf93056291c69fd512a8d8db0bbb2caa/arm9/source/DracoDS.c) | Opt-in noncommercial; MIT base + README noncommercial condition |
| [ds994a 2.9](https://github.com/wavemotion-dave/DS994a/releases/tag/2.9) | [direct](https://github.com/wavemotion-dave/DS994a/blob/85098fcfcaa176a919fa4f2162706ac7814ea263/arm9/source/DS99.c) | Noncommercial opt-in; Noncommercial redistribution with constituent permissions/notices |
| [dslibris v1.6.0 (prerelease)](https://github.com/rhaleblian/dslibris/releases/tag/v1.6.0) | [none](https://github.com/rhaleblian/dslibris/blob/309923e6e46f2f146229498d7d5c98d2e854aaca/source/main.cpp) | Manual / held; GPL-2.0-or-later code; mixed library/font package |
| [dsxp v1.1.0-sd-argv.1](https://github.com/nextcode4u/DSXP/releases/tag/v1.1.0-sd-argv.1) | [direct](https://github.com/nextcode4u/DSXP/blob/485805cffae5ad2cfa32efd6946cc7ec6aa94ae0/source/main.c) | Included; GPL-2.0-or-later |
| [fastvideods v0.0.1](https://github.com/Gericom/FastVideoDSPlayer/releases/tag/v0.0.1) | [direct](https://github.com/Gericom/FastVideoDSPlayer/blob/62c909ebf6e509b11eedc2ad4e4549a717f0f6ab/arm9/source/main.cpp) | Included, permission unconfirmed; Project license not stated |
| [gameyob-dsi v0.5.3b-mavica](https://github.com/Lana-chan/GameYob/releases/tag/v0.5.3b-mavica) | [direct](https://github.com/Lana-chan/GameYob/blob/04f88d96ca653c60fd1b110ce7011db7e8e906a3/arm9/source/main.cpp) | Included; MIT |
| [gbarunner3 No upstream release; develop source snapshot](https://github.com/Gericom/GBARunner3) | [direct](https://github.com/Gericom/GBARunner3/blob/ecaa817815d9761745606592f04affe5ee9c3731/code/core/arm9/source/main.cpp) | Third-party hicode candidate; exact build source unconfirmed; permission unconfirmed |
| [gimlids 1.7a](https://github.com/wavemotion-dave/GimliDS/releases/tag/1.7a) | [directory-only](https://github.com/wavemotion-dave/GimliDS/blob/e99a7e8b2ad65b7dcb93dd5d9feb217eaccc5740/arm9/source/main.cpp) | Manual / held; GPL-2.0-or-later |
| [hachibitto 1.9](https://github.com/wavemotion-dave/Hachibitto/releases/tag/1.9) | [none](https://github.com/wavemotion-dave/Hachibitto/blob/a388a295945d3929ac41a6369e40ce1c0afefa7e/arm9/source/Hachibitto.c) | Manual / held; Noncommercial core; firmware/asset rights unresolved |
| [jenesisdsplus v0.8.1](https://github.com/DS-Homebrew/jEnesisDSPlus/releases/tag/v0.8.1) | [direct](https://github.com/DS-Homebrew/jEnesisDSPlus/blob/ca3bb43d14f8d78ccb6546d753817defaac25937/newCode/source/main.s) | Included, permission unconfirmed; No explicit Plus redistribution license; closed-source base terms unconfirmed |
| [lodjurds v0.1.5](https://github.com/FluBBaOfWard/LodjurDS/releases/tag/v0.1.5) | [direct](https://github.com/FluBBaOfWard/LodjurDS/blob/1e8fe92f82bb0c8926e7fc6212139630f46e2e7b/source/Main.c) | Included, permission unconfirmed; Project-wide redistribution grant not found |
| [microds 1.1](https://github.com/wavemotion-dave/MicroDS/releases/tag/1.1) | [direct](https://github.com/wavemotion-dave/MicroDS/blob/9a74db2c6aa58df1cfe530f52e723d2aa2e74e09/arm9/source/MicroDS.c) | Opt-in noncommercial; MIT base + README noncommercial condition |
| [nesds v2.1](https://github.com/DS-Homebrew/NesDS/releases/tag/v2.1) | [direct](https://github.com/DS-Homebrew/NesDS/blob/928afd9ca3c846f06264dfb1be6a2edf44575b66/arm9/source/romloader_frontend.c) | Included under project assumption; Legacy public-domain statement; newer component grants unconfirmed |
| [ngpds v0.6.0](https://github.com/FluBBaOfWard/NGPDS/releases/tag/v0.6.0) | [direct](https://github.com/FluBBaOfWard/NGPDS/blob/4dbcc210cdb519414ff20d2886645a530e298206/source/Main.c) | Included, permission unconfirmed; Project-wide redistribution grant not found |
| [nintv-ds 6.4](https://github.com/wavemotion-dave/NINTV-DS/releases/tag/6.4) | [direct](https://github.com/wavemotion-dave/NINTV-DS/blob/c03e2ab9f052013d0328d1ccd3144ed07a8b9ae6/arm9/source/main.cpp) | Manual / held; Custom redistribution notice; restricted extras |
| [nitrografx v0.9.1](https://github.com/FluBBaOfWard/NitroGrafx/releases/tag/v0.9.1) | [direct](https://github.com/FluBBaOfWard/NitroGrafx/blob/62920a05e9c7e31144d7e16b58797cdd8bc3789a/source/Main.c) | Included, permission unconfirmed; Project-wide redistribution grant not found |
| [nitroswan v0.7.7](https://github.com/FluBBaOfWard/NitroSwan/releases/tag/v0.7.7) | [direct](https://github.com/FluBBaOfWard/NitroSwan/blob/564e171fb93c2c11f98f38607493675727676c2c/source/Main.c) | Included, permission unconfirmed; Project-wide redistribution grant not found |
| [picodrivetwl v2.1.0](https://github.com/DS-Homebrew/PicoDriveTWL/releases/tag/v2.1.0) | [direct](https://github.com/DS-Homebrew/PicoDriveTWL/blob/4fcdb1f0bef46a4704b1870d787b362afa7bfec0/arm9/source/main.cpp) | Noncommercial opt-in; Noncommercial Pico/DrZ80 cores with inherited component notices |
| [pokemini 0.60-arg](https://github.com/DS-Homebrew/PokeMini/releases/tag/0.60-arg) | [direct](https://github.com/DS-Homebrew/PokeMini/blob/f8eb158f7082d80739075baa3653afc14fdf3d70/platform/nds/PokeMini_NDS.c) | Included; GPL-3.0-or-later + zlib notices |
| [rocketvideoplayer v2.3.1](https://github.com/RocketRobz/RocketVideoPlayer/releases/tag/v2.3.1) | [direct](https://github.com/RocketRobz/RocketVideoPlayer/blob/50803b9027ca43ab278c1486442fcfc70c90b309/nds/arm9/source/main.cpp) | Manual / held; MIT code; Nintendo assets separately credited |
| [s8ds v1.1.8](https://github.com/FluBBaOfWard/S8DS/releases/tag/v1.1.8) | [direct](https://github.com/FluBBaOfWard/S8DS/blob/90f2c13368f0b2ceac7887802e77d516d6337436/source/Main.c) | Included, permission unconfirmed; Project-wide redistribution grant not found |
| [scummvm v2.9.1 (published DSi build)](https://www.scummvm.org/downloads/) | [game-ID options, not ROM-file association](https://github.com/scummvm/scummvm/blob/9d9b9e93108a276c551aeffa390169ccc5148e15/backends/platform/ds/dsmain.cpp) | Manual / held; GPL-3.0-or-later + constituent notices |
| [snemulds TGDS1.65 (shim-compatible snapshot)](https://github.com/cotodevel/SnemulDS/tree/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd) | [TGDS-only](https://github.com/cotodevel/SnemulDS/blob/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd/arm9/source/main.c) | Included; GPL-2.0-or-later |
| [snemulds-shim v0.15](https://github.com/Sanrax/snemulds-shim/releases/tag/v0.15) | [shim](https://github.com/Sanrax/snemulds-shim/blob/2a7b3ed08a72d28529662cfc9ede8d3e83c2095b/source/arguments.c) | Included; GPL-3.0-or-later + vendored notices |
| [speccyse 2.2](https://github.com/wavemotion-dave/SpeccySE/releases/tag/2.2) | [direct](https://github.com/wavemotion-dave/SpeccySE/blob/85c85862470b69b75f6e49476a9c4ce992064bb3/arm9/source/SpeccySE.c) | Opt-in noncommercial; Custom noncommercial redistribution + core notices |
| [stellads 8.4a](https://github.com/wavemotion-dave/StellaDS/releases/tag/8.4a) | [direct](https://github.com/wavemotion-dave/StellaDS/blob/1ff41dc60c4debacd4493bae5bb045cdbbef7800/arm9/source/main.cpp) | Included; GPL-2.0-or-later + BSD notices |
| [sugards 1.6](https://github.com/wavemotion-dave/SugarDS/releases/tag/1.6) | [direct](https://github.com/wavemotion-dave/SugarDS/blob/ca4b95960d55e24d1d5bf815f5fffcacad8e4704/arm9/source/SugarDS.c) | Manual / held; GPL-2.0-or-later code; separately copyrighted firmware |
| [tuna-vids v1.2](https://github.com/chishm/tuna-vids/releases/tag/v1.2) | [direct](https://github.com/chishm/tuna-vids/blob/f1f00243b2f969ebc9ded1a4942f610819f8c673/arm9/source/main.c) | Pending executable Helix/source notice; GPL + RPSL |
| [wasabids v0.2.6](https://github.com/FluBBaOfWard/WasabiDS/releases/tag/v0.2.6) | [direct](https://github.com/FluBBaOfWard/WasabiDS/blob/b074708a4c0b670e9f3e7d520a776f2bad2695c1/source/Main.c) | Included, permission unconfirmed; Project-wide redistribution grant not found |

The companion license archive carries the assumption document and available
upstream notices for every selected unconfirmed entry. This does not establish
a missing grant or apply Gloam's GPL to those programs.

## Conditions and exceptions

- **[a5200ds](https://github.com/wavemotion-dave/A5200DS/tree/b7bba6ca9ea2a4e411753ed2d54e33c8bc93fd1a):** Distribution without profit is explicitly permitted with complete notice and thanks to Alekmaul and wavemotion-dave.
- **[a7800ds](https://github.com/wavemotion-dave/A7800DS/tree/32b7b1ed78f5edbcd29a797a1376e25a9bd9d265):** README explicitly adopts ProSystem GPLv2-or-later; supply full GPL text and matching source.
- **[a8ds](https://github.com/wavemotion-dave/A8DS/tree/b5718df173e0b4037bcb8414640af26458c0bade):** Retain Atari800, Alekmaul, wavemotion-dave and Avery Lee notices.
- **[colecods](https://github.com/wavemotion-dave/ColecoDS/tree/29596955995526a10c8a9ca8c17d39b642f54dbf):** README explicitly permits no-profit distribution of the emulator with notices and credits to wavemotion-dave, Alekmaul and Marat Fayzullin. Package the unchanged release only; preserve its ColEM contact request and FluBBa notices. No external proprietary BIOS is included.
- **[dracods](https://github.com/wavemotion-dave/DracoDS/tree/03cddfbddf93056291c69fd512a8d8db0bbb2caa):** Root LICENSE covers the MIT base; README adds a noncommercial condition for the derivative. Preserve both; do not label the whole app MIT.
- **[ds994a](https://github.com/wavemotion-dave/DS994a/tree/85098fcfcaa176a919fa4f2162706ac7814ea263):** README permits no-profit redistribution of the remaining code and records permission for SN76496 and Classic99-derived code. Package the unchanged release with lowzip/yxml and ColEM notices. Its embedded speech samples have no separately established redistribution grant; inclusion follows the project policy for this listed release, without claiming clearance for those samples.
- **[dslibris](https://github.com/rhaleblian/dslibris/blob/v1.6.0/source/main.cpp):** main(void) does not consume a selected-book argument. Manual app; complete bundled font/library notices and runtime Book/Font layout need review before automatic redistribution.
- **[dsxp](https://github.com/nextcode4u/DSXP/tree/485805cffae5ad2cfa32efd6946cc7ec6aa94ae0):** Use nextcode4u/DSXP SD-argv prerelease; matching release source includes libmad.
- **[fastvideods](https://github.com/Gericom/FastVideoDSPlayer/tree/62c909ebf6e509b11eedc2ad4e4549a717f0f6ab):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.
- **[gameyob-dsi](https://github.com/Lana-chan/GameYob/tree/04f88d96ca653c60fd1b110ce7011db7e8e906a3):** Retain the Drenn/contributor notice and both DS/DSi variants.
- **[gbarunner3](https://github.com/Gericom/GBARunner3/tree/ecaa817815d9761745606592f04affe5ee9c3731):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The supplied third-party hicode ZIP is hash-pinned as a candidate. Its exact source revision is unidentified; the recorded develop commit is reference source only. It is not automatically packaged yet.
- **[gimlids](https://github.com/wavemotion-dave/GimliDS/tree/e99a7e8b2ad65b7dcb93dd5d9feb217eaccc5740):** Code redistribution permitted, but the stock build only selects a directory. Not auto-installed over the prepared argv build. ArgvReceived records the basename and changes directory, then leaves a TODO for auto-loading. The prepared argv build in the backup is different.
- **[hachibitto](https://github.com/wavemotion-dave/Hachibitto/tree/a388a295945d3929ac41a6369e40ce1c0afefa7e):** Stock 1.9 has no argv handling. arm9/source/MSX_BIOS.c explicitly says the embedded MSX firmware is used without permission. Do not redistribute this prebuilt binary based on the emulator-code terms. Stock 1.9 main() has no argc/argv or system-argv handling. The prepared 1.8b backup build is different; do not overwrite it with stock.
- **[jenesisdsplus](https://github.com/DS-Homebrew/jEnesisDSPlus/tree/ca3bb43d14f8d78ccb6546d753817defaac25937):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL. saveArgv validates the standard _arg record, copies argv[1] and loadRomFromArgv starts the ROM. Original jEnesisDS does not establish the Plus behavior. Native DSi SD handoff still needs testing; do not infer it from the BlocksDS toolchain used for the injected code.
- **[lodjurds](https://github.com/FluBBaOfWard/LodjurDS/tree/1e8fe92f82bb0c8926e7fc6212139630f46e2e7b):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.
- **[microds](https://github.com/wavemotion-dave/MicroDS/tree/9a74db2c6aa58df1cfe530f52e723d2aa2e74e09):** Root LICENSE covers the MIT base; README adds a noncommercial condition for the derivative. Preserve both; do not label the whole app MIT.
- **[nesds](https://github.com/DS-Homebrew/NesDS/tree/928afd9ca3c846f06264dfb1be6a2edf44575b66):** Package the unchanged nesDS.nds release under the documented project assumption. Retain README.TXT, README_ORIGINAL.TXT, ARM7/Wi-Fi notices and the exact RP2A03 and nested ARM6502 source pins. Neither submodule pin has a project-wide license file; do not apply the legacy public-domain statement to those additions.
- **[ngpds](https://github.com/FluBBaOfWard/NGPDS/tree/4dbcc210cdb519414ff20d2886645a530e298206):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.
- **[nintv-ds](https://github.com/wavemotion-dave/NINTV-DS/tree/c03e2ab9f052013d0328d1ccd3144ed07a8b9ae6):** README permits emulator redistribution with attribution, but extras/overlays are explicitly personal-use-only. A source/runtime subset excluding those extras needs a separate completed audit.
- **[nitrografx](https://github.com/FluBBaOfWard/NitroGrafx/tree/62920a05e9c7e31144d7e16b58797cdd8bc3789a):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.
- **[nitroswan](https://github.com/FluBBaOfWard/NitroSwan/tree/564e171fb93c2c11f98f38607493675727676c2c):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.
- **[picodrivetwl](https://github.com/DS-Homebrew/PicoDriveTWL/tree/4fcdb1f0bef46a4704b1870d787b362afa7bfec0):** Package the unchanged upstream release for noncommercial distribution. Pinned config.h selects _USE_DRZ80, not _USE_MZ80; mz80.c is gated by ARM9_SOUND, which is not defined for that translation unit by the pinned build. Retain MZ80 terms in the unchanged source archive; this does not authorize distributing modified MZ80 or commercial use. Preserve the Pico, DrZ80 and other source notices.
- **[pokemini](https://github.com/DS-Homebrew/PokeMini/tree/f8eb158f7082d80739075baa3653afc14fdf3d70):** FreeBIOS is included; retain its source and upstream licensing notices. 0.60-arg directly copies argv[1], but tests argc >= 1 rather than > 1. Launch via a .min association; no-argument safety is not established.
- **[rocketvideoplayer](https://github.com/RocketRobz/RocketVideoPlayer/tree/50803b9027ca43ab278c1486442fcfc70c90b309):** README credits Nintendo DSi fonts/graphical assets and borrowed FastVideoDS code. MIT on project code does not establish all asset/component redistribution rights.
- **[s8ds](https://github.com/FluBBaOfWard/S8DS/tree/90f2c13368f0b2ceac7887802e77d516d6337436):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.
- **[scummvm](https://github.com/scummvm/scummvm/blob/v2.9.1/COPYING):** DS main forwards argc/argv to scummvm_main. Use its native game-list launcher; arbitrary game data is not a one-ROM association. Official downloads retain 2.9.1 for DSi while newer versions exist for other platforms. Manual installation until the complete platform/resource/source package is pinned.
- **[snemulds](https://github.com/cotodevel/SnemulDS/tree/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd):** Pinned older TGDS1.65 commit matches both hashes documented by shim v0.15; do not track the moving branch.
- **[snemulds-shim](https://github.com/Sanrax/snemulds-shim/tree/2a7b3ed08a72d28529662cfc9ede8d3e83c2095b):** Retain third_party/NOTICE.md and all vendored license texts.
- **[speccyse](https://github.com/wavemotion-dave/SpeccySE/tree/85c85862470b69b75f6e49476a9c4ce992064bb3):** No-profit redistribution permitted with full author/core notices. Preserve the Z80, sound and lzav terms.
- **[stellads](https://github.com/wavemotion-dave/StellaDS/tree/1ff41dc60c4debacd4493bae5bb045cdbbef7800):** Retain LICENSE and Copyright.txt, including the Aaron Giles notice.
- **[sugards](https://github.com/wavemotion-dave/SugarDS/tree/ca4b95960d55e24d1d5bf815f5fffcacad8e4704):** Built-in Amstrad ROMs rely on the quoted conditional permission; ParaDOS is separately copyrighted. Record the firmware grants before redistributing the prebuilt package.
- **[tuna-vids](https://github.com/chishm/tuna-vids/tree/f1f00243b2f969ebc9ded1a4942f610819f8c673):** Upstream README explicitly describes GPL v2 ARM9 and GPL v2/RPSL ARM7 distribution. RPSL 2.1(e) requires an executable source-availability notice plus the Exhibit A object-code notice. The stock v1.2 executable/startup source does not contain that Helix notice. Keep packaging pending a reproducible notice-bearing build; accompanying license files alone do not establish that the executable condition is met. This is a notice/source obligation, not a blanket prohibition on redistribution.
- **[wasabids](https://github.com/FluBBaOfWard/WasabiDS/tree/b074708a4c0b670e9f3e7d520a776f2bad2695c1):** Redistribution is assumed for packaging under the documented project policy; upstream permission remains unconfirmed. The pinned upstream binary is included without claiming a license grant or relicensing it under Gloam's GPL.

## SNEmulDS handoff

SNEmulDS 0.6d uses TGDS argv, so Gloam launches `snemulds-shim.nds`,
which translates normal argv and chainloads the emulator. The shim and its
INI stay beside `SNEmulDS.nds` and `SNEmulDS.srl`. The chosen upstream snapshot
is `d47b02a2a33da2f386b699cbcc9e6f03773c6cfd`, not the moving TGDS1.65 branch.
Both binaries match the hashes listed by shim v0.15:

- NTR: `09bc43aad372da4984155d77c24a30a8c0a8a668288aa834eeb0695f1359094b`
- TWL: `f2817e29db1958af6af2957d0ac7ccfce01b5a57f78e522527d1d508c121b7a2`

On DSi console SD, the shim supports TWL mode; its NTR choices require a
flashcart. DSi handoff requires unlocked SCFG. SNEmulDS 0.6a has no direct argv
and is not included. Its shim route changes the browser directory, not an
automatic selected-game launch. DSP/S-DD1 variants are not substituted.

## BIOS and firmware

- **[a5200ds](https://github.com/wavemotion-dave/A5200DS/blob/b7bba6ca9ea2a4e411753ed2d54e33c8bc93fd1a/readme.md):** Altirra 5200 replacement is already compiled in; some games need user-supplied original 5200.rom in /roms/bios or /data/bios.
- **[a7800ds](https://github.com/wavemotion-dave/A7800DS/blob/32b7b1ed78f5edbcd29a797a1376e25a9bd9d265/README.md):** No BIOS required normally. Optional 7800.rom and highscore.rom go in /roms/bios, /data/bios or beside emulator.
- **[a8ds](https://github.com/wavemotion-dave/A8DS/blob/b5718df173e0b4037bcb8414640af26458c0bade/readme.md):** Built-in Altirra OS 3.41 and BASIC 1.58. Optional originals: atarixl.rom, atariosb.rom, ataribas.rom, a5200.rom in /roms/bios or /data/bios.
- **[colecods](https://github.com/wavemotion-dave/ColecoDS/blob/29596955995526a10c8a9ca8c17d39b642f54dbf/README.md):** README explicitly permits no-profit distribution of the emulator with notices and credits to wavemotion-dave, Alekmaul and Marat Fayzullin. Package the unchanged release only; preserve its ColEM contact request and FluBBa notices. No external proprietary BIOS is included.
- **[dracods](https://github.com/wavemotion-dave/DracoDS/blob/03cddfbddf93056291c69fd512a8d8db0bbb2caa/README.md):** User supplies dragon32.rom or bas12.rom/extbas11.rom; disk11.rom for CoCo disks. Look in /roms/bios or beside emulator.
- **[ds994a](https://github.com/wavemotion-dave/DS994a/blob/85098fcfcaa176a919fa4f2162706ac7814ea263/README.md):** README permits no-profit redistribution of the remaining code and records permission for SN76496 and Classic99-derived code. Package the unchanged release with lowzip/yxml and ColEM notices. Its embedded speech samples have no separately established redistribution grant; inclusion follows the project policy for this listed release, without claiming clearance for those samples.
- **[dslibris](https://github.com/rhaleblian/dslibris/blob/v1.6.0/INSTALL.txt):** No BIOS required. Select EPUB files inside the app and preserve its Book/Font configuration.
- **[dsxp](https://github.com/nextcode4u/DSXP/blob/485805cffae5ad2cfa32efd6946cc7ec6aa94ae0/README.md):** No emulator BIOS required.
- **[fastvideods](https://github.com/Gericom/FastVideoDSPlayer/blob/62c909ebf6e509b11eedc2ad4e4549a717f0f6ab/readme.md):** No BIOS required; convert videos with the matching FastVideoDS encoder.
- **[gameyob-dsi](https://github.com/Lana-chan/GameYob/blob/04f88d96ca653c60fd1b110ce7011db7e8e906a3/README.txt):** No BIOS required for normal GB/GBC play. Optional GBC boot ROM affects boot/palettes; not included.
- **[gbarunner3](https://github.com/Gericom/GBARunner3/blob/ecaa817815d9761745606592f04affe5ee9c3731/README.md):** Requires original 16 KiB GBA BIOS at /_gba/bios.bin. Fixed-offset relocation/patches prevent assuming a replacement BIOS is compatible.
- **[gimlids](https://github.com/wavemotion-dave/GimliDS/blob/e99a7e8b2ad65b7dcb93dd5d9feb217eaccc5740/README.md):** Requires KERNAL.ROM, BASIC.ROM, CHAR.ROM and 1541.ROM in /roms/bios or /roms/c64. MEGA65 Open ROMs are not a verified complete replacement set for GimliDS.
- **[hachibitto](https://github.com/wavemotion-dave/Hachibitto/blob/a388a295945d3929ac41a6369e40ce1c0afefa7e/arm9/source/MSX_BIOS.c):** Stock binary embeds MSX/MSX2/MSX2+ and disk firmware. Optional Kanji font is user-supplied. C-BIOS is not a verified drop-in full replacement.
- **[jenesisdsplus](https://github.com/DS-Homebrew/jEnesisDSPlus/blob/ca3bb43d14f8d78ccb6546d753817defaac25937/README.md):** No external console BIOS needed for Genesis cartridge ROMs. Plus supports up to 7.5 MiB on DSi (4 MiB on DS debug hardware), with mapper support for ROMs above 4 MiB. Native console-SD compatibility is not yet validated in Gloam.
- **[lodjurds](https://github.com/FluBBaOfWard/LodjurDS/blob/1e8fe92f82bb0c8926e7fc6212139630f46e2e7b/README.md):** Requires a user-supplied Atari Lynx BIOS, selected inside LodjurDS. Upstream explicitly says it is needed to run games; not included.
- **[microds](https://github.com/wavemotion-dave/MicroDS/blob/9a74db2c6aa58df1cfe530f52e723d2aa2e74e09/README.md):** Requires MC10.BIN or MC10.ROM; optional MCX.BIN/MCX.ROM. Put them in /roms/bios or beside emulator.
- **[nesds](https://github.com/DS-Homebrew/NesDS/blob/928afd9ca3c846f06264dfb1be6a2edf44575b66/README.TXT):** Package the unchanged nesDS.nds release under the documented project assumption. Retain README.TXT, README_ORIGINAL.TXT, ARM7/Wi-Fi notices and the exact RP2A03 and nested ARM6502 source pins. Neither submodule pin has a project-wide license file; do not apply the legacy public-domain statement to those additions.
- **[ngpds](https://github.com/FluBBaOfWard/NGPDS/blob/4dbcc210cdb519414ff20d2886645a530e298206/README.md):** Upstream says a real NGP/NGPC BIOS is needed for in-game saving. Select it in emulator settings; not included.
- **[nintv-ds](https://github.com/wavemotion-dave/NINTV-DS/blob/c03e2ab9f052013d0328d1ccd3144ed07a8b9ae6/README.md):** Requires exec.bin and grom.bin; optional ivoice.bin, ecs.bin and Tutorvision firmware. Configure BIOS location in emulator; /roms/bios is the recommended collection.
- **[nitrografx](https://github.com/FluBBaOfWard/NitroGrafx/blob/62920a05e9c7e31144d7e16b58797cdd8bc3789a/README.md):** PCE cartridge route needs no BIOS. CD-ROM games require a user-supplied CD system BIOS; CD files are not default argv associations.
- **[nitroswan](https://github.com/FluBBaOfWard/NitroSwan/blob/564e171fb93c2c11f98f38607493675727676c2c/README.md):** Built-in startup handling permits games without original firmware; optional WS/WSC/Crystal BIOS can be selected in the emulator.
- **[picodrivetwl](https://github.com/DS-Homebrew/PicoDriveTWL/blob/4fcdb1f0bef46a4704b1870d787b362afa7bfec0/README.md):** Package the unchanged upstream release for noncommercial distribution. Pinned config.h selects _USE_DRZ80, not _USE_MZ80; mz80.c is gated by ARM9_SOUND, which is not defined for that translation unit by the pinned build. Retain MZ80 terms in the unchanged source archive; this does not authorize distributing modified MZ80 or commercial use. Preserve the Pico, DrZ80 and other source notices.
- **[pokemini](https://github.com/DS-Homebrew/PokeMini/blob/f8eb158f7082d80739075baa3653afc14fdf3d70/readme.txt):** Upstream replacement FreeBIOS is already compiled in; original Nintendo BIOS is not needed for basic operation.
- **[rocketvideoplayer](https://github.com/RocketRobz/RocketVideoPlayer/blob/50803b9027ca43ab278c1486442fcfc70c90b309/README.md):** No BIOS required; use matching .rvid conversion.
- **[s8ds](https://github.com/FluBBaOfWard/S8DS/blob/90f2c13368f0b2ceac7887802e77d516d6337436/README.md):** Configured SMS/GG/SG-1000 cartridge routes normally bypass BIOS. Other emulated machines can require their own BIOS selected inside S8DS.
- **[scummvm](https://docs.scummvm.org/en/latest/other_platforms/nintendo_ds.html):** No general console BIOS. Supply the required data files for each supported game; do not bundle commercial game data.
- **[snemulds](https://github.com/cotodevel/SnemulDS/blob/d47b02a2a33da2f386b699cbcc9e6f03773c6cfd/README.md):** No separately installed SNES BIOS. This pack selects the normal/CX4 build, not DSP or S-DD1 variants.
- **[snemulds-shim](https://github.com/Sanrax/snemulds-shim/blob/2a7b3ed08a72d28529662cfc9ede8d3e83c2095b/README.md):** No console BIOS needed for the selected SNEmulDS builds. Keep shim INI and NTR/TWL executables together.
- **[speccyse](https://github.com/wavemotion-dave/SpeccySE/blob/85c85862470b69b75f6e49476a9c4ce992064bb3/readme.md):** Requires 48.ROM and 128.ROM beside emulator or in /roms/bios. OpenSE BASIC is a candidate for 48K only, not a verified full replacement for this required pair.
- **[stellads](https://github.com/wavemotion-dave/StellaDS/blob/1ff41dc60c4debacd4493bae5bb045cdbbef7800/README.md):** No separate Atari 2600 BIOS required.
- **[sugards](https://github.com/wavemotion-dave/SugarDS/blob/ca4b95960d55e24d1d5bf815f5fffcacad8e4704/readme.md):** Amstrad CPC firmware and ParaDOS are embedded in the stock binary, with separate terms; no extra download is needed, but embedding is not a free-BIOS license.
- **[tuna-vids](https://github.com/chishm/tuna-vids/blob/f1f00243b2f969ebc9ded1a4942f610819f8c673/README.md):** Upstream README explicitly describes GPL v2 ARM9 and GPL v2/RPSL ARM7 distribution. RPSL 2.1(e) requires an executable source-availability notice plus the Exhibit A object-code notice. The stock v1.2 executable/startup source does not contain that Helix notice. Keep packaging pending a reproducible notice-bearing build; accompanying license files alone do not establish that the executable condition is met. This is a notice/source obligation, not a blanket prohibition on redistribution.
- **[wasabids](https://github.com/FluBBaOfWard/WasabiDS/blob/b074708a4c0b670e9f3e7d520a776f2bad2695c1/README.md):** No separate Supervision BIOS required.

## Free replacements

- **Altirra:** A8DS already includes replacement OS/BASIC, and A5200DS has a
  replacement 5200 kernel. Preserve Avery Lee's permissive firmware notices;
  a separate original-Atari firmware download is not included.
- **PokeMini:** its replacement FreeBIOS is built in. The retained source
  includes `freebios/freebios.asm` and the project notices.
- **[C-BIOS](https://cbios.sourceforge.net/):** independently written MSX
  replacement under the 2-clause BSD license. ColecoDS already embeds it for
  MSX1. It is for cartridge software and does not supply BASIC, cassette or
  disk support. It is not a verified replacement for all firmware in Hachibitto.
- **[Cult-of-GBA BIOS](https://github.com/Cult-of-GBA/BIOS):** MIT replacement
  exists, but GBARunner3 relocates and patches hard-coded offsets in the original
  BIOS. Do not install it as `/_gba/bios.bin` and claim compatibility.
- **[MEGA65 Open ROMs](https://github.com/MEGA65/open-roms):** open C64 ROMs
  are available, but no complete matching KERNAL/BASIC/character/1541 set has
  been validated with GimliDS here. Not installed automatically.
- **[OpenSE BASIC](https://packages.debian.org/bookworm/opense-basic):**
  free Spectrum replacement, with limitations on 128K machines. Not a validated
  drop-in pair for SpeccySE's required 48.ROM and 128.ROM.

No unverified firmware is silently copied into an emulator's expected BIOS
filename. A file being downloadable or labelled freeware is not a firmware
redistribution grant. The conventional shared folder is `/roms/bios/`; GBA
uses `/_gba/bios.bin`. Follow each emulator's actual lookup/configuration rules.

The audited lock is [config/addon-dependencies.json](../../config/addon-dependencies.json).
It records release assets, exact source commits, argv evidence, BIOS notes and
distribution decisions. It is updated deliberately; the builder does not select
a moving latest release. See [associations](EMULATORS.md) for configured file
types and [building](../development/BUILDING.md) for main release outputs.
