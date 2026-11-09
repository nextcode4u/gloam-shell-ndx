# Emulator and media associations

Gloam includes 31 grouped profiles covering 78 extensions and 79 configured handler entries. These defaults model
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
| `roms/Commodore 64/` | GimliDS argv fork | .d64, .crt, .prg |
| `roms/MSX2/` | Hachibitto argv fork | .mx2 cartridges, .mxd disks |
| `roms/ScummVM/` | ScummVM | scummvm.nds launcher; game folders in titles/ |
| `apps/Homebrew Apps/` | Homebrew application/utility | .nds, .dsi, supported native containers |
| `roms/Homebrew Games/` | Homebrew game | .nds, .dsi, supported native containers |
| `media/music/` | [DSXP SD-argv fork](https://github.com/nextcode4u/DSXP) | .mp3 |
| `media/videos/` | tuna-vids / FastVideoDS / RocketVideoPlayer | .avi / .fv / .rvid, converted for the matching player |
| `media/books/` | DSLibris BlocksDS argv fork | EPUB/MOBI/FB2, PDF text, documents, OPF/OEB, PML/TCR and text archives |

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
| `dslibris-books.ini` | EPUB/Kepub, FB2/ZIP, MOBI/AZW, OPF/OEB, PML/TCR | `dslibris-blocksds.nds` | — |
| `dslibris-documents.ini` | TXT/TEXT/MD/MARKDOWN, PDF/HTML, RTF/DOCX/ODT, HTMLZ/TXTZ | `dslibris-blocksds.nds` | — |
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

## Shared extensions and folder rules

Add `content_root` to a normal profile in `/_nds/dsi-frontend/plugins/` to prefer
that application for content in a folder and its descendants. The folder belongs
to the file being opened, not the application's installation directory. For example:

```ini
; picodrivetwl.ini
[plugin]
version=1
name=PicoDriveTWL
extensions=gen,md
content_root=sd:/roms/Genesis
application=sd:/_nds/dsi-frontend/plugins/apps/PicoDriveTWL.nds
argv=1
bootstrap=0
```

The DSLibris document profile also lists `md`, with
`content_root=sd:/media/books`. Rules use complete, case-insensitive SD folder
boundaries: `/media/books` includes subfolders, but not `/media/bookshelf`.
Malformed paths or parent traversal invalidate the profile. One root is supported
per profile; additional profiles can prefer the same application in other folders.

Resolution order is:

1. A saved per-file application assignment.
2. A personal extension default or disable rule in `plugins/user/`, which is global.
3. The longest matching extension suffix, then the deepest matching `content_root`.
4. If no folder decides it, a single registered handler launches directly; multiple
   different handlers open **Open with**. Equal-priority folder rules also ask.

Profiles without `content_root` remain eligible as ordinary extension handlers.
Two unscoped handlers for the same suffix ask rather than using filename order.
Repeated profiles with the same application and argv/backend flags count as one
choice. The chooser retains each profile's launch flags, launches only after A,
and changes no saved default; B/START returns to the same browser selection.
Existing per-file assignments and explicit `.argv` shortcuts bypass the chooser.
A countdown launch of an ambiguous recent file stops at the chooser instead of
guessing or restarting the countdown.
Ambiguous files remain visible in the supported-file filter and do not inherit an
arbitrary handler's banner or artwork. Missing applications report an error without
silently selecting another handler.

Use the extension editor's **Restore profile rules** to remove a personal global
default when you want folder rules and the chooser to apply again. Keep the matching
`BOOT.NDS` and profile templates together when updating: older frontend builds do
not understand folder rules and can choose the first duplicate extension globally.
The updater preserves edited/existing profiles; merge the new `content_root` lines
into preserved profiles when needed. Unmodified templates are also in `sd.zip`.

## Prepared builds and format conflicts

- **GimliDS and Hachibitto:** use the nextcode4u argv source forks with the configured filenames. GimliDS v1.7a-argv.1 is bundled with matching source/notices; Hachibitto v1.9-argv.1 is download-only through the shared installer with noncommercial opt-in, because inherited firmware permissions remain unconfirmed. Stock GimliDS 1.7a only selects a directory and stock Hachibitto 1.9 has no selected-file argv handler. Preserve the MSX2 `.mx2`/`.mxd` conventions.
- **SNES:** `.sfc` and `.smc` target `snemulds-shim.nds`. Keep `snemulds-shim.ini`, `SNEmulDS.nds` and `SNEmulDS.srl` together. Keep the existing root `snemul.cfg` and TGDS payload if that prepared setup uses them. The shim config mentions an optional legacy payload; install that payload separately only if your setup requires it.
- **GameYob:** retain both `gameyob_dsi.nds` and `gameyob.nds`; the selected console mode chooses the configured variant.
- **GBA:** GBARunner3 remains the configured `.gba` default and is fetched by the shared downloader, not bundled. GBARunner2 is also an alternative, but does not have a competing default profile.
- **Shared `.dsk`:** the default assigns it to SugarDS/Amstrad CPC. A CoCo, TI or MSX disk does not become CPC content because of its extension. Use the correct emulator's browser or the implemented per-file application picker; add a `content_root` to each handler profile, or use a per-file application choice.
- **Genesis alternative:** [jEnesisDS Plus v0.8.1](https://github.com/DS-Homebrew/jEnesisDSPlus/releases/tag/v0.8.1)
  has standard argv support and adds ROM support up to 7.5 MiB on DSi. Its
  binary is download-only through the shared Python/PowerShell installer.
  The native-SD route remains unverified; select it explicitly rather than
  replacing the PicoDriveTWL profile automatically.
- **Shared `.md`:** folder rules select DSLibris under `/media/books/` and PicoDriveTWL under `/roms/Genesis/`. Outside those folders, Gloam offers the registered handlers instead of guessing. The selected application receives the file through its own argv/backend profile.
- **Machine selection:** CoCo and Dragon share DracoDS, while `.c10`/`.k7` share MicroDS. A folder name alone does not prove the application selected the intended machine. Follow the emulator's instructions and preserve its configuration.
- **Video:** `.avi`, `.fv` and `.rvid` target their respective players. Arbitrary MP4/AVI files need the correct encoding/conversion; no universal video decoder is implied.
- **Books:** the DSLibris BlocksDS fork is bundled with fonts and direct argv profiles. Supported unprotected EPUB/MOBI/FB2, PDF text, document and OPF/OEB files open from `/media/books/`; see its [format guide](https://github.com/nextcode4u/dslibris/blob/argv-launch/DOCUMENT_FORMATS.md). OPF packages need their chapter files beside them. Fonts use `/data/dslibris/font/`. `.md` is shared with Genesis and resolved by the folder rules above.
- **ScummVM:** use the automatic collection discovery described below. Arbitrary game-data files are not ROM associations.
- Generic archives, generic `.bin`/`.rom` and unrelated disk formats are not assigned automatically. Extract/prepare content using the application's actual requirements; changing an extension is not a general conversion method.

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
The [stock DS port disables command-line parsing](https://github.com/scummvm/scummvm/blob/v2.9.1/backends/platform/ds/portdefs.h).
Its launcher can open in available development controls, but its default build
ignores the discovery options above. Use its normal Add Game interface and set
`auto_add=0` when using that binary. Automatic discovery requires a compatible
CLI-enabled DS build and remains a prepared integration pending native validation.
Frontend tests cover argument preparation and preservation of explicit choices;
passing options alone does not establish game detection, play or save support.

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
