# Theme fonts

Themes can replace the UI font using `[fonts] ui=fonts/ui.dsfont`. Gloam supports
fixed-cell ASCII and Latin-1 packs, described byte-for-byte in
[the theme package contract](THEME_PACKAGES.md#bitmap-fonts). The fixed 7×12 cell
keeps row, touch and footer geometry consistent across faces.

Create a pack with:

```sh
python3 tools/pack_theme_font.py source-font.ttf fonts/ui.dsfont --size 11 --baseline 9
```

The authoring tool measures the actual monochrome ink, including hinted
bearings, before placing it in the cell. It shifts ink horizontally only when
needed to preserve the full glyph; advance and vertical baseline stay fixed.
Glyphs that cannot fit are rejected rather than cropped or stretched. Use the pinned Pillow dependency for reproducible output.
Include the source face's redistribution license alongside the pack.

Test long filenames, accent characters, settings values and every browser view.
Text measurement and drawing share UTF-8 decoding and cell metrics; unsupported
scripts fall back to `?`. A shared built-in face supplies Latin Extended-A,
Greek and Cyrillic characters beyond a theme pack's Latin-1 range. These
characters keep the same 7×12 cells and do not enlarge each loaded theme font. Missing or invalid external fonts reject the theme
candidate, keeping the current face. The built-in face is always available,
and SELECT-at-start recovery bypasses external appearance.

AKMenu Next's configurable theme fonts informed this feature's design; Gloam's
pack format and renderer have their own fixed-cell contract. DejaVu/Bitstream
notices for supplied faces are preserved in [THIRD_PARTY.md](../../THIRD_PARTY.md)
and theme font directories.

Rebuild the shared fallback with `python3 tools/make_font.py --extended-only`.
It uses the credited DejaVu face, rejects clipped glyphs, and keeps ASCII/Latin-1
theme packs compatible. Unsupported codepoints retain the current face's `?` glyph.

Variable-width/role-specific metrics, PCF import, CJK/RTL scripts and full UI
localization remain future work. They require a shared measurement/layout
contract before replacing the current geometry guarantees.

The Spanish catalog includes every settings label and help entry, plus common
settings values. Other dialogs and runtime errors still fall back to English.
Catalogs support up to 256 additional keys within the 32 KiB file limit; values
share a bounded text pool rather than allocating a full help buffer per key.
Personal theme names, paths and custom tab labels are never translated as values.
