# Emulator and media associations

Gloam includes 29 grouped profiles covering 50 extensions. These defaults model
established argv association conventions, use upstream application filenames
and preserve console-mode choices. Profiles are editable defaults; application
compatibility depends on the chosen build. The optional
[add-ons audit](ADDONS.md) records exact releases, actual argv behavior, license
conditions and BIOS requirements; only reviewed packages enter `addons.zip`.

Profiles live under `/_nds/dsi-frontend/plugins/`. Their default executable paths
use Gloam's own `/_nds/dsi-frontend/plugins/apps/` directory.
On a fresh card, install the chosen application and adjust `application`,
`dsipath` and `ntrpath` through the association editor or INI files. Applications
may live elsewhere; keep each one's companion files/configuration intact.

Use Settings → Maintenance → Refresh metadata after changing profiles. Missing
application banners use a generic icon. Content boot, saves and return depend
on the application build; see [homebrew routes](HOMEBREW_LAUNCH.md).

## Collection folders and recommended applications

These are default collection folders and configured formats, not every format each application accepts.

| Collection folder | Application / route | Configured content |
| --- | --- | --- |
| `roms/` | Console games and homebrew games. |  |
| `roms/Nintendo DS/` | nds-bootstrap adapter | .nds |
| `roms/DSiWare/` | DSiWare adapter | .nds, .dsi |
| `roms/Game Boy/` | GameYob | .gb, .sgb |
| `roms/Game Boy Color/` | GameYob | .gbc |
| `roms/Game Boy Advance/` | GBARunner3 | .gba |
| `roms/NES/` | nesDS | .nes, .fds |
| `roms/SNES/` | SNEmulDS through the prepared launcher | .sfc, .smc |
| `roms/Genesis/` | PicoDriveTWL | .md, .gen |
| `roms/Master System/` | S8DS | .sms |
| `roms/Game Gear/` | S8DS | .gg |
| `roms/SG-1000/` | S8DS | .sg |
| `roms/PC Engine/` | NitroGrafx | .pce cartridge files |
| `roms/Pokemon Mini/` | PokeMini | .min |
| `roms/WonderSwan/` | NitroSwan | .ws, .wsc |
| `roms/Neo Geo Pocket/` | NGPDS | .ngp, .ngc |
| `roms/Atari/2600/` | StellaDS | .a26 |
| `roms/Atari/5200/` | A5200DS | .a52 |
| `roms/Atari/7800/` | A7800DS | .a78 |
| `roms/Atari/8-bit/` | A8DS | .atr, .xex |
| `roms/Atari/Lynx/` | LodjurDS | .lnx |
| `roms/ColecoVision/` | ColecoDS | .col |
| `roms/Intellivision/` | Nintellivision / NINTV-DS | .int |
| `roms/MSX/` | ColecoDS, for MSX1 | .msx |
| `roms/Sord M5/` | ColecoDS | .m5 |
| `roms/ZX Spectrum/` | SpeccySE | .z80, .tap, .tzx |
| `roms/TI99/` | DS994a | .rpk; some games also need matching disk files |
| `roms/Amstrad CPC/` | SugarDS | .dsk, compatible .cpr, .dan |
| `roms/Tandy CoCo/` | DracoDS | .ccc cartridges, .cas tapes |
| `roms/Dragon 32/` | DracoDS | .ccc cartridges, .cas tapes |
| `roms/Tandy MC-10/` | MicroDS | .c10 tapes; optional Alice .k7 tapes |
| `roms/Watara Supervision/` | WasabiDS | .sv cartridges |
| `roms/Commodore 64/` | GimliDS, prepared direct loader | .d64, .crt, .prg |
| `roms/MSX2/` | Hachibitto, prepared direct loader | .mx2 cartridges, .mxd disks |
| `roms/ScummVM/` | ScummVM | scummvm.nds launcher; game folders in titles/ |
| `apps/Homebrew Apps/` | Homebrew application/utility | .nds, .dsi, supported native containers |
| `roms/Homebrew Games/` | Homebrew game | .nds, .dsi, supported native containers |
| `media/music/` | [DSXP SD-argv fork](https://github.com/nextcode4u/DSXP) | .mp3 |
| `media/videos/` | tuna-vids / FastVideoDS / RocketVideoPlayer | .avi / .fv / .rvid, converted for the matching player |
| `media/books/` | Reader application (manual) | .epub; no argv association configured by default |

`roms/bios/` is a support location, not a game system. The Atari folder has separate 2600, 5200, 7800, 8-bit and Lynx collections. No old `gb`, `gba`, `nds`, `Music`, `Videos` or mixed `DS(i) Homebrew` aliases are added by this template.

## Exact preconfigured associations

Paths in the next table are relative to `_nds/dsi-frontend/plugins/apps/`. Full paths and flags are in the named profile files. Multiple extensions sharing the same settings are grouped; no original extension is dropped.

| Profile | Extensions | Default application | DS override |
| --- | --- | --- | --- |
| `a5200ds.ini` | `.a52` | `A5200DS.nds` | — |
| `a7800ds.ini` | `.a78` | `A7800DS.nds` | — |
| `a8ds.ini` | `.atr`, `.xex` | `A8DS.nds` | — |
| `colecods.ini` | `.col`, `.m5`, `.msx` | `ColecoDS.nds` | — |
| `dracods.ini` | `.cas`, `.ccc` | `DracoDS.nds` | — |
| `ds994a.ini` | `.rpk` | `DS994a.nds` | — |
| `dsxp.ini` | `.mp3` | `DSXP.nds` | — |
| `fastvideods.ini` | `.fv` | `FastVideoDS.nds` | — |
| `gameyob-dsi.ini` | `.gb`, `.gbc`, `.sgb` | `gameyob_dsi.nds` | `gameyob.nds` |
| `gbarunner3.ini` | `.gba` | `GBARunner3.nds` | — |
| `gimlids.ini` | `.crt`, `.d64`, `.prg` | `GimliDS.nds` | — |
| `hachibitto.ini` | `.mx2`, `.mxd` | `Hachibitto.nds` | — |
| `lodjurds.ini` | `.lnx` | `LodjurDS.nds` | — |
| `microds.ini` | `.c10`, `.k7` | `MicroDS.nds` | — |
| `nesds.ini` | `.fds`, `.nes` | `nesDS.nds` | — |
| `ngpds.ini` | `.ngc`, `.ngp` | `NGPDS.nds` | — |
| `nintv-ds.ini` | `.int` | `NINTV-DS.nds` | — |
| `nitrografx.ini` | `.pce` | `NitroGrafx.nds` | — |
| `nitroswan.ini` | `.ws`, `.wsc` | `NitroSwan.nds` | — |
| `picodrivetwl.ini` | `.gen`, `.md` | `PicoDriveTWL.nds` | — |
| `pokemini.ini` | `.min` | `PokeMini.nds` | — |
| `rocketvideoplayer.ini` | `.rvid` | `RocketVideoPlayer.nds` | — |
| `s8ds.ini` | `.gg`, `.sg`, `.sms` | `S8DS.nds` | — |
| `snemulds-shim.ini` | `.sfc`, `.smc` | `snemulds-shim.nds` | — |
| `speccyse.ini` | `.tap`, `.tzx`, `.z80` | `SpeccySE.nds` | — |
| `stellads.ini` | `.a26` | `StellaDS.nds` | — |
| `sugards.ini` | `.cpr`, `.dan`, `.dsk` | `SugarDS.nds` | — |
| `tuna-vids.ini` | `.avi` | `tuna-vids.nds` | — |
| `wasabids.ini` | `.sv` | `WasabiDS.nds` | — |

Example, preserving GameYob mode choices:

```ini
[plugin]
version=1
name=gameyob_dsi
extensions=gb,gbc,sgb
application=sd:/_nds/dsi-frontend/plugins/apps/gameyob_dsi.nds
dsipath=sd:/_nds/dsi-frontend/plugins/apps/gameyob_dsi.nds
ntrpath=sd:/_nds/dsi-frontend/plugins/apps/gameyob.nds
argv=1
bootstrap=0
```

All supplied associations declare `argv=1`. Optional `bootstrap=0` flags are retained where present. The launcher applies argv and route selection; direct homebrew and a separate nds-bootstrap adapter are implemented. Grouping keeps related settings together. Each application still needs its own content, save and return checks.

## Prepared builds and format conflicts

- **GimliDS and Hachibitto:** use the configured prepared argv builds. Stock
  GimliDS 1.7a only selects a directory; stock Hachibitto 1.9 has no argv handler. Stock builds may require argv adaptations. A stock binary with the same filename is not proof of equivalent argv behavior. Preserve the MSX2 `.mx2`/`.mxd` conventions from that setup.
- **SNES:** `.sfc` and `.smc` target `snemulds-shim.nds`. Keep `snemulds-shim.ini`, `SNEmulDS.nds` and `SNEmulDS.srl` together. Keep the existing root `snemul.cfg` and TGDS payload if that prepared setup uses them. The shim config mentions an optional legacy payload; install that payload separately only if your setup requires it.
- **GameYob:** retain both `gameyob_dsi.nds` and `gameyob.nds`; the selected console mode chooses the configured variant.
- **GBA:** GBARunner3 remains the configured `.gba` default and is fetched by the shared downloader, not bundled. GBARunner2 is also an alternative, but does not have a competing default profile.
- **Shared `.dsk`:** the default assigns it to SugarDS/Amstrad CPC. A CoCo, TI or MSX disk does not become CPC content because of its extension. Use the correct emulator's browser or the implemented per-file application picker; folder-based association overrides are not implemented yet.
- **Genesis alternative:** [jEnesisDS Plus v0.8.1](https://github.com/DS-Homebrew/jEnesisDSPlus/releases/tag/v0.8.1)
  has standard argv support and adds ROM support up to 7.5 MiB on DSi. Its
  binary is download-only through the shared Python/PowerShell installer.
  The native-SD route remains unverified; select it explicitly rather than
  replacing the PicoDriveTWL profile automatically.
- **Shared `.md`:** the Genesis association also matches Markdown filenames. The profile treats it as a ROM extension. A Markdown document may therefore display the PicoDrive icon unless its per-file application choice overrides that mapping.
- **Machine selection:** CoCo and Dragon share DracoDS, while `.c10`/`.k7` share MicroDS. A folder name alone does not prove the application selected the intended machine. Follow the emulator's instructions and preserve its configuration.
- **Video:** `.avi`, `.fv` and `.rvid` target their respective players. Arbitrary MP4/AVI files need the correct encoding/conversion; no universal video decoder is implied.
- **Books:** launch DSLibris as an application and select books inside it. It has no `.epub` argv profile, so none is invented here. Its existing Book/Font configuration may still be needed even though the frontend's browsing default is `media/books`.
- **ScummVM:** use the automatic collection discovery described below. Arbitrary game-data files are not ROM associations.
- Archives, generic `.bin`/`.rom` and unrelated disk formats are not assigned automatically. Extract/prepare content using the application's actual requirements; changing an extension is not a general conversion method.

Every ROM collection, the `roms/` root and shared BIOS folder includes a
plain-text `README.txt` with configured formats, argv behavior and firmware
notes. These are generated from the collection catalog and audited add-on data.

## ScummVM collection discovery

Keep the launcher beside the collection, with complete games one level down:

```text
roms/ScummVM/
  scummvm.nds
  titles/
    Game One/       required files for this game
    Game Two/       required files for another game
```

Copy the official DSi executable from its release archive. Gloam's template contains
setup notes and directories; the ScummVM binary is still a manual install. Keep any
required companion files from the matching release and its existing support paths.
SD/FAT paths are case-insensitive; this template retains the established `ScummVM`
folder spelling.

Edit `/_nds/dsi-frontend/plugins/startup/scummvm.ini` to set `application`, `titles`,
`auto_add=1` and `recursive=1`. On a normal, argument-free launch of that configured
`scummvm.nds`, Gloam supplies native `--add --recursive --path=<titles> --no-exit`
options. ScummVM detects supported games, saves newly added entries and opens its
normal game list. New folders are picked up on the next launch, with no scanning
cost during Gloam startup or ordinary folder browsing. Large collections can make
ScummVM's startup scan slow.

This uses the [stock 2.9.1 discovery implementation](https://github.com/scummvm/scummvm/blob/v2.9.1/base/commandLine.cpp)
and [DS argument forwarding](https://github.com/scummvm/scummvm/blob/v2.9.1/backends/platform/ds/dsmain.cpp),
with no ScummVM source fork. It requires a build retaining those command-line
options. Folder names alone are not games: unsupported/incomplete data is skipped.

ScummVM owns its game configuration, game options and saves. Gloam does not replace
its native `scummvm.ini`. Existing default target IDs are normally skipped rather
than overwritten; manually renamed targets or multiple editions can need review.
Removing/moving a folder does not delete or repair its old entry. Keep existing
configured paths, or update them in ScummVM when reorganizing an older installation.

Set `auto_add=0` to use the normal launcher without a scan. Missing collection
folders also fall back to the normal launcher. Explicit `.argv` shortcuts/game-ID
arguments take precedence. This integration applies when launched through Gloam;
launching the same executable from another menu does not apply Gloam's profile.
ScummVM startup is currently blocked in available DSi emulation, including direct
boots and launches without discovery options. Treat automatic discovery as a
prepared integration pending a compatible native startup path. Game detection,
play, saves and return behavior are not established; frontend tests cover argument
preparation and preservation of explicit choices.

## Updating an older installation

Earlier Gloam defaults pointed at `/_nds/akmenunext/plugins/apps/` because the
initial association list used the configured AKMenu reference setup. New
profiles and `addons.zip` use `/_nds/dsi-frontend/plugins/apps/`.

Copy existing emulator executables and their adjacent companion/configuration
files into the new folder if you want to keep those builds. Leave machine-wide
support data in the locations required by each emulator. For customized profiles,
update `application`, `dsipath` and `ntrpath` in Gloam's association editor or INI
files; preserve your other choices. Fresh default profiles come with `sd.zip`.
Refresh metadata after updating the profiles. There is no automatic deletion or
rewrite of another launcher's files or your customized profiles.

## Maintaining profiles

[config/emulator-presets.json](../../config/emulator-presets.json) defines the
extension mappings, mode choices and collection folders. Regenerate with:

```sh
python3 tools/make_emulator_presets.py
python3 tools/make_emulator_presets.py --check
```

Staging preserves edited profiles; review updates instead of overwriting personal
configuration. No games, BIOS files or optional emulator/player executables are
included in the base SD package. The optional `addons.zip` installs the reviewed
subset documented in [add-ons](ADDONS.md). nds-bootstrap and Slot-1 Launcher are separate
bundled launch backends, described in [building](../development/BUILDING.md).

Nintellivision needs its BIOS directory set explicitly if files live in `/data/bios`.
Emulator-specific save directories, firmware and media conversion remain the
application's responsibility. Generic `.bin`/`.rom`, archives and game-data
folders are not assigned automatically.
