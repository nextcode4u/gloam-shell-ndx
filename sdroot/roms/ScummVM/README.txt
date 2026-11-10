ScummVM
========================================

SD folder: /roms/ScummVM/

Content: scummvm.nds launcher; one game-data folder per title under titles/

Application: ScummVM

Argv in the audited upstream build: collection discovery.

Firmware: No general console BIOS. Supply the required data files for each
supported game; do not bundle commercial game data.

Add-on package: direct upstream download with shared installer; terms:
GPL-3.0-or-later + constituent notices.

Experimental download; select this application explicitly using --only /
-Only. restoring from the game list can stall during detection-plugin
unloading. Full dynamic Quit/return and physical DS/DSi behavior remain
unverified.

Upstream: https://github.com/nextcode4u/scummvm
Release: https://github.com/nextcode4u/scummvm/releases/tag/v2026.4.0-ds-discovery.1

Install the experimental ScummVM discovery build by explicitly selecting
scummvm in the shared downloader. Its command-line options support automatic
collection scanning. Restoring directly from its game list can stall; full
Quit/return and hardware behavior remain unverified. Existing configuration
and saves are preserved.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
