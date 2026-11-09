Commodore 64
========================================

SD folder: /roms/Commodore 64/

Content: .d64, .crt, .prg

Application: GimliDS argv fork

Argv in the audited upstream build: direct. Selected PRG, CRT and D64 startup
paths are implemented in the fork. Stock upstream 1.7a only changes directory.

Firmware: Requires KERNAL.ROM, BASIC.ROM, CHAR.ROM and 1541.ROM in /roms/bios
or /roms/c64. MEGA65 Open ROMs are not a verified complete replacement set for
GimliDS.

Add-on package: manual / held; terms: GPL-2.0-or-later.

Upstream: https://github.com/nextcode4u/GimliDS
Release: https://github.com/nextcode4u/GimliDS/releases

The configured .d64/.crt/.prg associations target the nextcode4u/GimliDS argv
fork. No versioned runtime release is pinned yet; build the fork manually.
Stock upstream 1.7a only changes directory.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
