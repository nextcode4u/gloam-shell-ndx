#!/usr/bin/env python3
"""Build original example-theme assets without modifying personal settings."""

import argparse
import io
import math
import struct
import wave
from pathlib import Path

if __package__:
    from .make_theme_scenes import assets as scene_assets
    from .make_audio_samples import tones
    from .pack_theme_font import pack_font
else:
    from make_theme_scenes import assets as scene_assets
    from make_audio_samples import tones
    from pack_theme_font import pack_font

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
FONT_DIRECTORY = Path("/usr/share/fonts/truetype/dejavu")


def music(name: str) -> bytes:
    """An original eight-second loop with zero-ended notes and restrained levels."""
    rate = 16000
    notes = (
        [220, 261.6256, 329.6276, 293.6648, 220, 261.6256, 391.9954, 329.6276]
        if name == "nocturne"
        else [261.6256, 329.6276, 391.9954, 329.6276, 293.6648, 349.2282, 440, 391.9954]
    )
    if name == "eclipse":
        notes = [164.8138, 220, 246.9417, 220, 196, 164.8138, 146.8324, 164.8138]
    elif name == "prism":
        notes = [293.6648, 369.9944, 440, 554.3653, 493.8833, 440, 369.9944, 329.6276]
    elif name == "ember":
        notes = [196, 246.9417, 293.6648, 246.9417, 220, 196, 164.8138, 196]
    elif name == "glacier":
        notes = [
            329.6276,
            493.8833,
            440,
            369.9944,
            329.6276,
            293.6648,
            246.9417,
            293.6648,
        ]
    elif name == "garnet":
        notes = [146.8324, 174.6141, 220, 196, 174.6141, 164.8138, 130.8128, 146.8324]
    frames = bytearray()
    for frequency in notes:
        for index in range(rate):
            time = index / rate
            fade = min(1.0, time / 0.06, (rate - 1 - index) / (rate * 0.18))
            envelope = fade * (0.65 + 0.35 * math.exp(-3 * time))
            sample = (
                0.14 * math.sin(2 * math.pi * frequency * time)
                + 0.055 * math.sin(math.pi * frequency * time)
                + 0.018 * math.sin(4 * math.pi * frequency * time)
            )
            frames.extend(struct.pack("<h", round(sample * envelope * 32767)))
    output = io.BytesIO()
    with wave.open(output, "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(rate)
        stream.writeframes(frames)
    return output.getvalue()


def assets() -> dict[Path, bytes]:
    result = {}
    for name, pitch in [
        ("folio", 0.65),
        ("nocturne", 0.8),
        ("tide", 1.2),
        ("phosphor", 0.5),
        ("eclipse", 0.7),
        ("prism", 1.4),
        ("ember", 0.6),
        ("glacier", 1.05),
        ("garnet", 0.45),
    ]:
        for filename, notes, duration, level in [
            ("cursor", [880 * pitch], 0.025, 0.075),
            ("confirm", [660 * pitch, 990 * pitch], 0.065, 0.10),
            ("back", [550 * pitch, 330 * pitch], 0.045, 0.075),
        ]:
            result[Path(name) / "sounds" / (filename + ".wav")] = tones(
                notes, duration, level
            )
        if name not in ("folio", "phosphor"):
            result[Path(name) / "sounds/loop.wav"] = music(name)
    result[Path("nocturne/fonts/ui.dsfont")] = pack_font(
        FONT_DIRECTORY / "DejaVuSansMono.ttf", 10, 10, latin1=True
    )
    result[Path("tide/fonts/ui.dsfont")] = pack_font(
        FONT_DIRECTORY / "DejaVuSansMono-Bold.ttf", 11, 9
    )
    result[Path("eclipse/fonts/ui.dsfont")] = result[Path("nocturne/fonts/ui.dsfont")]
    result[Path("prism/fonts/ui.dsfont")] = result[Path("tide/fonts/ui.dsfont")]
    for name in ("ember", "glacier", "garnet"):
        result[Path(name) / "fonts/ui.dsfont"] = result[
            Path("nocturne/fonts/ui.dsfont")
        ]
    result.update({path.relative_to(THEMES): data for path, data in scene_assets()})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Check assets without changing them"
    )
    args = parser.parse_args()
    generated = assets()
    for relative, data in generated.items():
        destination = THEMES / relative
        if args.check:
            if not destination.is_file() or destination.read_bytes() != data:
                raise SystemExit(f"Theme asset differs: {relative}")
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
    print(
        f"{'Checked' if args.check else 'Wrote'} {len(generated)} assets, "
        f"{sum(map(len, generated.values())):,} bytes"
    )


if __name__ == "__main__":
    main()
