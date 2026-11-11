#!/usr/bin/env python3
"""Build small original theme assets; never modify Copper or personal settings."""

import argparse
import io
import math
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw

if __package__:
    from .make_audio_samples import tones
    from .pack_theme_font import pack_font
else:
    from make_audio_samples import tones
    from pack_theme_font import pack_font

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
FONT_DIRECTORY = Path("/usr/share/fonts/truetype/dejavu")


def background(name: str, screen: int) -> bytes:
    """Quiet geometry across a shared 256x384 canvas; no text baked into art."""
    if name in ("eclipse", "prism"):
        # Eclipse exercises the decoder's maximum dimensions and center crop.
        side = 512 if name == "eclipse" else 256
        height = side if name == "eclipse" else 192
        image = Image.new("RGB", (side, height), "#100f1d" if name == "eclipse" else "#f2f4fb")
        draw = ImageDraw.Draw(image)
        if name == "eclipse":
            cx, cy = side // 2 + 75, side // 2 - 25 + screen * 45
            for radius, color in [(110, "#463356"), (92, "#8d618f"), (75, "#d9a4df")]:
                draw.ellipse((cx-radius, cy-radius, cx+radius, cy+radius), fill=color)
            draw.ellipse((cx-60, cy-85, cx+97, cy+72), fill="#100f1d")
        else:
            # Broad facets, not fine stripes, keep small text distinguishable.
            shift = screen * 35
            draw.polygon([(155-shift, 0), (256, 0), (256, 192), (95+shift, 192)], fill="#9cadde")
            draw.polygon([(225, 0), (256, 0), (256, 192), (175-shift, 192)], fill="#cfacd5")
            draw.line([(155-shift, 0), (95+shift, 192)], fill="#ffffff", width=3)
        output = io.BytesIO()
        image.save(output, format="PNG" if screen == 0 else "JPEG", **(
            {"optimize": True} if screen == 0 else {"quality": 90, "subsampling": 0, "optimize": True}
        ))
        return output.getvalue()
    image = Image.new("RGB", (256, 384), "#121729" if name == "nocturne" else "#dceee6")
    draw = ImageDraw.Draw(image)
    if name == "nocturne":
        draw.ellipse((162, 35, 215, 88), fill="#8d8aaa")
        draw.ellipse((174, 28, 222, 77), fill="#121729")
        draw.polygon(
            [(0, 183), (66, 112), (143, 206), (213, 145), (256, 198), (256, 384), (0, 384)],
            fill="#384058",
        )
        draw.polygon(
            [(0, 246), (97, 171), (179, 270), (256, 220), (256, 384), (0, 384)], fill="#545574"
        )
        for x, y in [(26, 41), (81, 79), (133, 32), (234, 112)]:
            draw.point((x, y), fill="#a6aac6")
    else:
        # Large coastal bands keep the text area calm on a small LCD.
        draw.polygon(
            [(155, 0), (256, 0), (256, 384), (43, 384), (114, 273), (92, 189), (168, 93)],
            fill="#75b7ac",
        )
        draw.line([(172, 0), (184, 93), (109, 189), (132, 275), (61, 384)], fill="#f7fff6", width=3)
        draw.polygon(
            [(228, 0), (256, 0), (256, 384), (119, 384), (176, 282), (155, 204), (222, 105)],
            fill="#388a83",
        )
    image = image.crop((0, screen * 192, 256, (screen + 1) * 192))
    output = io.BytesIO()
    if name == "tide" and screen == 1:
        image.save(output, format="JPEG", quality=90, subsampling=0, optimize=True)
    else:
        image.save(output, format="PNG", optimize=True)
    return output.getvalue()


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
    for name, pitch in [("folio", 0.65), ("nocturne", 0.8), ("tide", 1.2),
                        ("phosphor", 0.5), ("eclipse", 0.7), ("prism", 1.4)]:
        for filename, notes, duration, level in [
            ("cursor", [880 * pitch], 0.025, 0.075),
            ("confirm", [660 * pitch, 990 * pitch], 0.065, 0.10),
            ("back", [550 * pitch, 330 * pitch], 0.045, 0.075),
        ]:
            result[Path(name) / "sounds" / (filename + ".wav")] = tones(notes, duration, level)
        if name not in ("folio", "phosphor"):
            for screen in range(2):
                suffix = ".jpg" if name in ("tide", "eclipse", "prism") and screen == 1 else ".png"
                filename = ("top" if screen == 0 else "bottom") + suffix
                result[Path(name) / "backgrounds" / filename] = background(name, screen)
            result[Path(name) / "sounds/loop.wav"] = music(name)
    result[Path("nocturne/fonts/ui.dsfont")] = pack_font(
        FONT_DIRECTORY / "DejaVuSansMono.ttf", 10, 10, latin1=True
    )
    result[Path("tide/fonts/ui.dsfont")] = pack_font(
        FONT_DIRECTORY / "DejaVuSansMono-Bold.ttf", 11, 9
    )
    result[Path("eclipse/fonts/ui.dsfont")] = result[Path("nocturne/fonts/ui.dsfont")]
    result[Path("prism/fonts/ui.dsfont")] = result[Path("tide/fonts/ui.dsfont")]
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check assets without changing them")
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
