# Slate background music

**“Night on the Docks - Sax” — Kevin MacLeod (incompetech.com).**

Source: https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100137

License: [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/). The full text is in `MUSIC-LICENSE.txt`. This music is separate from the theme artwork, fonts and code licenses. No endorsement is implied.

Adaptation: 30.000-second excerpt beginning at 12.000 seconds; mono downmix, 16 kHz PCM16, high/low-pass filtering and loudness normalization, a 1.500-second circular crossfade, and a quiet loop-boundary rotation. No recurring fade to silence. Source and output SHA-256 pins are in `config/theme-music.json`.

Reproduce using `tools/prepare_theme_music.py --rebuild --download`. The original MP3 stays in ignored build storage; the theme contains the prepared WAV.
