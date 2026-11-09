MSX2
========================================

SD folder: /roms/MSX2/

Content: .mx2 cartridges, .mxd disks

Application: Hachibitto, prepared direct loader

Argv in the audited upstream build: none. Stock 1.9 main() has no argc/argv or
system-argv handling. The prepared 1.8b backup build is different; do not
overwrite it with stock.

Firmware: Stock binary embeds MSX/MSX2/MSX2+ and disk firmware. Optional Kanji
font is user-supplied. C-BIOS is not a verified drop-in full replacement.

Add-on package: manual / held; terms: Noncommercial core; firmware/asset
rights unresolved.

Upstream: https://github.com/wavemotion-dave/Hachibitto
Release: https://github.com/wavemotion-dave/Hachibitto/releases/tag/1.9

The .mx2/.mxd conventions belong to the prepared argv build. Stock Hachibitto
1.9 accepts .rom/.dsk in its own browser and has no argv handler. Renaming a
file does not add argv support. The stock binary is not redistributed by this
add-on builder.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
