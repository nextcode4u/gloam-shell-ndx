# M+ Japanese bitmap fallback

Original bitmap glyphs: **COZ / M+ FONTS PROJECT**, copyright 2002–2004 in
the BDF source. The retained upstream notice credits M+ FONTS PROJECT 2002–2013.

Source mirror: [itouhiro/PixelMplus](https://github.com/itouhiro/PixelMplus),
revision `d89b95f04eeb95bf98bb09cb9234a2f99d186df5`.
`UPSTREAM.json` records the original paths and SHA-256 of each unchanged input.
`LICENSE_E` and `LICENSE_J` grant use, copying and distribution with or without
modification, commercially or noncommercially, without warranty.

`tools/make_japanese_font.py` converts JIS X 0208 to Unicode and JIS X 0201
halfwidth katakana to U+FF61–U+FF9F. It reverses MSB-left BDF rows into the
renderer’s bit order and prepares 6,817 shared glyphs. The 12-pixel baseline is
retained; for full-height rule/bracket glyphs with ink in a thirteenth row, that
row is folded into row twelve rather than discarded. Theme-provided ASCII,
Latin-1 and existing extended-script fallbacks keep priority.

The generated read-only table adds approximately 180 KiB to the executable;
it needs no extra per-theme glyph bank, font download or SD read during rendering.
This covers Japanese kana, JIS kanji and associated symbols, not all Unicode CJK
or Korean. Source font files and full notices accompany source/license packages.
