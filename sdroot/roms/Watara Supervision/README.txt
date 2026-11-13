Watara Supervision
========================================

SD folder: /roms/Watara Supervision/

Content: .sv cartridges

Application: WasabiDS

Argv in the audited upstream build: direct. Temporary nextcode4u fork of
FluBBaOfWard/WasabiDS v0.2.6: initializes the emulated machine and initial ROM
before opening the Maxmod audio stream, fixing a startup data abort. Direct
argv loading is retained. Reassess upstream after it includes the fix;
physical DSi, full gameplay and audio-quality validation remain pending.

Firmware: No separate Supervision BIOS required.

Add-on package: direct upstream download with shared installer; terms:
Project-wide redistribution grant not found.

This application is not bundled because redistribution permission or component
coverage remains unconfirmed. Use the Python or PowerShell downloader in
addons.zip/addon-installer, or install directly from upstream. Existing files
are preserved by default.

Upstream: https://github.com/nextcode4u/WasabiDS
Release: https://github.com/nextcode4u/WasabiDS/releases/tag/v0.2.6-audio-init.1

Keep required companion files and saves together. Other emulator-supported
formats may need its own browser or a per-file application choice. Changing an
extension is not a file-format conversion.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
