MSX
========================================

SD folder: /roms/MSX/

Content: .msx

Application: ColecoDS, for MSX1

Argv in the audited upstream build: direct.

Firmware: ColecoVision requires coleco.rom; Sord M5 requires sordm5.rom (JP)
or sordm5p.rom (PAL). MSX1 has built-in C-BIOS for cartridges, without
BASIC/cassette/disk support. Additional machine BIOS files are user-supplied.

Add-on package: available; terms: Noncommercial redistribution with retained
author/component notices.

Upstream: https://github.com/wavemotion-dave/ColecoDS
Release: https://github.com/wavemotion-dave/ColecoDS/releases/tag/11.1

ColecoDS MSX1 cartridge files use .msx in the configured profile. Its own
browser supports other content; .dsk is globally assigned to SugarDS and .cas
to DracoDS, so use a per-file override or the emulator browser for MSX media.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
