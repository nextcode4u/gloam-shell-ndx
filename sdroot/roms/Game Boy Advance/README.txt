Game Boy Advance
========================================

SD folder: /roms/Game Boy Advance/

Content: .gba

Application: GBARunner3

Argv in the audited upstream build: direct.

Firmware: Requires original 16 KiB GBA BIOS at /_gba/bios.bin. Fixed-offset
relocation/patches prevent assuming a replacement BIOS is compatible.

Add-on package: manual / held; terms: Project-wide license not established.

Redistribution is assumed for this pack; upstream permission remains
unconfirmed. See addons/UNCONFIRMED_DISTRIBUTION.md in the companion
licenses.zip.

Upstream: https://github.com/Gericom/GBARunner3
Release: https://github.com/Gericom/GBARunner3/releases

Keep per-game configuration under /_gba/configs as required by your GBARunner3
build. A downloadable replacement GBA BIOS is not automatically compatible
with its fixed-offset patches.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
