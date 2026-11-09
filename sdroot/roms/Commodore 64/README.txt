Commodore 64
========================================

SD folder: /roms/Commodore 64/

Content: .d64, .crt, .prg

Application: GimliDS, prepared direct loader

Argv in the audited upstream build: directory-only. ArgvReceived records the
basename and changes directory, then leaves a TODO for auto-loading. The
prepared argv build in the backup is different.

Firmware: Requires KERNAL.ROM, BASIC.ROM, CHAR.ROM and 1541.ROM in /roms/bios
or /roms/c64. MEGA65 Open ROMs are not a verified complete replacement set for
GimliDS.

Add-on package: manual / held; terms: GPL-2.0-or-later.

Upstream: https://github.com/wavemotion-dave/GimliDS
Release: https://github.com/wavemotion-dave/GimliDS/releases/tag/1.7a

The configured .d64/.crt/.prg associations expect the prepared argv-enabled
build. Stock GimliDS 1.7a only changes directory; choose/mount content inside
stock GimliDS. It is not automatically installed over the prepared build.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
