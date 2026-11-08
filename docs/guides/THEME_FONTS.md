# Theme fonts

Themes can replace the UI font using `[fonts] ui=fonts/ui.dsfont`. Gloam supports
fixed-cell ASCII and Latin-1 packs, described byte-for-byte in
[the theme package contract](THEME_PACKAGES.md#bitmap-fonts). The fixed 7×12 cell
keeps row, touch and footer geometry consistent across faces.

Create a pack with:

```sh
python3 tools/pack_theme_font.py source-font.ttf fonts/ui.dsfont --size 11 --baseline 9
```

The authoring tool checks advance/ink bounds and rejects overflow instead of
silently cropping. Use the pinned Pillow dependency for reproducible output.
Include the source face's redistribution license alongside the pack.

Test long filenames, accent characters, settings values and every browser view.
Text measurement and drawing share UTF-8 decoding and cell metrics; unsupported
scripts fall back to `?`. Missing or invalid external fonts reject the theme
candidate, keeping the current face. The built-in face is always available,
and SELECT-at-start recovery bypasses external appearance.

AKMenu Next's configurable theme fonts informed this feature's design; Gloam's
pack format and renderer have their own fixed-cell contract. DejaVu/Bitstream
notices for supplied faces are preserved in [THIRD_PARTY.md](../../THIRD_PARTY.md)
and theme font directories.

Variable-width/role-specific metrics, PCF import, wider scripts and full UI
localization remain future work. They require a shared measurement/layout
contract before replacing the current geometry guarantees.
