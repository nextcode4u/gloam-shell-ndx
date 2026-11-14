# Startup splashes

Gloam plays prepared `.gsp` packages on one or both 256×192 screens. Packages
contain static artwork, simple layer effects or converted frame animations, with
optional sound. They are loaded once; playback does not decode PNG/GIF images or
read frames from SD. All package memory is released before normal browsing.

## On the console

START → Appearance includes these controls in Simple and Advanced settings:

- **Startup splash:** Off, Theme or Custom. Theme looks for `splash.gsp` inside
  the selected theme's folder. Custom tries the chosen SD file first.
- **Choose splash:** browse SD for a `.gsp`, see a two-screen still preview, and
  select it. This enables Custom without changing the theme.
- **Splash playback:** While loading ends as soon as initialization and the first
  browser scan finish. Play once allows the package's duration to finish. If loading
  takes longer, the last animation frame remains visible until browsing is ready.
- **Splash sound:** optional, off by default; uses UI sound volume. Theme music
  starts after the splash stops. Reduced motion uses the designated still frame
  and suppresses splash sound.
- **Preview splash:** plays the selection without restarting. Any button returns
  to settings; the preview does not save or change preferences.

Any button or touch skips startup playback and cancels a pending auto-launch.
Hold SELECT for recovery; it bypasses splash assets. SELECT held during startup
also requests recovery. Splash loading begins after SD mounting and preferences;
it cannot cover time spent in the bootloader or SD mounting itself. Initialization
has presentation checkpoints between stages and theme assets, so a slow SD read
can still interrupt animation briefly. No network initialization is required.

Missing or invalid Custom packages fall back to the theme, then
`/_nds/dsi-frontend/splashes/default.gsp`, when installed. Theme also uses that
default when it has no splash. If none is valid, startup continues normally.
Off and recovery do not load packages. Defaults are Theme, While loading and
sound off, including existing installations without splash preferences.

The settings records are `splash_mode=0|1|2`, `splash_once=0|1`,
`splash_sound=0|1` and `splash_path=sd:/...`. Configuration export/import includes
these records through the existing state file. User artwork stays in place.

## Contributor workflow

Install the Python dependencies from `requirements-dev.txt`, then run:

```sh
python tools/pack_splash.py
```

The graphical Splash Builder uses Python's Tk interface (included with the
standard Windows Python installer; some Linux distributions package Tk separately).
It is a small splash tool, independent of a full theme editor.

1. Choose PNG/JPEG artwork, an animated GIF, or a folder of numbered PNG frames
   for each screen. A single screen is enough; unused screens stay black.
2. Optionally add a transparent overlay/logo for either screen. Overlays are
   centered; their width can be adjusted. Without an overlay, the chosen effect
   applies to the screen image instead.
3. Choose fit or crop, frame rate and effect: none, fade, slide up, slide left or
   reveal. Effects last 0.5 seconds by default. Fit preserves aspect ratio and
   uses the selected letterbox colour; crop explicitly fills the screen.
4. Set duration and a still-frame time, or leave them automatic. Animated inputs
   retain their timing through fixed-rate sampling; PNG folders use the selected
   frame rate. Short sequences hold their last frame. Frames beyond an explicitly
   shorter package duration are not shown.
5. Optionally choose a WAV. The converter accepts mono/stereo 8/16-bit PCM at
   8–48 kHz and converts it to mono PCM16 at 16 kHz. Sound is limited to the package
   duration. The PC visual preview is silent; check audio in Gloam.
6. Build and preview. Check Reduced motion, Skip, and the fast-startup simulation.
   The opening image should already make sense: While loading may end mid-animation.
7. Save a `.gsp`, **Add to theme** (choose the folder containing `theme.ini`), or
   **Install as custom splash** (choose the SD root/staging folder).

Add to theme writes only `splash.gsp`; it needs no INI changes. Include the source
artwork's credits/license with the theme. Custom installation keeps the package
under `splashes/`, backs up an existing valid state file, and preserves unrelated
settings and repeated navigation/favorite records. It does not change ROMs or saves.

For repeatable command-line builds:

```sh
python tools/pack_splash.py splash.gsp --top sky.png --top-logo logo.png --effect fade
python tools/pack_splash.py splash.gsp --top animation.gif --bottom still.png --fps 15
python tools/pack_splash.py splash.gsp --top frames/ --fps 20 --sound intro.wav
```

An existing output requires `--replace`. `--duration` and `--poster` use seconds;
`--transition` controls effect duration. The default is shared indexed colour;
`--direct` retains RGB555 colours with greater memory/SD cost. Source images and
frame sequences remain editable; the `.gsp` is the runtime asset.

## Limits and format

Version 1 permits at most 2 MiB for the complete package, four layers, 120 frames
per layer and ten seconds of playback. A static 256×192 indexed screen costs about
48 KiB plus its 512-byte palette; a small animated overlay is cheaper than a
full-screen sequence. Two full-screen animations reach the size limit sooner.
The builder reports package size and rejects excesses; it does not silently reduce
frame rate, discard screens or change colour mode. Supported rates divide 60:
5, 6, 10, 12, 15 and 20 fps. GIF inputs are resampled by the builder. Prefer 15–20 fps for animation. Static
frames are drawn once; the player redraws only when a frame or effect changes.

All integers below are little-endian. Header (32 bytes): magic `GLOMSP01` (8),
total bytes (u32), IEEE CRC32 of bytes 16 onward (u32), duration and poster time
(u16 each, 60 Hz ticks), layer count (u8), three zero bytes, audio offset and size
(u32 each; both zero for silence).

Each layer has a 32-byte record: screen, pixel format, effect and reserved zero
(u8 each); width, height, frame count and ticks per frame (u16 each); x/y (i16);
delay and effect duration (u16); payload offset, payload size and reserved zero
(u32). Screen is 0/1; pixel format is RGB555 with bit-15 opacity (0), or indexed
8-bit (1). Indexed payloads begin with 256 RGB555 palette words. Effects are
none/fade/slide-up/slide-left/reveal (0–4). Palette opacity is binary; fades blend
prepared colours over the layer below. Layers compose in record order.

Payloads follow the records in order, each padded to four-byte alignment. Frames
are row-major and share their layer's dimensions and palette. Optional audio is
last, four-byte aligned mono PCM16 at 16 kHz, at most 320,000 bytes. Offsets,
dimensions, reserved fields, sizes, duration and CRC are validated before rendering.
Unknown or malformed formats are rejected without changing preferences.
