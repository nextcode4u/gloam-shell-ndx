#!/usr/bin/env python3
"""Build original modular nine-slice UI artwork for the bundled themes (CC0)."""

import argparse
import configparser
import io
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw

if __package__:
    from .make_theme_navigation import MATERIALS
else:
    from make_theme_navigation import MATERIALS

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
    image = Image.new("RGBA", (32, 32))
    draw = ImageDraw.Draw(image)
    bg, surface, line, accent = (
        colors[key] for key in ("background", "surface", "line", "accent")
    )
    if name.startswith("battery-"):
        ink = colors["muted"] if name == "battery-unknown" else accent
        draw.rounded_rectangle(
            (1, 7, 27, 24),
            radius=3 if style == "lunar" else 1,
            fill=bg,
            outline=ink,
            width=2,
        )
        draw.rectangle((28, 12, 30, 19), fill=ink)
        bars = {"empty": 0, "low": 1, "half": 2, "high": 3, "full": 4}.get(name[8:], 0)
        for bar in range(bars):
            draw.rectangle((5 + bar * 5, 11, 7 + bar * 5, 20), fill=ink)
        if name == "battery-charging":
            draw.polygon(
                [(15, 9), (9, 17), (14, 17), (12, 23), (20, 14), (15, 14)], fill=accent
            )
        if name == "battery-unknown":
            draw.line(
                [(11, 12), (11, 10), (17, 10), (17, 14), (14, 16), (14, 18)],
                fill=ink,
                width=2,
            )
            draw.rectangle((13, 21, 15, 23), fill=ink)
        return image
    if name == "wireless-idle":
        for box in [(2, 5, 29, 31), (8, 11, 23, 27)]:
            draw.arc(box, 220, 320, fill=colors["muted"], width=2)
        draw.ellipse((14, 22, 17, 25), fill=colors["muted"])
        draw.line((3, 27, 27, 3), fill=accent, width=2)
        return image
    if name in ("icon-frame", "artwork-frame"):
        # Transparent center keeps native/animated banners and cover art intact.
        draw.rounded_rectangle(
            (0, 0, 31, 31), radius=4 if style == "lunar" else 1, outline=line, width=2
        )
        draw.line([(1, 28), (1, 1), (28, 1)], fill=accent)
        draw.line([(3, 30), (30, 30), (30, 3)], fill=bg, width=2)
        return image
    if name == "row":
        # Quiet recessed face: avoid stacking several outlines under selection.
        draw.rounded_rectangle(
            (1, 1, 30, 30), radius=4 if style == "lunar" else 1, fill=surface
        )
        draw.line((4, 30, 28, 30), fill=bg)
        return finish_material(image, name, theme)
    selected = name in ("row-selected", "card-selected")
    fill = accent if name == "tab-selected" else surface
    edge = accent if selected or name == "panel" else line
    if style == "terminal":
        draw.rectangle((0, 0, 31, 31), fill=fill)
        if selected:
            # Open brackets rather than the stock filled strip.
            for x, side in ((0, 1), (31, -1)):
                draw.line(
                    [(x + 4 * side, 0), (x, 0), (x, 31), (x + 4 * side, 31)],
                    fill=accent,
                )
        elif name in ("header", "footer"):
            y = 30 if name == "header" else 0
            draw.line((0, y, 31, y), fill=accent)
        elif name != "tab-selected":
            draw.rectangle((0, 0, 31, 31), outline=edge)
    elif style == "lunar":
        draw.rounded_rectangle((0, 0, 31, 31), radius=7, fill=fill, outline=edge)
        if selected or name == "panel":
            draw.arc((2, 2, 15, 15), 180, 270, fill=accent, width=2)
            draw.arc((16, 16, 29, 29), 0, 90, fill=accent, width=2)
        if name == "header":
            draw.line((8, 30, 23, 30), fill=accent)
    elif style == "paper":
        draw.polygon(
            [(0, 0), (25, 0), (31, 6), (31, 31), (0, 31)], fill=fill, outline=edge
        )
        draw.polygon([(25, 0), (25, 6), (31, 6)], fill=accent if selected else line)
        if selected:
            draw.line((1, 30, 30, 30), fill=accent, width=2)
    else:
        draw.rounded_rectangle((0, 0, 31, 31), radius=3, fill=fill, outline=edge)
        if name != "row-selected":
            draw.line([(2, 28), (2, 2), (28, 2)], fill=accent if selected else line)
        draw.line([(3, 30), (30, 30), (30, 3)], fill=bg, width=2)
    if name in ("row", "row-selected") and style in ("lunar", "retro"):
        # A shallow lit face and inset shadow remain legible at native row sizes.
        face = ImageColor.getrgb(surface)
        light = ImageColor.getrgb(colors["text"])
        shadow = ImageColor.getrgb(bg)
        for y in range(4, 28):
            target = light if y < 12 else shadow
            weight = (12 - y) / 100 if y < 12 else (y - 12) / 80
            color = tuple(
                round(a * (1 - weight) + b * weight) for a, b in zip(face, target)
            )
            draw.line((3, y, 28, y), fill=color)
    if name == "row":
        draw.line((3, 30, 29, 30), fill=bg, width=2)
        draw.line((2, 2, 28, 2), fill=line)
    if selected and style == "lunar":
        draw.line([(0, 9), (0, 1), (8, 1)], fill=accent, width=2)
        draw.line([(23, 30), (30, 30), (30, 23)], fill=accent, width=2)
    return finish_material(image, name, theme)


def finish_material(image, name, theme):
    """Small authored details survive nine-slicing without filling the center.

    Reading rows remain opaque. Only the compact inspector uses partial coverage;
    backgrounds themselves are shaded once during loading, not every frame.
    """
    d = ImageDraw.Draw(image)
    warm, cool, rose = MATERIALS[theme][:3]
    selected = name in ("row-selected", "card-selected")
    if name == "panel":
        alpha = image.getchannel("A").point(lambda a: 170 if a else 0)
        image.putalpha(alpha)
    if name in ("header", "footer"):
        # A restrained second ink separates orientation from selected content.
        d.line(
            (4, 30 if name == "header" else 1, 27, 30 if name == "header" else 1),
            fill=cool,
        )
    if selected:
        if theme in ("copper", "phosphor", "glacier"):
            # Enamel faces, brass fasteners and a colored instrument notch.
            for x, y in ((3, 3), (28, 3), (3, 28), (28, 28)):
                d.point((x, y), fill=warm)
            d.line((0, 10, 0, 21), fill=cool, width=2)
        elif theme in ("folio", "tide"):
            # Binding and a red pencil margin; no competing thick inner frame.
            d.line((2, 8, 2, 24), fill=warm, width=2)
            d.line((5, 30, 26, 30), fill=cool)
        elif theme == "prism":
            d.polygon([(0, 0), (7, 0), (0, 7)], fill=rose)
            d.polygon([(31, 31), (24, 31), (31, 24)], fill=cool)
        elif theme in ("nocturne", "eclipse"):
            d.arc((1, 1, 12, 12), 180, 270, fill=cool, width=2)
            d.arc((19, 19, 30, 30), 0, 90, fill=warm, width=2)
        elif theme == "garnet":
            for x in (2, 29):
                d.polygon([(x, 12), (x + 2, 16), (x, 20), (x - 2, 16)], fill=rose)
        else:
            d.line((1, 8, 1, 23), fill=cool, width=2)
    return image


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
