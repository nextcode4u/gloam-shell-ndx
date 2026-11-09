ScummVM
========================================

SD folder: /roms/ScummVM/

Content: ScummVM.nds launcher; game data lives in data/scummvm/games/

Application: ScummVM

Argv in the audited upstream build: game-ID options, not ROM-file association.

Firmware: No general console BIOS. Supply the required data files for each
supported game; do not bundle commercial game data.

Add-on package: manual / held; terms: GPL-3.0-or-later + constituent notices.

Upstream: https://github.com/scummvm/scummvm
Release: https://www.scummvm.org/downloads/

Start ScummVM as an app and use Add Game for a game-data directory here or
/data/scummvm/games. No arbitrary game-data extension is associated with
ScummVM. The published 2.9.1 Nintendo build targets DSi/DSi XL, not the
original DS/DS Lite.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
