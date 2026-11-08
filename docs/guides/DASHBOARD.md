# Personal Home dashboard

Open **Settings → Browser & Home → Dashboard**. Up/Down selects a section, A
shows/hides it, and Left/Right changes its position. Touch selects a row and
activates it on a second selection. L/R changes the selected section's visible item count; All restores its full bounded list. Limits hide excess entries without removing pins, tabs or history. Changes save immediately; failures retain
the previous configuration. B/START returns to the previous view.

The four sections are Browse links, Folder tabs, Pinned items and Recent
requests. Pinned items use the existing Favorites collection. Press START to open frontend settings, including when every section is hidden.
Settings is not a dashboard entry.

Recent requests are a bounded, deduplicated list of 16 content paths. The most
recent request moves to the front. They are recorded after frontend preflight
and preference saving, before calling the hardware adapter. They do not prove
that an application started, ran successfully, or returned. No playtime is
invented. A backend failure can therefore leave a request in this list.

Turning off Recent hides the section and stops new recording; existing history
is retained. Y in the dashboard editor clears this history. It does not remove
pins, titles, covers or saves. History write failures do not prevent launching.
Missing targets stay visible with a Missing label. Open or SELECT → Locate
chooses a replacement; Remove shortcut removes only frontend references.
References in both pins and history are updated together. Per-title saves/options
and item-artwork identities are not automatically migrated. Folder tabs retain their existing configuration and independent paths.

`_nds/dsi-frontend/dashboard.ini` stores version 1, an `order=` permutation of
0–3, `visible=` booleans and optional `limits=` counts indexed by section ID.
A zero limit means All. Old version-1 files without limits retain that behavior. IDs are links=0, tabs=1,
pins=2 and recent=3. `_nds/dsi-frontend/recent.ini` contains version 1 and bounded
`request=<timestamp>,sd:/<content path>` records. Both use recoverable
current/temporary/backup replacement. Config/history loading tries a valid
backup if the current file is missing or invalid. Paths are normalized against
the mounted SD root and foreign devices/parent traversal are rejected.

This is the first configurable dashboard. Arbitrary custom sections, richer
widgets, separate pin collections and playtime tracking are not implemented.

System destinations can also be pinned from the System tab. Their stable
`system:` identities resolve Settings, Camera, Sound, PictoChat, Download Play,
stock Menu and Slot-1 through the same hardware route as the original entry.
A cold Home pin triggers lazy title discovery before handoff. Pinning does not
change the destination's boot compatibility or exit behavior.

Auto-launch recent is separately configured under Browser & Home: Off (default),
3, 5 or 10 seconds. Any input cancels the countdown; early SELECT recovery and
a disabled Recent section bypass it. A failed launch returns to browsing without
restarting the countdown. Configuration reload also bypasses auto-launch.
