Pokemon Mini
========================================

SD folder: /roms/Pokemon Mini/

Content: .min

Application: PokeMini

Argv in the audited upstream build: direct. 0.60-arg directly copies argv[1],
but tests argc >= 1 rather than > 1. Launch via a .min association; no-
argument safety is not established.

Firmware: Upstream replacement FreeBIOS is already compiled in; original
Nintendo BIOS is not needed for basic operation.

Add-on package: available; terms: GPL-3.0 + zlib notices.

Upstream: https://github.com/DS-Homebrew/PokeMini
Release: https://github.com/DS-Homebrew/PokeMini/releases/tag/0.60-arg

Launch a .min file through its association. PokeMini 0.60-arg reads argv[1]
with an overly broad argc check; no-argument launch safety has not been
established.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
