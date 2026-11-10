# Credits

Gloam Shell NDX exists thanks to the tools, research and software shared by the
Nintendo DS homebrew community.

## Design influences

- [AKMenu Next](https://github.com/coderkei/akmenu-next): responsive browsing,
  theme conventions, argv associations and cheat-format compatibility.
- [TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu): broad DS/DSi
  integration, animated banners and the established launcher ecosystem.
- [Pico Launcher](https://github.com/LNH-team/pico-launcher): compact
  launcher design. [HBMenu](https://github.com/devkitPro/nds-hb-menu) provides
  longstanding homebrew launch conventions.
- Firmux: an earlier deprecated project by me.
- [PowerShell File Organizer Scripts](https://github.com/nextcode4u/PowerShell-File-Organizer-Scripts/tree/93e08429d31d76bffe7db2c684296f333473ebd9):
  nextcode4u's A-Z/`#` grouping and flattening workflow. The setup tool uses an
  original ROM-aware implementation; the standalone scripts are not bundled.

These are design influences, not claims of shared authorship or endorsement.
Actual source reuse is identified separately below and in THIRD_PARTY.md.

## Software and data

- [BlocksDS](https://blocksds.skylyrac.net/), Wonderful Toolchain and their
  contributors: the development foundation and SDK/runtime libraries.
- [nds-bootstrap](https://github.com/DS-Homebrew/nds-bootstrap): ahezard,
  shutterbug2000, RocketRobz and all contributors; SD game-loading backend.
- [Simple DS Slot-1 Launcher](https://github.com/Lorenzooone/Simple-DS-Slot-1-Launcher):
  Lorenzooone, with upstream credits to TWiLight Menu++, Apache Thunder's
  NTR Launcher and Michael “Chishm” Chisholm's NitroHax.
- [nds-bootloader](https://github.com/coderkei/nds-bootloader) and
  [nds-hb-menu](https://github.com/devkitPro/nds-hb-menu): native homebrew
  handoff and return bootstub; inherited notices remain in third_party/.
- TWiLight Menu++ / Arisotura: ROM save-memory table.
- Lode Vandevenne / LodePNG and Sean Barrett / stb contributors: image decoding.
- DejaVu / Bitstream font contributors: built-in and theme bitmap faces.
- pixelpion: “Sentinel,” Copper's music, under the Pixabay Content License.

The [dependency inventory](THIRD_PARTY.md) records exact revisions, source links,
licenses and local changes. Theme CREDITS.md files identify asset ownership.

## Optional add-ons

The [add-on inventory](docs/guides/ADDONS.md) links every audited upstream
release and its terms. Packaged executables remain separate programs:

- DSXP: Farzin Abdi; nextcode4u's native-SD/argv fork.
- GameYob: Drenn and contributors; maple “mavica” syrup's maintained fork.
- Optional SameBoot GBC firmware: Lior Halphon and the SameBoy contributors;
  gbdev hardware definitions are CC0. Exact source and notices accompany the add-on.
- A7800DS, A8DS and StellaDS: wavemotion-dave and their credited upstream
  emulator teams; Avery Lee supplies the embedded Altirra replacement firmware.
- PokeMini: JustBurn, Team Pokeme and contributors, including the replacement FreeBIOS.
- SNEmulDS: Archeide and Coto; Sanrax supplies the normal-argv launcher shim.
- Opt-in noncommercial A5200DS, DracoDS, MicroDS, SpeccySE and ColecoDS: wavemotion-dave
  and the original authors named in each package's complete retained notices.

`licenses.zip` retains original author/license material and `source.zip`
contains the exact matching upstream source archives for the selected pack.
These programs are not relicensed under Gloam's original-source license.

Download-only applications retain their original authorship: Gericom and
contributors (GBARunner3/FastVideoDS), FluBBa and credited component authors
(S8DS, NitroGrafx, NitroSwan, NGPDS, LodjurDS, WasabiDS and newer nesDS cores),
Lordus/RocketRobz/xonn83 (jEnesisDS Plus), Chishm and DS-Homebrew contributors
(tuna-viDS), and wavemotion-dave and credited contributors (DS994a).
PicoDriveTWL credits Ryan FB, RocketRobz, xonn83 and its core authors.
These binaries are not bundled; the shared installer links directly to their
pinned downloads. See [the download-only scope](docs/guides/DOWNLOAD_ONLY_ADDONS.md).
