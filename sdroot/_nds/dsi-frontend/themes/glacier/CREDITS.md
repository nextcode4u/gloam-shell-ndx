# Glacier

Visual direction: Layered paper mountain landscape.

Illustrations, material surfaces and individually illustrated icons were created for
Gloam Shell NDX with agent assistance. The project offers its rights in these
assets under CC0-1.0:
https://creativecommons.org/publicdomain/zero/1.0/

Background masters and their attribution are in `assets/theme-art/` in the source
package. Prepare backgrounds with `tools/make_theme_scenes.py`; Prism's vector
composition is authored directly in that tool. No other launcher's artwork is
included. Prompts and draft images are not distributed.

The body face is PublicSans; headings use ArchivoBlack-Regular. Both use the SIL Open Font License, retained in `fonts/LICENSE.txt`. Source revisions and checksums are recorded in `config/theme-fonts.json`.
`tools/make_theme_samples.py` reproduces those packs and the original synthesized
UI cues. `tools/make_theme_icons.py` and `tools/make_theme_components.py` reproduce
the sprites; `tools/pack_theme_assets.py <theme-folder>` rebuilds `assets.gfx`.
All smoothing, texture preparation and resampling happen on the authoring PC.

Original synthesized UI cues retain the same CC0-1.0 dedication. Background music has its own terms below.

## Background music

“Frozen Star” by Kevin MacLeod (incompetech.com), CC-BY-4.0. See [MUSIC.md](MUSIC.md) for source, adaptations and attribution, and [MUSIC-LICENSE.txt](MUSIC-LICENSE.txt) for the full terms.

## Startup splash

The 1.2-second `splash.gsp` adapts this pack’s existing background artwork and
heading/body typefaces. Artwork and font credits and terms above also apply to
the splash. It adds no third-party music.
Prepared by `tools/build_theme_splashes.py` in the
[Gloam theme directory](https://github.com/nextcode4u/gloam-shell-theme-directory).
