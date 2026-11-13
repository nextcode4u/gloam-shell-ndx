# Theme packages (version 1)

Open **Frontend Settings → Appearance → Theme**. Up/Down previews packages on
both screens. A applies; B/START cancels. Touch selects a preview, then applies
when the same row is selected again. Preview never writes preferences. Applying
records the package ID in normal frontend state; the original package is not
edited. Restart reloads that package. Missing/invalid packages use the built-in
appearance and report the fallback, while retaining the requested ID for repair.

The current package format supports palettes, fixed-cell fonts, per-screen backgrounds, icon packs, modular UI components and audio.
Named on-console appearance snapshots are documented in [APPEARANCE_PRESETS.md](APPEARANCE_PRESETS.md). Bounded layout defaults/spacing/grid overrides are implemented (see below);
variable font metrics and full UI localization remain further work. This version does not interpret AKMenu or
TWiLight theme files directly. Required navigation keeps its shared geometry.

Packages live at `_nds/dsi-frontend/themes/<id>/theme.ini`. IDs use ASCII letters,
numbers, hyphens and underscores and are at most 63 bytes. The chooser includes
Built-in and the alphabetically first 23 valid package directory names. It reads
one selected package at a time; startup does not scan all installed themes.

```ini
[theme]
version=1
name=Slate

[colors]
background=#101820
surface=#20303c
line=#405060
text=#ecf1f5
muted=#abc0cf
accent=#8dbce8
ink=#14202b

[fonts]
ui=builtin
```

`name` and color/font overrides are optional; omitted values use the built-in
base. `version=1` is required. Files are at most 4 KiB with lines shorter than
1 KiB. Unsupported fields/sections and invalid values reject the candidate.
Colors use exactly six hexadecimal RGB digits. Integer brightness-separation
checks reject unreadable text/background, text/surface, muted/background,
accent/background and ink/accent combinations. These checks do not certify a
font's legibility or substitute for native preview and physical LCD review.

The Accent control creates a personal preset-color override without changing the
package. Its 16 choices are Amber, Mint, Iris, Rose, Coral, Sky, Cyan, Lime,
Gold, Peach, Violet, Pink, Teal, Blue, Slate and Silver. Left/Right cycles in
both directions. Bright shades suit dark backgrounds; deeper versions with light
selection text suit light backgrounds. The original first three saved IDs stay
unchanged. Expanded selections persist in state and named appearance snapshots. The value shows Theme until an override is selected. Reapplying a theme
restores its accent. Other browser preferences remain independent of the package.
Copper and Slate demonstrate a packed face and the built-in face respectively;
Folio, Nocturne and Tide add light/dark layouts, wallpapers and original audio. All screens consume the resolved palette, including settings,
file pickers and theme preview. PNG covers retain their existing neutral alpha
matte; they are content artwork rather than theme backgrounds.

## Bundled sample themes

Choose these from START → Appearance → Theme. Preview with Up/Down, apply with
A, or cancel with B. The packages are ready to copy; no build step is needed.

| Theme | Illustrated identity | Browser character |
| --- | --- | --- |
| Copper | Riveted clockwork workshop, meshing brass gears. | Warm metal faces and amber selection edges. |
| Slate | Rain over a lit city archive. | Blue steel and restrained technical lines. |
| Folio | Botanical field notebook with a pressed flower. | Warm paper, graphite and folded corners. |
| Nocturne | Moonlit rooftops with a watchful cat. | Indigo surfaces and soft lunar corners. |
| Tide | A lighthouse above a quiet coast. | Sea-glass colors and pale chart-like panels. |
| Phosphor | Radar sweep and instrument grid. | Terminal green, bracket selections and compact rows. |
| Eclipse | Ringed planet above an observatory. | Dark plum and wide, spacious grid tiles. |
| Prism | Pastel arcade at closing time. | Bright cabinets, folded panels and dense grid tiles. |
| Ember | A tent and campfire beneath pine trees. | Canvas-brown rows and warm amber highlights. |
| Glacier | Polar research outpost beneath an aurora. | Ice-blue panels and larger grid tiles. |
| Garnet | Rose window and candlelit gothic hall. | Burgundy surfaces and a text-first browser. |

All eleven packs include original native-resolution illustrations, with four
screen compositions chosen to suit their character:

- **Scene — Copper, Nocturne, Tide and Garnet:** illustration above a compact
  inspector and horizontal tabs. Tide uses a larger thumbnail; Garnet a portrait
  framed thumbnail beside its rose window.
- **Poster — Eclipse, Prism and Ember:** large artwork beside title details,
  with horizontal tabs below.
- **Left sidebar — Folio and Glacier:** navigation on the left, artwork and
  information on the right.
- **Right sidebar — Slate and Phosphor:** artwork and information on the left,
  with a persistent tab list on the right.

Icons, cover art, labels and status remain live; none are baked into the
background. Ordinary dialogs keep a shaded reading surface. Personal view,
audio and Reduced motion preferences still apply.

The illustrations are CC0-1.0 and reproducible with
`.venv/bin/python tools/make_theme_scenes.py`. No other launcher's theme artwork
is redistributed. Existing music/font notices remain in each package.

[View all eleven rendered themes](../images/theme-personalities.png).
Regenerate with `.venv/bin/python tools/make_screenshots.py --theme-variants personality`.

![Phosphor, Eclipse and Prism rendered at native resolution](../images/theme-variants.png)

Regenerate this gallery with `.venv/bin/python tools/make_screenshots.py --theme-variants`
after building the host renderer. The images show actual renderer output with
the supplied example folders; physical LCD contrast still needs hardware review.

![Ember, Glacier and Garnet rendered at native resolution](../images/theme-atmosphere.png)

Use `.venv/bin/python tools/make_screenshots.py --theme-variants atmosphere` to
regenerate the second gallery. These three variants use native 256×192 backgrounds
and the same bounded audio format as the other samples; each music loop adds
250 KiB of sample memory only while enabled. Each package includes its own credits
and font license.

START → Audio → Background music must be Theme to hear a package's loop;
Theme sounds enables its cues. Applying a theme respects personal audio levels,
reduced motion and wallpaper overrides. Reset wallpapers to see package art if
personal wallpapers are assigned. Reapplying resets the accent override and
applies the package's browser defaults. Personal view/icon changes still work.
These examples use the existing selection easing, marquee and animated-banner
behavior; packages cannot supply custom animation scripts or widget positions.

Original geometry and synthesized audio are CC0; packed fonts retain the
included DejaVu licenses. Per-package CREDITS.md files record ownership. Rebuild
or verify only these sample assets with the pinned authoring environment:

```sh
.venv/bin/python tools/make_theme_samples.py
.venv/bin/python tools/make_theme_samples.py --check
```

The generator leaves Copper's licensed music and all personal settings alone.
`tools/host-check.sh` loads the actual packages through the native-compatible
loader and checks preview/cancel/apply/restart, both background decoders, WAV
loading and browser/settings bounds. Native LCD readability remains a hardware
check. These are reference packages, not a claim that unsupported theme schemas
or arbitrary UI layouts are implemented.

## Bitmap fonts

`[fonts] ui=fonts/ui.dsfont` loads a relative file contained within the package.
Absolute paths, devices and parent traversal are rejected. A bad/missing face
rejects the candidate; the current active face remains available. The built-in
font is permanently compiled into the ROM.

The pack format is deliberately a restricted first version:

| Bytes | Meaning |
| --- | --- |
| 0–7 | ASCII `DSFONT01` |
| 8–11 | Cell width 7, height 12, first codepoint 32, glyph count 95 |
| 12–15 | Zero reserved bytes |
| 16 onward | 95 glyphs × 12 little-endian uint16 rows |

Each row contains seven two-bit coverage values, low bits first. The top two
bits must be zero. Every printable glyph except space must have visible ink.
The complete file is exactly 2,296 bytes; truncation and trailing data are errors.
Measurement and drawing use the same fixed seven-pixel advance and twelve-pixel
line height. Font replacement therefore cannot change row, column, touch or
footer geometry. UTF-8 drawing and Latin-1 fallback/format-v2 packs are supported; see TEXT_AND_TITLES.md. Other scripts still use `?`. This is not full
UI localization or PCF compatibility.

Theme authors can run:

```sh
python3 tools/pack_theme_font.py source-font.ttf fonts/ui.dsfont --size 11 --baseline 9
```

Pillow/FreeType is an authoring dependency only. The tool checks ink bounds and
advance before rasterization and rejects overflow rather than cropping or
stretching. Ordinary users copy an already prepared package; no compiler is
needed on the console. Font redistribution licenses must accompany the package;
Copper includes the DejaVu license. Larger and variable-width font work is
specified separately in [THEME_FONTS.md](THEME_FONTS.md).

## Recovery

Hold SELECT while the frontend starts to bypass the external theme before its
files are read. Recovery uses built-in fonts/colors, Small/Classic rows, reduced
motion, covers off and Home. Its header says RECOVERY.

Appearance edits in this session are temporary: navigation/favorite saves retain
the normal accent, motion, icon size/view, cover visibility, clock format, theme
ID and accent-override preference. Restart normally to use the saved appearance.
Theme preview remains available by explicit selection, but applying packages is
blocked in recovery. Configuration export/import and reset scopes are described in
[CONFIGURATION_BACKUPS.md](CONFIGURATION_BACKUPS.md); this is not a general SD recovery utility.


## Copper background music

Copper uses “Sentinel” by pixelpion from Pixabay. Its `sounds/loop.wav` contains
the full approximately 43-second track, converted to 12 kHz mono PCM16. The
1,035,648-byte sample buffer fits the existing 1 MiB music cap. Source/license
links, conversion settings and hashes are recorded in Copper's `CREDITS.md`.
The UI cue generator changes only cursor/confirm/back sounds; it leaves this
licensed track intact. Audio validation fixtures use a separate original tone
sequence so their format checks do not depend on the theme's chosen music.

## Backgrounds and on-console wallpapers

Optional package fields:

```ini
[backgrounds]
top=backgrounds/top.png
bottom=backgrounds/bottom.jpg
strength=25
```

Paths stay within the theme package. PNG/JPEG inputs are bounded to 512×512 and
1 MiB compressed data each; images are center-cropped to 256×192 and blended
with the package background color. Strength is 0–100 percent (default 25). Full strength preserves the artwork;
authors must place text over calm areas or set `layout/content_opacity` to blend
a reading surface over it. Unsupported/missing optional images fall back
individually to the palette. No image decoding or filesystem access occurs in
rendering. Active and preview pixel buffers are allocated only for supplied
images, transferred on Apply and freed on Cancel/replacement.

Appearance → Top wallpaper/Bottom wallpaper chooses personal images anywhere on
SD without editing the theme. Wallpaper strength uses Theme/Off/5–35 percent;
Reset wallpapers restores the package defaults without deleting any image.
Personal wallpaper references persist across theme changes. Recovery bypasses
all external wallpaper reads and blocks changes to normal wallpaper settings.
Refresh metadata also reloads prepared backgrounds.

The drawing/font path now supports fixed-cell Latin-1 packs and UTF-8-aware
measurement. See [TEXT_AND_TITLES.md](TEXT_AND_TITLES.md); wider scripts and
variable glyph metrics still require more work.

## Bounded layout settings

```ini
[layout]
view=grid
icon_size=large
row_padding=6
large_grid_columns=3
top_layout=poster
chrome=minimal
content_opacity=40
```

All keys are optional. `view` accepts classic/extended/list/grid/shelf/index and `icon_size`
accepts small/large. These supply defaults when Apply is pressed; ordinary
Appearance choices can override them afterward and remain saved across restart.
Preview/cancel does not change browser preferences. Named presets retain their
explicit icon/view choices independently of these apply-time defaults.

Framed browser rows reserve at least four pixels above and below their text or
icon content. Their selection frame stays anchored to the selected row, avoiding
a moving border crossing neighbouring labels. Built-in flat highlights retain
their slide animation.

`row_padding` adds 0–12 pixels to the shared row/cell height; both drawing and
touch/navigation use the resolved size. Large Grid accepts 2–4 columns and
allocates its label width from the actual cell width. Small Grid keeps two
columns to protect label readability. Invalid layout values reject the package
without changing the active theme. Settings, required navigation and font-cell bounds retain their native geometry. Arbitrary widget positions and
unbounded custom layout scripts are not supported. Optional component artwork
changes the appearance of these regions while retaining their input geometry.

### Screen composition

These optional composition/color keys require the matching current `BOOT.NDS`.
Older builds reject unknown manifest keys; update the binary along with these
example theme packs. Packages without the new keys keep their previous behavior.

`top_layout` accepts `sidebar` (the original right rail), `sidebar_left`,
`poster`, or `scene`. The left rail moves the artwork and information together without
mirroring text or wallpaper. Poster uses a larger artwork area and a horizontal
strip showing the focused tab and its neighbours. L/R still cycles all tabs;
X enters tab selection, where Up/Down and A retain their usual behavior. Long
labels continue to loop. Item information and Library Index temporarily use the
full six-row right rail instead of squeezing their contents into the poster.

Scene reserves y=24..117 for an illustration, with item information below it and
contextual navigation. Y opens the regular item inspector; Library Index keeps
its index workspace. It uses the package's top background, including personal
wallpaper strength, but preserves an unshaded copy of just that illustration
strip. This costs **47 KiB per active/preview scene** in addition to normal
background buffers. Loading prepares both surfaces; drawing only copies pixels.
Missing images or allocation failure fall back to the normal prepared background.

Authors can place the cover around their subject:

```ini
[layout]
top_layout=scene
scene_cover_x=14
scene_cover_y=34
scene_cover_width=48
scene_cover_height=72
content_opacity=82
```

The cover fits this box without changing aspect ratio. Width is 8–96 pixels,
height 8–90; the complete box must stay within x=2..253 and y=26..115, reserving
two pixels for its frame. Invalid boxes reject the package. Show covers still
controls the live thumbnail; backgrounds remain independent. Coordinates do
not relocate touch targets or the item inspector.

`chrome=full` retains header/footer images and browser separators.
`chrome=minimal` omits those component images and the browser heading/divider
lines, leaving required headings, settings access and button hints visible.
Dialogs retain their own separators. Selected rows/cards remain skinnable.

`content_opacity=0..100` blends the palette background over the inner reading
surface (x=6..249, y=4..187) of both wallpapers; 0 preserves the image, 100 makes
that area opaque. The outer edge keeps its original strength. This is prepared
when loading the theme, adds no image allocation and does no blending per frame.
The default is 0, preserving existing packages. Personal wallpaper strength
remains a separate user override.

Optional `[colors] selection_text` sets focused browser-row, quick/action-menu,
settings-label and picker-label text. `[colors] tab_text` sets active navigation
text. Omitted values inherit `text` and `ink`, respectively. Secondary metadata,
settings values and other dialog text retain their established palette roles;
check these against the selected component artwork too. PNG colors are not
recolored by palette or accent changes.

The bundled packs demonstrate Scene, both sidebar directions and Poster. These compositions
work with all browser views and do not change bottom-screen touch targets.

### Tab presentation

Tab presentation is independent of `top_layout` and the bottom browser view.
These keys require the matching current `BOOT.NDS`; older builds reject unknown
keys. Omitted keys preserve label tabs with a skinned selection, three horizontal
cells and a four-pixel gap.

```ini
[layout]
tab_style=icons
tab_highlight=outline
tab_visible=5
tab_gap=8
```

| Key | Values | Effect |
| --- | --- | --- |
| `tab_style` | `labels`, `icons`, `numbered`, `pager` | Changes navigation presentation, not the user's configured tabs. |
| `tab_highlight` | `skin`, `underline`, `outline` | Uses `tab-selected.png` (accent fill fallback), a two-pixel underline, or a one-pixel outline. |
| `tab_visible` | 3–5 | Number of horizontal label/icon/numbered cells. Sidebars show up to six; Pager shows one. |
| `tab_gap` | 0–8 | Horizontal space between cells, in pixels. Does not affect sidebar geometry or Pager. |

- **Labels** show a scrolling window of names.
- **Icons** use a horizontal dock for Scene/Poster, or a two-column tile selector
  in a sidebar. The focused tab's full name has its own scrolling caption.
  Scene omits its secondary file-type/size line to make room for that caption;
  Y still opens complete item information.
- **Numbered** prefix names with their one-based position. Three horizontal
  cells give names more space than five; selected long names scroll.
- **Pager** shows one focused category with its position and navigation hints.
  In a sidebar it includes a larger category icon; horizontally it is a single
  wide selector between arrows.

Dock/tile/pager icons use the theme's existing shortcut, game, application,
audio, system and folder PNGs for Home, Games, Apps, Media, System and Files.
Custom folder tabs use the folder icon. This does not replace native game banners.
Selection artwork applies only to `skin`; outline/underline keep ordinary text
colors for contrast. Existing `tab_text` controls text on skinned selections.

All styles keep L/R cycling and X → Up/Down → A selection. Windows follow the
focused tab, including custom tabs beyond the visible count. Reduced motion
continues to stop marquees. These controls do not move touch targets: navigation
is on the non-touch top screen. Item information and Library Index retain their
sidebar workspace and use the same tab presentation there.

## Theme icon packs

Optional package-local icons replace the generic symbols used in browser rows,
Home shortcuts, pickers and cover placeholders. Add this to `theme.ini`:

```ini
[icons]
directory=icons
```

Place any of these **32×32 PNGs**, at most **64 KiB each**, in that directory:

- File families: `file.png`, `folder.png`, `game.png`, `application.png`,
  `audio.png`, `video.png`, `book.png`, `text.png`, `image.png`, `save.png`,
  `config.png`, `archive.png`, `shortcut.png`, `system.png`, `missing.png`.
- System fallbacks: `settings.png`, `camera.png`, `sound.png`, `pictochat.png`,
  `download-play.png`, `system-menu.png`, `slot1.png`.

Each image is optional. Missing, malformed or oversized images use the built-in
symbol; they do not invalidate other images or the theme. The directory must
stay inside the theme package (no absolute paths or `..`). PNG colours are kept
as RGB555 with binary transparency (alpha below 128 is transparent), and scaled
without smoothing. Accent overrides do not recolour PNGs. List view stays text-only.

Assigned item icons, native title/cartridge banners and association icons take
priority over theme fallbacks. System images replace fallback symbols only;
readable native system-app banners keep their animations. Cover images remain
independent. Theme icons are static.

Decoding happens on theme load/preview, never in the renderer. Each decoded
image uses 2 KiB; all 22 use 44 KiB, or 88 KiB while previewing a second full
pack. Cancel frees the preview and retains the applied icons. Applying a theme
replaces its icon bank; no per-frame filesystem reads are added.

All eleven bundled themes include matching icon sets. Reproduce or check them:

```sh
.venv/bin/python tools/make_theme_icons.py
.venv/bin/python tools/make_theme_icons.py --check
```

The original icon artwork uses terminal outlines, rounded lunar shapes, folded
paper and beveled retro forms. Icons are complete designs, not stock masks
laid over decorative badges.
The icons are CC0; each package includes a credit notice.

**Compatibility:** these manifests require a build with `[icons]` support. Older
builds reject unknown theme sections. Update `BOOT.NDS` with these theme packages;
existing third-party themes without `[icons]` continue to use built-in symbols.

## Header and console status

```ini
[theme]
version=1
name=My theme
header=My library
```

`header` replaces the top-screen Gloam label. Omit it for `GLOAM SHELL NDX`, or
use an empty value to hide that text. The limit is 63 UTF-8 bytes; long labels
loop within their reserved width, while Reduced motion keeps them static.
Recovery always displays `RECOVERY`. Battery, wireless and tab position retain
reserved space on the right, independently of the header text.

Battery sprites reflect the raw hardware levels, grouped into empty/low/half/high/
full, with separate charging and unknown states. They are not a percentage or a
runtime estimate. Gloam currently does not establish a Wi-Fi connection:
`wireless-idle.png` means **not connected by Gloam**, not radio power-off or a
hardware switch reading. It never claims signal strength from an LED or saved
credentials. Missing status sprites use built-in readable symbols. Status art is
shown on the main browser header; dialog titles retain their existing space.

## Modular UI artwork

Themes can replace individual UI surfaces without changing touch targets or
copying a whole screen image. The text, icons, scrolling and input behavior
remain live. Each component is independent: mix your own header with the
built-in selection style, or copy only a card treatment from another Gloam pack.

```ini
[components]
directory=components
border=8
```

Inside that package-local directory, supply any of these **32×32 PNGs**:

| Filename | Used for |
| --- | --- |
| `header.png` | Browser and dialog headings on both screens. |
| `footer.png` | Bottom control/status bands on both screens. |
| `panel.png` | Quick-controls and item-action overlay surfaces. |
| `row.png` | Ordinary browser rows; drawn beneath the moving selection. |
| `icon-frame.png` | Transparent bezel around native/associated browser icons. |
| `artwork-frame.png` | Transparent frame around resolved top-screen artwork. |
| `battery-empty.png`, `battery-low.png`, `battery-half.png`, `battery-high.png`, `battery-full.png` | Battery level sprites. |
| `battery-charging.png`, `battery-unknown.png` | Charging or unavailable battery reading. |
| `wireless-idle.png` | No active connection established by Gloam. |
| `row-selected.png` | Selected browser rows, settings, pickers and editors. |
| `tab-selected.png` | The active top-screen tab and the theme preview's selection sample. |
| `card.png` | Unselected Grid/Shelf cells and the top-screen cover placeholder. |
| `card-selected.png` | Selected Grid/Shelf cell. |

Status sprites scale as a whole from 32×32 to 16×16; keep their design bold enough
to read at that size. Other images use **nine-slice scaling**: four corners stay at their authored size;
the four edges stretch in one direction and the center stretches in both.
`border` specifies the source corner width (0–12 pixels, default 8). Zero stretches
the whole image. When a destination is too small for both corners, the corners
shrink to fit. This keeps an outline or folded corner intact on a short row and
a tall cover card. All output clips to the screen, including moving Shelf cards.
Avoid words and icons inside these images: Gloam draws the live content over them.

Each PNG is at most 64 KiB. Binary alpha and RGB555 follow the icon contract.
Missing or invalid images fall back individually to the built-in drawing for
that region. An unselected Grid cell without a component stays transparent.
Paths cannot leave the theme package. A malformed manifest, including a border
outside 0–12, rejects the preview; Apply/Cancel never mixes asset banks.

Use palette `surface` behind normal text and `accent` behind tab text (`ink`).
PNG colors are fixed and personal accent changes do not recolor them. The loader's
palette checks cannot certify contrast inside arbitrary artwork; inspect both
screens, small/large icons, every view and settings before sharing a pack.

All eleven supplied packs include components. Phosphor uses open selection
brackets, Eclipse uses rounded panels, Prism uses folded corners, and Copper
uses beveled edges. Regenerate them with:

```sh
.venv/bin/python tools/make_theme_components.py
.venv/bin/python tools/make_theme_components.py --check
```

Components decode only when loading or previewing a theme. Eighteen complete images
use 36 KiB, or 72 KiB with a second preview bank. No scripts, filesystem reads or
image decoding run in drawing. Recovery bypasses external components. The desktop
setup tool previews and validates them along with icons. Older builds reject the
new section: update `BOOT.NDS` together with these theme packs.

### Cover shelf

Choose **Shelf** in browser appearance settings, or set `view=shelf` in a theme.
The selected cover stays centered between its neighbors on a continuous shelf.
Left/right moves one item, up/down skips three, and L/R switches tabs. The full
selected name scrolls below the shelf; details remain on the top screen. Tap a
neighbor to select it, then tap the selected item to open it. Home and custom
folder tabs use the same layout. Reduced motion disables the horizontal slide.

Jackets retain their size while artwork resolves. Missing covers and native
banners follow the icon-size setting: Small uses 32 pixels and Large uses 48,
including animated banners. Cards stay the same size, and covers retain their
aspect ratio inside the jackets. Row-padding and grid-column settings apply
to their other layouts.

Three path-keyed prepared images are retained across selection changes and
invalidated on explicit refresh. One entry is prepared per tick using the
existing bounded decoder/cache. Shelf adds approximately 62 KiB of prepared
image and path storage; rendering never reads files or decodes artwork.

### Library Index

Choose **Index** under START → Appearance → Browser view, or use `view=index`
in a theme. The bottom screen shows a title list with small (16-pixel) or large (32-pixel)
icons and matching row spacing; the top
screen shows folders and alphabetical groups. Left/Right jumps to the first
item in the next populated group, wrapping past empty groups. Up/Down browses
normally; L/R changes tabs and Y switches to item details.

Groups count the current directory batch, not the entire SD card. Existing
sorting stays intact. Common Latin-1 accented initials join their base letter;
other scripts and punctuation appear under `*` (Other). Genre metadata is not
required. Index honors the icon-size setting and theme row padding.
