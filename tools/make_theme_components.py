#!/usr/bin/env python3
"""Build original modular nine-slice UI artwork for the bundled themes (CC0)."""

import argparse
import configparser
import io
import random
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw

if __package__:
    from .make_theme_navigation import MATERIALS, SmoothDraw, smooth_export
else:
    from make_theme_navigation import MATERIALS, SmoothDraw, smooth_export

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
COMPONENTS = (
    "header",
    "footer",
    "panel",
    "row-selected",
    "tab-selected",
    "card",
    "card-selected",
    "row",
    "icon-frame",
    "artwork-frame",
    "battery-empty",
    "battery-low",
    "battery-half",
    "battery-high",
    "battery-full",
    "battery-charging",
    "battery-unknown",
    "wireless-idle",
)


def component_image(name, style, colors, theme="copper"):
    """Broad material shapes, drawn at 4x; no bevel grids or hairline frames."""
    image = Image.new("RGBA", (128, 128))
    d = SmoothDraw(image)
    bg, face, accent, muted = (
        colors[k] for k in ("background", "surface", "accent", "muted")
    )
    secondary = MATERIALS[theme][1]
    radius = {
        "copper": 3,
        "slate": 8,
        "folio": 1,
        "nocturne": 10,
        "tide": 8,
        "phosphor": 5,
        "eclipse": 11,
        "prism": 12,
        "ember": 6,
        "glacier": 8,
        "garnet": 10,
    }[theme]
    selected = name in ("row-selected", "card-selected")
    if name.startswith("battery-"):
        d.rounded_rectangle((2, 7, 27, 24), radius=3, fill=muted)
        d.rectangle((27, 12, 30, 19), fill=muted)
        d.rounded_rectangle((5, 10, 24, 21), radius=1, fill=bg)
        level = {"empty": 0, "low": 1, "half": 2, "high": 3, "full": 4}.get(name[8:], 0)
        if level:
            d.rounded_rectangle((6, 11, 6 + level * 4, 20), radius=1, fill=accent)
        if name == "battery-charging":
            d.polygon(
                [(16, 8), (9, 18), (14, 18), (12, 24), (22, 13), (16, 13)], fill=accent
            )
        if name == "battery-unknown":
            d.ellipse((12, 13, 18, 19), fill=accent)
    elif name == "wireless-idle":
        d.arc((2, 4, 29, 31), 220, 320, fill=muted, width=3)
        d.arc((8, 10, 23, 27), 220, 320, fill=muted, width=3)
        d.ellipse((14, 22, 18, 26), fill=muted)
        # Keep the disconnected state explicit; the launcher does not associate
        # with an access point merely because the hardware radio is available.
        d.line((23, 22, 29, 28), fill=accent, width=2)
        d.line((29, 22, 23, 28), fill=accent, width=2)
    elif name in ("icon-frame", "artwork-frame"):
        # Open center and a soft, broad shadow. Native banners remain untouched.
        if theme in ("folio", "copper", "ember"):
            d.rounded_rectangle((1, 2, 31, 31), radius=radius, outline=muted, width=2)
            d.rounded_rectangle((0, 0, 29, 29), radius=radius, outline=face, width=2)
        else:
            d.rounded_rectangle((1, 1, 30, 30), radius=radius, outline=face, width=2)
    else:
        fill = accent if name == "tab-selected" else face
        if name in ("header", "footer"):
            fill = bg
        if selected:
            fill = {
                "prism": "#c5e864",
                "folio": "#f8e9c9",
                "tide": "#fff3dc",
                "glacier": "#d0e5eb",
                "ember": "#f7e7c7",
            }.get(theme, face)
        if theme == "folio":
            # Fibrous torn-paper silhouette, broad enough for nine-slice corners.
            points = [
                (1, 2),
                (5, 1),
                (8, 2),
                (15, 1),
                (21, 2),
                (27, 0),
                (30, 2),
                (31, 7),
                (30, 13),
                (31, 21),
                (30, 27),
                (31, 30),
                (24, 31),
                (18, 30),
                (12, 31),
                (7, 30),
                (2, 31),
                (0, 28),
                (1, 22),
                (0, 16),
                (1, 10),
                (0, 5),
            ]
            d.polygon(points, fill=fill)
        elif theme == "phosphor":
            d.polygon([(5, 0), (31, 0), (31, 26), (26, 31), (0, 31), (0, 5)], fill=fill)
        else:
            d.rounded_rectangle((0, 0, 31, 31), radius=radius, fill=fill)
        if selected and theme != "prism":
            # Broad selection marks reserve the live icon/text area.
            if theme in ("tide", "nocturne", "eclipse"):
                d.rounded_rectangle((0, 8, 3, 24), radius=2, fill=accent)
            elif theme == "garnet":
                d.polygon([(1, 10), (4, 16), (1, 22)], fill=accent)
            else:
                d.rounded_rectangle((1, 5, 4, 26), radius=1, fill=accent)
        if theme == "copper" and name in ("card", "card-selected"):
            d.rounded_rectangle((3, 3, 28, 6), radius=1, fill="#d8ba88")
        if theme == "ember" and name == "tab-selected":
            d.ellipse((24, 12, 28, 16), fill=bg)
    # Mild physical grain belongs only to the paper/canvas themes, not all packs.
    if theme in ("folio", "ember", "copper") and name in (
        "row",
        "row-selected",
        "card",
        "card-selected",
        "panel",
    ):
        rng = random.Random(theme + name)
        pixels = image.load()
        for y in range(128):
            for x in range(128):
                r, g, b, a = pixels[x, y]
                if a:
                    grain = rng.choice((-2, -1, 0, 0, 1, 2))
                    pixels[x, y] = tuple(
                        max(0, min(255, c + grain)) for c in (r, g, b)
                    ) + (a,)
    # Downsampling and alpha quantization happen here/on the PC, never at runtime.
    return smooth_export(image)


def assets():
    for manifest in sorted(THEMES.glob("*/theme.ini")):
        config = configparser.ConfigParser()
        config.read(manifest)
        if not config.has_section("components"):
            continue
        theme = manifest.parent.name
        style = (
            "terminal"
            if theme in ("phosphor", "slate")
            else (
                "lunar"
                if theme in ("eclipse", "nocturne")
                else (
                    "paper"
                    if theme in ("prism", "folio", "tide", "glacier")
                    else "retro"
                )
            )
        )
        for name in COMPONENTS:
            output = io.BytesIO()
            component_image(name, style, config["colors"], theme).save(
                output, format="PNG", optimize=True
            )
            yield manifest.parent / "components" / (name + ".png"), output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    count = 0
    for path, data in assets():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise SystemExit(f"Component differs: {path.relative_to(ROOT)}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        count += 1
    print(f'{"Checked" if args.check else "Wrote"} {count} theme components')


if __name__ == "__main__":
    main()
