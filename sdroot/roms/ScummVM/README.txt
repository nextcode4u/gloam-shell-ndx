ScummVM
========================================

SD folder: /roms/ScummVM/

Content: scummvm.nds launcher; one game-data folder per title under titles/

Application: ScummVM

Argv in the audited upstream build: native game-ID and discovery options; not
a ROM-file association.

Firmware: No general console BIOS. Supply the required data files for each
supported game; do not bundle commercial game data.

Add-on package: manual / held; terms: GPL-3.0-or-later + constituent notices.

Upstream: https://github.com/scummvm/scummvm
Release: https://www.scummvm.org/downloads/

Place the official scummvm.nds here and each supported game in titles/<game>/.
Gloam uses plugins/startup/scummvm.ini to request native add/recursive/no-exit
discovery whenever this app is launched without explicit arguments. ScummVM
keeps its own configuration and saves. Unknown folders are skipped; moved or
removed entries need manual review. This requires a command-line-capable
ScummVM build; the published 2.9.1 Nintendo build targets DSi/DSi XL.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
