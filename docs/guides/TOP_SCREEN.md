# Top-screen content

Development builds after 1.2.0 add **START → Appearance → Top screen**. The same
choice is available in desktop SD Setup → Settings. Existing configurations keep
**Theme**, preserving their original composition. This setting is global;
bottom-screen view, icon size and labels remain independent per tab.

| Choice | Content |
| --- | --- |
| Layout only | Always use the original composition, ignoring the theme content preset. |
| Theme | Use the theme's original layout or its optional content preset. |
| Context | Selected artwork/icon, resolved player availability, configured DS save slot and per-title override count, plus optional title details. |
| Neighbors | Previous, selected and next artwork from the current browser batch. |
| Folder overview | Current loaded folder/batch counts, file-type totals and up to three representative file covers. Selecting a subfolder does not recursively inspect it. |
| Home activity | Up to three published Home pins, collections or recent requests with available artwork. Hidden Home sections stay hidden. Outside Home, use Context. |
| Smart | Home activity on Home, Folder overview for a selected folder or empty library, and Context for other entries. |

Y switches to detailed context and back. L/R and X keep their navigation roles.
The presets reserve a bottom tab dock, including space for icon-tab captions;
they do not move touch targets or change the bottom browser. Theme components,
fonts, palette and backgrounds remain active. Cover preview Off avoids image
loading, and Reduced motion keeps scrolling labels static.

Context describes configured options and file presence, not a successful game
boot. The actual launch still validates headers, handlers and backend support.
Recent requests do not prove gameplay and are not playtime records. NAND saves
and application-managed emulator saves are not inspected or modified here.

## Optional title details

For a file `Example.nds`, the sidecar is `Example.nds.gloam.ini`. A folder uses
`folder.gloam.ini` inside that folder. Both are optional UTF-8 text files:

```ini
version=1
title=Example title
developer=Example studio
genre=Adventure
year=2026
description=A short description of this title.
```

Limits are 95 UTF-8 bytes for title, 63 for developer, 47 for genre, four digits
for a nonempty year, and 255 bytes for description. The whole file must fit in
2 KiB. Values occupy one line; comments start with `#` or `;`. Unknown or
duplicate fields, unsupported versions, invalid UTF-8, control characters and
oversized files discard the optional metadata without blocking launch. Missing
fields stay empty. Metadata never renames content, replaces covers or changes
associations. Normal browsing and PC search omit these sidecars; the ROM organizer
moves them with their content.

No metadata service is contacted automatically. Authors can supply their own
text or import details they have permission to use. Native banner names remain
independent of an optional descriptive title.

## Preparation and cost

No whole-library scan, artwork prefetch or metadata import runs at boot. After a
selection settles for seven frames, the controller prepares bounded state in
separate steps. Metadata is read once per selection; missing or malformed files
are not retried each frame. Rendering reads prepared state only.

Nearby covers reuse the existing three-image Shelf cache, not another image
bank. Current-folder summaries inspect at most the 512 published entries, never
descendants. Changing selection cancels pending image work and clears its text
snapshot. Recovery ignores these presets and optional metadata. Physical-device
latency and memory measurements are still needed alongside host checks.
