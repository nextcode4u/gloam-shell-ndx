ScummVM/titles
========================================

SD folder: /roms/ScummVM/titles/

Content: one directory per supported game containing its required original
game-data files

Application: ScummVM

Argv in the audited upstream build: native game-ID and discovery options; not
a ROM-file association.

Firmware: No general console BIOS. Supply the required data files for each
supported game; do not bundle commercial game data.

Add-on package: manual / held; terms: GPL-3.0-or-later + constituent notices.

Upstream: https://github.com/scummvm/scummvm
Release: https://www.scummvm.org/downloads/

Keep each complete game in its own subfolder. New supported titles are
detected the next time scummvm.nds is opened through Gloam. Folder names alone
do not identify a game. Existing configured IDs and saves are retained;
deleting or moving folders does not automatically remove or repair old
entries.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
