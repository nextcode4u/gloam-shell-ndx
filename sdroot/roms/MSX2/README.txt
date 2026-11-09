MSX2
========================================

SD folder: /roms/MSX2/

Content: .mx2 cartridges, .mxd disks

Application: Hachibitto argv fork

Argv in the audited upstream build: direct. Fork handles selected .mx2
cartridges and .mxd disks. Cartridge startup has been exercised; disk startup
remains unverified. Stock 1.9 has no selected-file argv support.

Firmware: Stock binary embeds MSX/MSX2/MSX2+ and disk firmware. Optional Kanji
font is user-supplied. C-BIOS is not a verified drop-in full replacement.

Add-on package: manual / held; terms: Noncommercial core; firmware/asset
rights unresolved.

Upstream: https://github.com/nextcode4u/Hachibitto
Release: https://github.com/nextcode4u/Hachibitto/releases

The configured .mx2/.mxd associations target nextcode4u/Hachibitto.
Redistribution of its inherited embedded firmware remains unconfirmed, so no
binary is bundled. Build/install separately; do not replace it with stock 1.9,
which has no selected-file argv support.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
