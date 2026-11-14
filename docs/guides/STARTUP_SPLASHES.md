# Startup splashes

Gloam plays prepared `.gsp` packages on one or both 256×192 screens. Packages
contain static artwork, simple layer effects or converted frame animations, with
optional sound. They are loaded once; playback does not decode PNG/GIF images or
read frames from SD. All package memory is released before normal browsing.

Splash playback is available in source builds after Gloam 1.0.0. The 1.0.0
release does not include these settings.

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

## Creating a splash

The graphical Splash Builder, converter, contributor instructions and Python
tests live in the [theme-directory repository](https://github.com/nextcode4u/gloam-shell-theme-directory).
Follow its [splash authoring guide](https://github.com/nextcode4u/gloam-shell-theme-directory/blob/main/docs/STARTUP_SPLASHES.md)
to import artwork/animation, preview it and export a package. No theme INI edits
are needed: place `splash.gsp` in the theme folder, or select a custom file in Gloam.

Gloam owns playback, settings and the format contract below. Its native checks
use independent synthetic fixtures in `tests/splash_fixture.py`; building or
testing the launcher does not require a theme-directory checkout.

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
