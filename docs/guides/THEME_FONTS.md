# Theme fonts

Themes can replace body text with `[fonts] ui=fonts/ui.dsfont`. Version-2 themes
can also set `heading=fonts/heading.dsfont` for screen headings, the selected
item's top-screen title and dialog titles. Omit `heading` to inherit the body
face, or use `builtin` for either role. Fonts load during theme preparation;
drawing performs no file access or allocation.

Legacy DSFONT01/02 packs retain their 7×12 metrics. DSFONT03 adds proportional
advances and four-level grayscale coverage. Body packs must keep a 12-pixel
line height; heading packs accept 12–16 pixels. Headings use reserved title
bands and do not resize rows or touch targets. Every glyph advances 1–16 pixels;
ink outside that advance is rejected. Changing a face therefore changes text
density without letting it cross another control's bounds.

Create a pack with:

```sh
python3 tools/pack_theme_font.py source-font.ttf fonts/ui.dsfont --size 11 --baseline 9
```

For proportional body text and a larger, separate heading face:

```sh
python3 tools/pack_theme_font.py DejaVuSans.ttf fonts/ui.dsfont --proportional --latin1 --size 10 --baseline 10
python3 tools/pack_theme_font.py DejaVuSerif.ttf fonts/heading.dsfont --proportional --latin1 --size 14 --baseline 13 --height 16
```

These sizes are examples for those specific faces. Other fonts need their own
baseline and size checks. The proportional exporter preserves grayscale ink,
including negative bearings, and rejects horizontal or vertical overflow.
It does not reduce or crop an offending glyph. Four coverage levels smooth
edges against the actual screen surface; there is no RGB subpixel rendering.
Keep full-coverage stems and inspect small punctuation on both light/dark LCDs.

For legacy output, the authoring tool measures the actual monochrome ink, including hinted
bearings, before placing it in the cell. It shifts ink horizontally only when
needed to preserve the full glyph; advance and vertical baseline stay fixed.
Glyphs that cannot fit are rejected rather than cropped or stretched. Use the pinned Pillow dependency for reproducible output.
Include the source face's redistribution license alongside the pack.

Test long filenames, accent characters, settings values and every browser view.
Text measurement and drawing share UTF-8 decoding and pixel advances; unsupported
scripts fall back to `?`. A shared built-in face supplies Latin Extended-A,
Greek and Cyrillic characters beyond a theme pack's Latin-1 range. These
characters keep their 7×12 cells, aligned to the bottom of a taller heading line.
Missing or invalid external fonts reject the theme
candidate, keeping the current face. The built-in face is always available,
and SELECT-at-start recovery bypasses external appearance.

AKMenu Next's configurable theme fonts informed this feature's design; Gloam's
pack format and renderer have their own bounded metrics contract. DejaVu/Bitstream
notices for supplied faces are preserved in [THIRD_PARTY.md](../../THIRD_PARTY.md)
and theme font directories.

Rebuild the shared fallback with `python3 tools/make_font.py --extended-only`.
It uses the credited DejaVu face, rejects clipped glyphs, and keeps ASCII/Latin-1
theme packs compatible. Unsupported codepoints retain the current face's `?` glyph.

## Proportional pack format

DSFONT03 requires `[theme] version=2`. All multi-byte fields are little-endian.

| Bytes | Meaning |
| --- | --- |
| 0–7 | ASCII `DSFONT03` |
| 8 | Maximum glyph width, 1–16 pixels |
| 9 | Line height, 12–16 pixels; body role requires 12 |
| 10–11 | First ASCII codepoint 32, count 95 |
| 12–13 | Either 0, 0 or Latin-1 first codepoint 160, count 96 |
| 14–15 | Zero reserved bytes |

Each ASCII glyph, followed by optional Latin-1 glyphs, contains a one-byte
advance, one zero reserved byte and `height` 32-bit rows. A row stores two-bit
coverage values from its least-significant bits. Bits outside the advance must
be zero. All glyphs except space, nonbreaking space and soft hyphen need visible
ink. Truncation, trailing data, invalid metrics and reserved fields reject the
face without replacing the currently loaded one. The maximum file is 12,622 bytes.

Wrapping uses pixel widths and word boundaries. Overflow clips within a pixel
viewport; selected text loops continuously with a three-space gap measured in
the same face. Reduced motion retains a static shortened label. Legacy drawing
call widths are seven-pixel layout units, not character counts; they preserve
existing screen geometry while allowing more narrow letters to fit.

PCF, CJK/RTL shaping and full UI localization are not supported by this format.

The Spanish catalog includes every settings label and help entry, plus common
settings values. Overlay headings and controls cover themes, associations, game options, cheats,
Home sections and SD removal. Untranslated help and runtime diagnostics retain English.
Catalogs support up to 384 additional keys within the 32 KiB file limit; values
share a 24 KiB text pool rather than allocating a full help buffer per key.
Personal theme names, paths and custom tab labels are never translated as values.
