# Personal extension defaults

In Advanced settings, open **Settings → Games & Apps → Emulator profiles**, or **SELECT → Edit file
associations**. The editor lists loaded extensions plus the current file's
extension. Select an extension, then choose an application, restore the supplied
default, or disable its automatic association. The application picker validates
the selected homebrew against the current direct route before saving.

Resolution order is:

1. A per-file application choice.
2. A personal extension default or explicit automatic-association disable.
3. Supplied plugin profiles: longest matching suffix, then the most specific
   matching preferred folder. Equivalent handlers are deduplicated; remaining
   distinct choices open the handler picker.

Disabling affects automatic association only; an explicit per-file choice still
wins. Restoring removes the personal override, leaving supplied profiles,
applications, content and saves intact. Native DS extensions and `.argv` keep
their dedicated routes and cannot be overridden by this editor. Custom extension
names use 1–15 alphanumeric/underscore characters.

Personal files live at `_nds/dsi-frontend/plugins/user/<extension>.ini`. This is a
separate capacity of 32 personal records; it does not consume the supplied
profile directory's 32 slots. It uses the existing version-1 `[plugin]` format,
with `disabled=1` representing an explicit disabled association. Generated
application defaults use direct argv routing. Configure nds-bootstrap flags in
plugin profiles; this editor does not change those flags. Supplied profile files are never
rewritten. Writes use temporary/current/backup replacement; failed writes are
reported. This is not a FAT power-loss atomicity guarantee.

The view preserves the underlying browser and supports buttons and touch. When
opened from a content item, its extension is selected automatically. Missing
applications remain visible in the effective path and fail usefully at launch;
choosing a replacement does not move the original application or its data.

## Why this handler?

Choose **SELECT → Inspect item / path**, then press left/right to open **Why this
handler?** for an SD file. The two-screen explanation shows the matching rule,
application, configured backend/argv route and read-only preflight status.
Left/right returns to the path view; B returns to the item menu.

Inspection follows the same per-file, personal-extension, preferred-folder and
shared-extension resolution as launch. If multiple handlers remain, it explains
that a choice is required rather than selecting one silently. File icons continue
to use the resolved application's banner or an explicit icon override; covers
remain independent.

Successful nds-bootstrap preflight supplies the planned public/private or retail
save paths. Direct homebrew/emulator saves are marked application-managed: Gloam
does not invent their locations. Failed preflight leaves saves unresolved and
shows the reason. Inspection creates no saves, launch configuration or association
changes. It is a snapshot; an actual launch revalidates files and settings.
