# Default startup artwork

The two `Gloam_Shell_NDX_64frame_20fps_*.gif` files are the production artwork
supplied by nextcode4u for Gloam's default splash. Each is 256×192 with 64 frames,
50 ms per frame. They are retained unchanged as the editable animation sources.

The prepared runtime asset is
`sdroot/_nds/dsi-frontend/themes/default/splash.gsp`. It uses one shared indexed
palette per screen and independently compressed frames, preserving the source
resolution, frame count and timing. The runtime folder deliberately has no
`theme.ini`: it supplies a fallback splash, not a selectable theme.

Rebuild with the Splash Builder converter from the
[theme directory](https://github.com/nextcode4u/gloam-shell-theme-directory),
using its pinned Pillow version. From that repository, replace `GLOAM` below
with the Gloam checkout path:

```sh
python tools/pack_splash.py "GLOAM/sdroot/_nds/dsi-frontend/themes/default/splash.gsp" \
  --top "GLOAM/assets/Gloam_Shell_NDX_64frame_20fps_top.gif" \
  --bottom "GLOAM/assets/Gloam_Shell_NDX_64frame_20fps_Bottom.gif" \
  --fps 20 --duration 3.2 --effect none --fit native --compress --replace
```

These project assets follow the repository's [GPL-3.0-or-later license](../LICENSE).
No source GIFs are placed on the SD card; they accompany the source package.
