ScummVM
========================================

SD folder: /roms/ScummVM/

Content: scummvm.nds launcher; one game-data folder per title under titles/

Application: ScummVM

Argv in the audited upstream build: disabled in stock DS build; requires a
command-line-enabled build.

Firmware: No general console BIOS. Supply the required data files for each
supported game; do not bundle commercial game data.

Add-on package: manual / held; terms: GPL-3.0-or-later + constituent notices.

Upstream: https://github.com/scummvm/scummvm
Release: https://www.scummvm.org/downloads/

Keep scummvm.nds here and supported game data in titles/<game>/. The stock
2.9.1 DS build disables command-line parsing; use its Add Game interface.
Gloam can request automatic discovery through plugins/startup/scummvm.ini only
with a compatible CLI-enabled build. ScummVM owns its entries and saves; moved
or removed entries need manual review.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
