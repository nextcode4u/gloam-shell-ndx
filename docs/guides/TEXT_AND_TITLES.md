# Text and internal title names

Drawing, clipping and wrapping now consume UTF-8 codepoints rather than bytes.
Invalid encodings produce safe fallback glyphs. Measurement and drawing use
the active face's pixel advances; legacy fonts retain seven-pixel cells.
Banner UTF-16 is converted without splitting a
surrogate pair or truncating inside an encoded character. Unsupported glyphs
remain a single `?`; this does not provide Japanese/Chinese/Korean glyph coverage
or translate the frontend's interface.

The existing 7×12 ASCII font appearance is unchanged. A built-in Latin-1 fallback
covers U+00A0–U+00FF, including accented Western European letters, even for older
ASCII theme packs. Its licensed source is DejaVu Sans Mono Bold. Format v1
`DSFONT01` packs remain supported unchanged. Optional `DSFONT02` packs include
95 ASCII glyphs followed by 96 Latin-1 glyphs in the same fixed envelope. Blank
space, nonbreaking space and soft hyphen are permitted; other blank glyphs,
foreign geometry, extra data and out-of-cell pixels are rejected.
Version-2 themes additionally support [DSFONT03 proportional fonts and separate
headings](THEME_FONTS.md), with bounded advances and line heights.

Generate the optional example and reproducible fallback table:

```sh
python3 tools/pack_theme_font.py /path/to/DejaVuSansMono-Bold.ttf \
  sdroot/_nds/dsi-frontend/themes/copper/fonts/latin.dsfont \
  --size 10 --baseline 10 --latin1 \
  --fallback-header arm9/source/ui/font_latin.h
```

The packer measures the actual monochrome ink and adjusts horizontal placement
only when the full glyph fits the fixed cell. It never crops or stretches a glyph. The optional
Latin pack does not replace Copper's existing ASCII face automatically.

## Internal names

START → Browser & Home → Title names chooses Filename or Internal. Title language
chooses a banner language, falling back to English then Japanese when that slot
is missing/empty. This is independent of the language passed to a game backend.
Sorting, selection identities, associations and launch/save paths still use the
actual filename/path. Titles containing dots are not treated as file extensions.

At most one visible native title is read per frame, after selection settles.
The selected entry is prioritized; failed reads are remembered until Refresh.
Text-only List has the same title support without loading browser icons. No full
library scan or title database is required at startup. Custom content `.argv`
files retain their own names rather than adopting the emulator's banner title.

## Selected overflow text

Long selected settings labels/values, folder/ROM names, file-picker names,
browser title details, tab labels and folder paths use the same pixel-clipped
horizontal loop. Text starts moving on selection, one pixel every three frames
(first movement within 50 ms at 60 Hz), with no initial delay. Three spaces in the active face separate the end from the next
beginning. There is no end hold, backwards movement or snap to the start;
the next beginning moves into view after the tail and keeps going.

The installed-library status beneath “NAND TITLE” moves one pixel per frame
(60 pixels per second), three times the standard label speed.

Only overflowing text moves. Unselected rows remain clipped and still. Reduced
motion disables scrolling; the settings top screen retains its full label/value.
Changing selection or tabs starts the new label at its beginning immediately. The tab rail follows the
focused tab while navigating and the active tab otherwise.

The helper reads UTF-8 boundaries, clips partial glyphs to the viewport and
allocates no buffers. Settings and tabs own their selection timestamps; browser
and picker views reuse their idle counters. Rendering never updates state or
reads files. Host tests compare both repetitions across the exact loop boundary,
verify continued movement after wrapping, and cover UTF-8, reduced motion and
viewport/framebuffer bounds. Native checks use the ROM grid and long tab names.

## Preview metadata

Short type labels beside cover art and system icons wrap within the 49-pixel
column, using up to three lines at the normal theme font size. `SYSTEM APP`
therefore shows completely as two lines. File size, console and storage rows
sit below the reserved label area, so longer labels do not overlap them or the
tab rail. Selected filenames, paths and tab names retain their scrolling loop.

## Partial interface catalogs

`/_nds/dsi-frontend/language.ini` is an optional UTF-8 catalog independent of the
banner-title language and theme font. It currently covers built-in tab labels,
Settings, reading-folder text and browser/footer hints. Other dialogs and
settings remain English; this is localization groundwork, not a full translation.

```ini
version=1
home=Accueil
games=Jeux
apps=Applications
settings=Réglages
```

Stable keys are `home`, `games`, `apps`, `media`, `system`, `files`, `settings`,
`reading_folder`, `browser_hint`, `tab_hint`, `installed_titles`, `empty_folder`.
Unspecified keys use English. User-defined tab labels remain user content.
Files are bounded to 32 KiB. Navigation and label values allow 95 UTF-8 bytes (63 for tab labels); setting help allows 255. Invalid UTF-8, unknown navigation keys, invalid translation
namespaces, duplicate keys, control characters and unsupported versions discard the whole
catalog. Early recovery bypasses it. Configuration export/import includes the
catalog, and configuration reset archives it. No language file is read while
rendering. Existing fonts still determine script coverage; CJK/RTL needs more work.

Settings labels and help use stable `setting.<option>.label` and
`setting.<option>.help` keys. Section names, settings-dialog hints and common
values use `section.*`, `dialog.*` and `value.*`. Up to 192 additional entries
are allowed. Missing entries retain English; language files cannot change any
setting values or launch behavior. Keep help within six display lines and test
long labels with scrolling enabled and Reduced motion.

[The editable English template](../../config/languages/template.ini) lists the
settings keys. [The Spanish starter](../../config/languages/spanish.ini) covers
navigation, settings labels, sections and common hints/values; most help and
runtime errors still fall back to English. Copy the desired file to
`/_nds/dsi-frontend/language.ini` and restart. These are interface catalogs,
not the separate language passed to games or used for their title banners.
