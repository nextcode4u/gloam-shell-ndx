# Configurable folder tabs

Edit `_nds/dsi-frontend/tabs.ini` on the SD card. The distributed file contains commented examples. Restart the frontend after editing it; ordinary restaging preserves your existing file.

```ini
[tabs]
version=1

[tab.snes]
label=SNES
path=sd:/roms/SNES

[tab.gameboy]
label=Game Boy
path=sd:/roms/Game Boy

[tab.music]
label=Music
path=sd:/media/music
```

Remove the leading semicolons from a complete example to enable it, or add a new section. This version adds folder tabs after the six built-ins: Home, Games, Apps, Media, System and Files. It does not replace/reorder those built-in destinations. Custom sections appear in file order.

## Fields and limits

| Field | Meaning |
| --- | --- |
| `[tab.id]` | Stable identifier, 1–31 ASCII letters/digits, `_` or `-`. Case-insensitive. Keep it unchanged when renaming/reordering a tab. |
| `label` | Display name, up to 63 bytes. Optional; defaults to the ID. Current UI glyph coverage remains ASCII. |
| `path` | Assigned folder. Accepts `sd:/roms/SNES`, `/roms/SNES` or `roms/SNES`; all resolve against the SD root. Spaces are supported without quotes. Put comments on separate lines. |
| `folder` | Alias for `path`; prefer one field per section. |

There are 48 total slots: six built-ins and up to 42 custom tabs. Configurations are limited to 64 KiB, with lines shorter than 1 KiB. Duplicate/reserved IDs, parent traversal, other device prefixes, invalid entries and entries beyond capacity are skipped with a startup notice. Unsupported/missing version declarations retain the built-in tabs and report a notice. Unknown option keys are ignored for forward-compatible extensions.

`home`, `games`, `apps`, `media`, `system` and `files` are reserved IDs. Reusing one does not override a built-in. Empty folders are allowed. If the assigned folder is absent or unreadable, its tab remains available and shows Folder unavailable plus its path; no folder or content is created automatically.

## Navigation and overflow

L/R moves between all loaded tabs. Holding a shoulder button repeats at the configured browser navigation rate. The six-row top rail scrolls as needed to keep the active tab visible. Up/down markers indicate more tabs beyond the current window; the header shows the current position and total. Wrap works in both directions between the first and last tab.

Long rail labels are clipped with an ellipsis inside the rail. The bottom header has more room for the active tab's label. Rendering never draws additional rows beyond the six-row viewport.

Each custom tab remembers its directory, selection and list scroll independently. B goes up within its assigned folder tree; B at its assigned root returns Home. Use Files for unrestricted browsing elsewhere on the SD. Custom tabs also appear as Home shortcuts and use the same browser views, icons, artwork and contextual actions as ordinary folder browsing.

## Saving and configuration edits

Saved custom-tab locations and the active tab are keyed by ID, not by their current position. Reordering sections or changing a label therefore preserves the correct tab's state. Removing the active ID falls back to Home. Changing a tab's assigned folder ignores an old saved location outside the new tree and opens the newly assigned folder instead.

The existing `state.ini` version and built-in numeric location records remain readable. New records use `active_tab=<id>` and `tab_location=<id>,<selected>,<scroll>,<path>`. These are navigation state, not the tab definitions. Editing `tabs.ini` is enough; do not hand-edit state to add tabs.

Removing a tab removes a shortcut, not its folder or content. Keep a copy of your configuration if you want to restore it later. Existing emulator profiles and system files are unaffected.

The template leaves extra-tab examples commented so users choose their own
folders. Tabs can be configured without moving or deleting any content.

X focuses the top-screen category rail. Up/Down or L/R moves the outlined
candidate and scrolls the rail; A opens it. B/X restores the original rail
window and leaves the browser selection unchanged. Ordinary L/R browsing still
changes tabs directly. Overview/Details choices are remembered independently
for each stable tab ID via `tab_detail=` records; the older global `detail=`
record supplies defaults for older state files.
