Genesis
========================================

SD folder: /roms/Genesis/

Content: .md, .gen

Application: PicoDriveTWL

Argv in the audited upstream build: direct.

Firmware: Genesis cartridges need no separate BIOS; Sega CD is not a
configured route.

Add-on package: direct upstream download with shared installer; terms:
Noncommercial Pico/DrZ80; complete redistribution coverage unconfirmed.

This application is not bundled because redistribution permission or component
coverage remains unconfirmed. Use the Python or PowerShell downloader in
addons.zip/addon-installer, or install directly from upstream. Existing files
are preserved by default.

Upstream: https://github.com/DS-Homebrew/PicoDriveTWL
Release: https://github.com/DS-Homebrew/PicoDriveTWL/releases/tag/v2.1.0

Folder rules assign .md files here to Genesis ROMs; Markdown documents belong
in /media/books or need a per-file override. PicoDriveTWL remains the default.
jEnesisDS Plus v0.8.1 is an experimental, explicit download option: official
nds-bootstrap homebrew routing reached gameplay, but cold launches remain
unreliable and .md save-state filenames can be truncated. It is not configured
automatically. See the add-on guide for compatibility limits.

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
