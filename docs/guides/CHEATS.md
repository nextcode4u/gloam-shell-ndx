# Title cheats

Select a DS title, then SELECT → Title cheats (also accessible from Games & Apps
settings while that title is selected). Up/Down selects, A toggles a code or
expands/collapses a folder, Y disables all and B/START saves and returns to the originating menu. Touch selects
a row and activates it on the second selection. The top screen shows the note
and whether a group is exclusive. Enabling one member clears its siblings.

The database is read-only. The frontend checks the four-byte game code and the
R4 CRC accumulator over the first 512 ROM bytes. This CRC omits the usual final
inversion; it is not the header's CRC16. Another revision cannot inherit a
selection just because its filename or title looks similar.

Database search order:

1. `_nds/dsi-frontend/extras/usrcheat.dat`
2. `_nds/TWiLightMenu/extras/usrcheat.dat`
3. `_nds/akmenunext/cheats/usrcheat.dat`

The first existing database is authoritative. A missing match does not silently
switch to another database. Records are bounded at 256 KiB and 256 entries,
including folder rows. Nested folders, invalid offsets/counts, incomplete strings
and unsupported code lengths are rejected. Strings currently display printable
ASCII; general cheat-note localization is not implemented. Databases larger
than the record limit remain usable when the selected record fits.

Selections are versioned records in
`_nds/dsi-frontend/games/<full-content-identity-hash>.cheats.ini`, separate from
the database. The full normalized content/database paths, game code, ROM CRC
and matched-record CRC are verified. Changed records start with all codes off.
The original database's default-selection bits are not applied automatically.

## Backend output

The nds-bootstrap adapter prepares the selected action-replay words followed by
the backend terminator, bounded to 16 KiB. It checks shared payload ownership
before changing anything. Existing active foreign `cheatData.bin` or widescreen
payloads stop this route with an explanation; they are not deleted. Our own
previous payload is replaced only when it matches our exact ownership snapshot.
Previous and latest snapshots remain in the frontend's private directory.
An empty selection clears only our own active codes to the terminator.

Selections are matched to the title/revision and stored separately. Effects
remain code/title-specific and require a compatible backend launch; selection
and payload generation alone do not establish that a cheat works in-game.
Synthetic fixtures contain original parser data, not a distributable cheat database.

The payload cap follows the current backend's ARM7 hook limit (0x4000), even
where other backend preparation paths accept larger temporary buffers. The
terminator is included in this bound.
