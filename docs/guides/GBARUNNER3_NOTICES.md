# GBARunner3 setup and source coverage

GBARunner3 is download-only: Gloam does not bundle the core, game configs or
third-party source/notice archives because its redistribution grant and exact
binary source revision remain unconfirmed. The shared Python/PowerShell
installer fetches the pinned [Deletecat hicode ZIP](https://files.deletecat.com/GBARunner3-hicode.zip)
directly onto the chosen SD root:

- `/_nds/dsi-frontend/plugins/apps/GBARunner3.nds`: unchanged core.
- `/_gba/configs/`: 304 hash-pinned game-specific JSON files.
- `/_gba/bios.bin`: original 16 KiB GBA BIOS, supplied by the user.

The existing `.gba` association launches the core directly. Static ARM9
disassembly verifies argv[1] selection in this binary; runtime launch/save
compatibility is not established by that check. The separate gbar3-frontend
browser is excluded. Existing custom configs are preserved unless the installer
is invoked with the overwrite option.

Credit Gericom and the GBARunner3 contributors. Original component notices
remain in the [upstream project](https://github.com/Gericom/GBARunner3).
The audited develop revision `ecaa817815d9761745606592f04affe5ee9c3731` is
reference-only, not verified corresponding source for the supplied hicode core.
The lock records `binary_source_commit: null`; it does not claim that this
revision or the [hicode branch](https://github.com/Gericom/GBARunner3/tree/feature/cache-hicode)
reproduces that binary. No upstream project-wide license is asserted.
