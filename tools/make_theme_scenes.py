#!/usr/bin/env python3
"""Prepare native backgrounds from original illustration masters and vector art.

Artwork lives under assets/theme-art. Conversion, resampling and palette
reduction happen on the authoring PC; the launcher loads ordinary bounded PNGs.
"""

import argparse
import configparser
import io
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw, ImageOps

if __package__:
    from .theme_materials import tinted_material
else:
    from theme_materials import tinted_material

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
ART = ROOT / "assets/theme-art"
SUBJECTS = {
    "copper": "Patinated artisan workshop",
    "slate": "Cinematic city rain",
    "folio": "Pressed botanical paper collage",
    "nocturne": "Moonlit storybook rooftops",
    "tide": "Painted coastal headland",
    "phosphor": "Tactile expedition station",
    "eclipse": "Surreal painted planetary landscape",
    "prism": "Playful cut-color composition",
    "ember": "Illustrated autumn campsite",
    "glacier": "Layered paper mountain landscape",
    "garnet": "Jewel-glass art nouveau interior",
}


def prism(screen):
    """Original broad vector shapes; no scanlines, grid or technical ornament."""
    image = Image.new("RGB", (1024, 768), "#fff6ed" if not screen else "#d00061")
    d = ImageDraw.Draw(image)
    if screen:
        for x in range(-180, 1200, 240):
            d.ellipse((x, -170, x + 300, 140), fill="#fff6ed")
        d.ellipse((820, 540, 1220, 940), fill="#f877a6")
    else:
        d.ellipse((650, -130, 1100, 330), fill="#f4b6ce")
        d.ellipse((830, 50, 1130, 350), fill="#ebc83c")
        d.polygon(
            [(580, 660), (830, 450), (1100, 700), (1100, 768), (580, 768)],
            fill="#e34c89",
        )
        d.ellipse((-220, 520, 350, 1120), fill="#d00061")
    return image.resize((256, 192), Image.Resampling.LANCZOS)


def scene(name, screen):
    if name == "prism":
        return prism(screen)
    # Screen-specific masters share a setting, not the same hero composition.
    filename = name + ("-bottom" if screen else "") + ".png"
    with Image.open(ART / filename) as master:
        image = ImageOps.fit(
            master.convert("RGB"), (256, 192), method=Image.Resampling.LANCZOS
        )
    return image


def assets():
    for name in SUBJECTS:
        config = configparser.ConfigParser()
        config.read(THEMES / name / "theme.ini")
        for screen in range(2):
            output = io.BytesIO()
            # An indexed PNG is decoded into the same fixed RGB555 background;
            # smaller SD reads do not sacrifice the authored smooth composition.
            scene(name, screen).quantize(colors=256, dither=Image.Dither.NONE).save(
                output, format="PNG", optimize=True
            )
            yield THEMES / name / "backgrounds" / (
                "scene-top.png" if not screen else "scene-bottom.png"
            ), output.getvalue()
            # Calm text bands are precomposited while loading. They protect
            # status/help labels even when a theme omits conventional chrome.
            shade = (
                {
                    "slate": 35,
                    "eclipse": 45,
                    "ember": 65,
                    "phosphor": 50,
                    "prism": 0,
                }.get(name, 86)
                if not screen
                else (
                    20
                    if name
                    in (
                        "folio",
                        "tide",
                        "prism",
                        "copper",
                        "garnet",
                        "glacier",
                        "ember",
                    )
                    else 42
                )
            )
            color = ImageColor.getrgb(config["colors"]["background"])
            surface = Image.new("RGBA", (256, 192), color + (round(shade * 2.55),))
            # Baked material bands also work with minimal chrome. Preparation
            # uses the existing reading surface; no runtime texture layer is kept.
            material = tinted_material(name, color, (256, 192))
            for top, bottom, alpha in ((0, 34 if screen else 24, 240), (176, 192, 245)):
                band = material.crop((0, top, 256, bottom)).convert("RGBA")
                band.putalpha(alpha)
                surface.paste(band, (0, top))
            output = io.BytesIO()
            surface.save(output, format="PNG", optimize=True)
            yield THEMES / name / "backgrounds" / (
                "reading-top.png" if not screen else "reading-bottom.png"
            ), output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for path, data in assets():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise SystemExit(f"Scene differs: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    print(
        "Checked 44 backgrounds/surfaces"
        if args.check
        else "Wrote 44 backgrounds/surfaces"
    )


if __name__ == "__main__":
    main()
