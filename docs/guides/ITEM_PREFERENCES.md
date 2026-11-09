# Covers and per-file applications

Select a file or folder and press SELECT for its actions. **Choose cover PNG**
opens an independent SD picker with a lazy preview. A applies the selected PNG;
B visits the parent folder; START cancels. Selecting the already highlighted
row by touch also opens/applies it. Left/Right pages through the picker.

Images are referenced in place, never copied over content or renamed. Assignments
work for arbitrary files, including music, books, emulator content and `.argv`
shortcuts, and for folders. Limits remain 512x512 pixels and 1 MiB encoded PNG.
A missing/invalid assigned image falls back to automatic lookup:

1. Explicit per-item assignment.
2. `filename.ext.png` sidecar for a file.
3. Folder `cover.png`.
4. Static native NDS banner, where applicable.

**Clear cover assignment** restores automatic lookup. It does not delete the
source image or disable sidecar/folder art. Browser icons remain independent.
The picker deliberately shows PNG assets that ordinary browsing hides as covers.

**Choose app for this file** opens the same picker filtered to native homebrew
applications. The selected application must pass direct-route preflight before
it is saved. Later A launches it with the selected file as one argv[1], preserving
spaces. The application icon is used in the browser. This overrides automatic
extension profiles for this file only. **Clear application choice** restores
the extension/native/shortcut route. See [launch limits](HOMEBREW_LAUNCH.md).

An explicit application choice on an `.argv` file treats that shortcut as content
for the chosen application. Without such a choice, normal shortcut parsing still
applies. Nothing in this UI promises that every application understands every
file type. The [personal extension-default editor](ASSOCIATION_EDITOR.md) is available; Open with alternatives remain pending.

Both pickers preserve the originating tab, path, selection, scroll and action
menu. Cancel returns to that view. Applying an assignment reports the result;
B or A dismisses that message and returns to the same SELECT-menu row. Invalid images/applications and failed saves leave the
picker available with a status message. Pickers scan at most 16 directory
records per frame and display 512 matching entries per batch. L/R or the touch
header arrows reaches additional batches; Left/Right moves by seven rows.
Their entry buffer exists only while open. They respect the hidden-file setting
and do not change the underlying browser's remembered locations.

## Storage

Sparse records live in `_nds/dsi-frontend/items/<identity-hash>.ini`. Version 1
records contain `content`, `cover` and `application` SD-absolute paths. The full
normalized content identity must match on reading, so a hash collision does not
apply another item's data. The location is independent of the host drive used
for testing. Parent traversal and foreign-device paths are rejected.

Writers use temporary/current/backup replacement and refuse to overwrite an
invalid or mismatched existing identity. A valid backup can recover a bad/missing
current record. This is recoverable replacement, not FAT power-loss atomicity.
Clearing a field preserves other fields. Clearing both leaves a valid empty
record; no cover, application, ROM or save is removed. Moving content does not
currently relocate its assignment automatically.

The main browser also supports Left/Right page movement. Its page size follows
the selected row layout and clamps at the first/last item; Up/Down retain their
existing single-row wrap behavior. L/R shoulders still switch folder tabs.

PNG and JPEG are accepted for explicit, full-filename sidecar and folder covers. MP3 ID3v2.3/v2.4 APIC PNG/JPEG art is read within a bounded tag. A profile's optional `cover=sd:/...` supplies a final association default after individual/embedded/folder art. Four prepared image results (including decode failures) are cached in RAM against full source path, inode, size and mtime. Maintenance → Refresh metadata or Clear caches also discards persistent thumbnails, including same-size edits within the FAT timestamp interval. Ordinary browser rescans retain disk thumbnails. The cache format and limits are documented in ARTWORK_CACHE.md.

## Stale dashboard shortcuts

Home retains missing favorites and recent requests as `Missing: …`. A opens a
Locate picker; SELECT also offers Locate missing target and Remove shortcut.
Choose a replacement file with A, or choose the current folder with Y. START
cancels. Repair updates matching frontend favorites/history and preserves
content and saves in place. Remove shortcut removes matching Home references,
never the target file. A reported write failure restores the in-memory
references and attempts to restore both preference files; FAT power loss is
not guaranteed atomic. Per-content application/artwork and game records retain
their original identities and are not silently applied to a replacement title.


## Independent browser icons

SELECT → Choose browser icon assigns a 32×32 PNG (up to 64 KiB) to a file or
folder, with picker preview and validation. Clear browser icon returns to the
native banner, associated application icon or generic folder/file icon. The
per-item `icon=sd:/...` record is independent of `cover=` and `application=`;
clearing one preserves the others and never deletes the image. Custom icons
use binary transparency (alpha below 128 is transparent), RGB555 color and the
existing bounded visible-icon cache. A malformed custom icon falls back to the
file's banner/application icon. Custom PNG icons are static; native DSi banner
animation remains available when no custom icon is assigned.

## Default icons and cover placeholders

When no native, assigned or association icon is available, the browser uses the
same original 16-pixel symbol family as the top-screen cover placeholder and
file pickers. Symbols scale cleanly with small/large/grid layouts and use the
active theme's colours. List view remains text-only below.

Distinct symbols cover folders, games, applications, audio, video, books/PDF,
text documents, images, save data, settings/cheats, archives, shortcuts, system
actions and missing entries. Known ROM extensions also receive console labels.
Unknown formats keep a generic page; raw `.bin` is not assumed to be a game.
`.md` is text except within a `roms/` folder, where it represents Genesis content.
A cover placeholder shows its family and filename extension instead of a generic
NO COVER label. Names and file access paths remain unchanged.

This classification only describes a file. It does not add a player, emulator,
file association or launch route. Assigned artwork, actual native banners and
configured emulator/player icons retain their existing priority. The bundled
text/album cover demos use neutral versions of the same document/note symbols;
they remain real sidecar examples rather than theme-tinted runtime placeholders.

## Returning from title submenus

SELECT-menu children return to their parent instead of closing the whole menu.
B/START saves and leaves Title options or Title cheats, restoring the SELECT
menu's row and scroll position. Failed saves keep the editor open for retry.
Item information and assignment/availability messages return to the same menu;
press Back again to close SELECT. The association editor retains its own
options/list levels before returning to SELECT. File pickers still use B to
visit parent folders and START to cancel directly back to their originating view.
