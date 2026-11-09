NES
========================================

SD folder: /roms/NES/

Content: .nes, .fds

Application: nesDS

Argv in the audited upstream build: direct.

Firmware: NES cartridges need no external BIOS. FDS content requires
disksys.rom; user-supplied.

Add-on package: manual / held; terms: Legacy public-domain statement + newer
components.

Upstream: https://github.com/DS-Homebrew/NesDS
Release: https://github.com/DS-Homebrew/NesDS/releases/tag/v2.1

FDS images are disk software; supply your own disksys.rom to nesDS if using
.fds. Do not assign arbitrary .bin files to this system.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
