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
-Only. Full dynamic Lure launch, save, cold restore and Quit/return pass in a
development emulator setup after the DSO exit-registry fix. Broader engine,
flashcard and physical DS/DSi behavior remain unverified.

Upstream: https://github.com/nextcode4u/scummvm
Release: https://github.com/nextcode4u/scummvm/releases/tag/v2026.4.1git-ds-discovery.2

Install the experimental ScummVM discovery build by explicitly selecting
scummvm in the shared downloader. Its command-line options support automatic
collection scanning. Dynamic save, cold restore and Quit/return pass for Lure
in a development emulator setup; broader engines and physical hardware remain
unverified. Existing configuration and saves are preserved.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
