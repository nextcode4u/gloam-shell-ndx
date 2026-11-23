# Title cheats

Select a DS title, then SELECT → Title cheats (also accessible from Games & Apps
in Advanced settings while that title is selected). Up/Down selects, A toggles a code or
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
and matched-record CRC are verified. Incomplete reads, duplicate identity fields
and invalid numeric fields reject the selection; a valid backup may be used.
Changed records start with all codes off.
The original database's default-selection bits are not applied automatically.
Saving first checks the complete existing record's version and title identity.
Unreadable, unsupported or ambiguous records and their backups stay untouched;
the editor reports a save failure so they can be inspected or restored manually.

## Backend output

The nds-bootstrap adapter prepares the selected action-replay words followed by
the backend terminator, bounded to 16 KiB. It checks shared payload ownership
before changing anything. Existing active foreign `cheatData.bin` or widescreen
payloads stop this route with an explanation; they are not deleted. Our own
previous payload is replaced only when its complete contents match our ownership
snapshot. Uncertain file metadata or ownership stops the handoff; unreadable
files are not treated as empty or absent.
Previous and latest snapshots remain in the frontend's private directory.
An empty selection clears only our own active codes to the terminator.

Selections are matched to the title/revision and stored separately. Effects
remain code/title-specific and require a compatible backend launch; selection
and payload generation alone do not establish that a cheat works in-game.
Synthetic fixtures contain original parser data, not a distributable cheat database.

The frontend payload cap is 16 KiB (0x4000), including the terminator. This
matches the backend's maximum retail ARM7 hook budget; DSiWare engine placement
and combined anti-piracy patches can reduce the available budget. A selection
fitting the frontend cap is not a guarantee that every backend route can apply it.

## Runtime validation

With unmodified official nds-bootstrap v2.17.0, development-emulator checks on
Super Mario 64 DS (USA, game code `ASME`, R4 header CRC `AEA63749`) cover:

- Infinite Lives: 99 lives in gameplay, with live RAM confirming the code value.
- Infinite Coins: the running title's coin counter receives 100.
- Refill Health (Select): the conditional target stays unchanged without Select
  and receives its expected value after the button press in a separate cold run.
- Cold-start selection persistence, followed by Disable all and a cold relaunch
  returning to the normal four lives.
- Empty-payload publication after disabling, exact ownership-snapshot agreement
  and a byte-identical source database.

Automated fixtures also check exclusive groups, revision mismatch, payload-size
boundaries, foreign regular/widescreen payload preservation and interrupted
staging. Physical DSi cheat effects, other title revisions, other code types and
DSiWare cheat execution still need separate checks. These results establish the
tested selection/launch path, not compatibility for an entire cheat database.
