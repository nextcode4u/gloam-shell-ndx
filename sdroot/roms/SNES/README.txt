SNES
========================================

SD folder: /roms/SNES/

Content: .sfc, .smc

Application: SNEmulDS through the prepared launcher

Argv in the audited upstream build: shim.

Firmware: No console BIOS needed for the selected SNEmulDS builds. Keep shim
INI and NTR/TWL executables together.

Add-on package: available; terms: GPL-3.0-or-later + vendored notices.

Upstream: https://github.com/Sanrax/snemulds-shim
Release: https://github.com/Sanrax/snemulds-shim/releases/tag/v0.15

Use snemulds-shim.nds rather than launching SNEmulDS directly with normal
argv. Keep the shim INI, SNEmulDS.nds and SNEmulDS.srl together. Console-SD
use needs DSi/TWL mode and unlocked SCFG. The add-on pins the exact supported
NTR/TWL hashes; 0.6a is not bundled.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
