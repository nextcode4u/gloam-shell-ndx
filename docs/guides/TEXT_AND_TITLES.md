# Text and internal title names

Drawing, clipping and wrapping now consume UTF-8 codepoints rather than bytes.
Invalid encodings produce safe fallback glyphs. Measurement and drawing use
the active face's pixel advances; legacy fonts retain seven-pixel cells.
Banner UTF-16 is converted without splitting a
surrogate pair or truncating inside an encoded character. Unsupported glyphs
remain a single `?`. Japanese kana, JIS kanji and associated symbols now use a
shared M+ bitmap fallback; full Unicode CJK/Korean and RTL shaping remain unsupported.
Displaying a title does not change the frontend interface language.

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

Long selected settings and title-option labels/values, folder/ROM names,
file-picker names, browser title details, tab labels and folder paths use the
same pixel-clipped horizontal loop. Text starts moving on selection, one pixel every three frames
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
allocates no buffers. Settings, title options and tabs own their selection
timestamps; browser and picker views reuse their idle counters. Rendering never
updates state or reads files. Host tests compare both repetitions across the exact loop boundary,
verify continued movement after wrapping, and cover UTF-8, reduced motion and
viewport/framebuffer bounds. Native checks use the ROM grid and long tab names.

## Preview metadata

Short type labels beside cover art and system icons wrap within the 49-pixel
column, using up to three lines at the normal theme font size. `SYSTEM APP`
therefore shows completely as two lines. File size, console and storage rows
sit below the reserved label area, so longer labels do not overlap them or the
tab rail. Selected filenames, paths and tab names retain their scrolling loop.

## Custom and legacy interface catalogs

For normal use, select an installed language as described [below](#selecting-an-external-interface-language).
`/_nds/dsi-frontend/language.ini` remains an optional custom UTF-8 catalog, used
when Interface language is **Custom / English** (`auto`). It is independent of
the banner-title language and theme font. It currently covers built-in tab labels,
Settings, reading-folder text, browser/footer hints, settings labels/help and
common dialog controls. Untranslated entries retain English; catalogs remain
partial rather than complete interface translations.

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
rendering. The shared Japanese fallback works with every theme; other unsupported CJK and RTL scripts still need more work.

Settings labels and help use stable `setting.<option>.label` and
`setting.<option>.help` keys. Section names, settings-dialog hints and common
values use `section.*`, `dialog.*` and `value.*`. Up to 416 additional entries
are allowed. Missing entries retain English; language files cannot change any
setting values or launch behavior. Keep help within six display lines and test
long labels with scrolling enabled and Reduced motion.

[The editable English template](../../config/languages/template.ini) lists the
settings keys. [The Spanish catalog](../../config/languages/spanish.ini) covers
navigation, settings labels, sections and common hints/values; missing entries
fall back to English. For a custom override, copy the desired file to
`/_nds/dsi-frontend/language.ini`, select **Custom / English**, and restart. These are interface catalogs,
not the separate language passed to games or used for their title banners.

## Japanese titles and interface

Japanese names work automatically in both English and translated interfaces.
No theme conversion or console-language change is needed. Install the shared script font packs with BOOT.NDS.
A shared read-only M+ bitmap table supplies 6,817 kana, JIS kanji, punctuation
and halfwidth katakana glyphs at the existing 12-pixel body height. Fullwidth
glyphs advance 12 pixels, halfwidth kana advance six, and headings align this
fallback to their baseline. Wrapping, clipping and scrolling use those widths.
Rare characters outside the supplied set still use the fallback question mark.

The [Japanese catalog](../../config/languages/japanese.ini) translates
navigation, settings labels, common values and dialog controls. Select
**START → Appearance → Interface language → Japanese** to enable the shipped
`languages/ja.ini`; omitted entries retain English. Select **English** to keep
English menus while still displaying Japanese titles. Title language in settings selects
which banner field to read; it does not select the interface language.

[Font attribution and source details](../../third_party/mplus-bitmap/UPSTREAM.md).
Regenerate the shared table with `python3 tools/make_japanese_font.py`; `--check`
verifies the pinned input hashes and generated data without network access.

## External script fonts

Shared fallbacks live in `/_nds/dsi-frontend/fonts/japanese.gsf` and
`extended.gsf` (Latin Extended-A, Greek and Cyrillic). Install these alongside
BOOT.NDS when upgrading an older SD setup. Theme body/heading fonts remain
independent. The controller loads a script once when visible names or an
interface catalog need it; rendering performs no filesystem reads. Missing or
invalid packs use question marks and do not repeatedly reopen the failed file.
Recovery deliberately bypasses external scripts and retains built-in ASCII and
Latin-1. Returning to normal startup reloads scripts as needed.

`GSFONT01` uses a 20-byte little-endian header: eight-byte magic, glyph count,
FNV-1a payload checksum, height (12), coverage depth (1 or 2), and two zero bytes.
Each 29-byte record contains a 32-bit Unicode scalar, byte advance, and twelve
16-bit rows. Scalars must be unique and sorted; rows must fit the advance and
depth. Packs are bounded to 16,416 glyphs each. Checksums detect corruption,
not authenticity. Japanese and extended source attribution remains unchanged.
Regenerate with `tools/make_japanese_font.py` and `tools/pack_script_fonts.py`.

## Selecting an external interface language

START → Appearance → **Interface language** (also in desktop Setup settings)
selects English, Custom / English, or an installed catalog. `auto` retains the
legacy `language.ini` override; `en` always uses built-in English. Other IDs load
`/_nds/dsi-frontend/languages/<id>.ini`. Gloam 1.2.0 SD packages ship:

| ID | Interface catalog | Editable source |
| --- | --- | --- |
| `ja` | Japanese | [japanese.ini](../../config/languages/japanese.ini) |
| `es` | Spanish | [spanish.ini](../../config/languages/spanish.ini) |
| `fr` | French | [french.ini](../../config/languages/french.ini) |
| `de` | German | [german.ini](../../config/languages/german.ini) |
| `it` | Italian | [italian.ini](../../config/languages/italian.ini) |
| `pt-BR` | Brazilian Portuguese | [portuguese-brazilian.ini](../../config/languages/portuguese-brazilian.ini) |
| `nl` | Dutch | [dutch.ini](../../config/languages/dutch.ini) |

The **Interface language** selector requires Gloam 1.2.0 or later. When upgrading
from an earlier release, update the frontend and shared SD assets together.

These are partial catalogs, with English for missing entries. The five newer
catalogs include every registered label/control and selected help; longer help
and runtime messages may remain English. Native-speaker review is welcome.
No new fonts are needed for these five languages, and none of their catalog
text is compiled into BOOT.NDS. Unselected catalogs are not read at startup.
Older published SD packages may not include them; copy an editable source to
`/_nds/dsi-frontend/languages/<id>.ini` using the ID above to install it.

The setting persists and updates the built-in tab labels live;
custom tab labels and banner-title language remain independent. Recovery uses
English without replacing the saved selection.

Contributors can add another UTF-8 catalog using the existing version-1 keys;
no compilation is required. IDs contain 1–31 ASCII letters, digits, underscores
or hyphens. Settings discovers up to 16 external catalogs from a bounded scan of
256 directory records. Catalogs are capped at 32 KiB and rejected in full if
malformed. Extra scripts still require a matching shared font pack; adding a
translation does not imply complete Unicode coverage or RTL shaping.

## Contributor catalog audit

Run `python3 tools/audit_localization.py` for the shipped catalogs, or pass catalog
paths, `--themes` and `--fonts` for another collection. It reports registered-key
coverage, missing/unused keys, native byte/storage bounds, unavailable glyphs and
potential fixed-control width overflow against prepared body-font advances.
Coverage refers to registered translation keys, not every English runtime message.
Width warnings need native review: some labels intentionally loop. `--strict`
also rejects incomplete/unknown keys or missing glyphs. Partial starter catalogs
use normal mode. The JSON report defaults to ignored `build/localization-audit.json`.

UTF-8 catalogs accept an optional initial BOM and LF/CRLF lines. Embedded NULs,
invalid UTF-8 and oversized input are rejected before catalog state is applied.

Keep editable sources and shipped copies identical, then run the localization
audit tests using the project virtualenv. Validate new text on the console:
a passing parser/font check does not establish translation quality or LCD
readability.
