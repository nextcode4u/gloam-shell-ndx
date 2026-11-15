#!/usr/bin/env python3
"""Build original modular nine-slice UI artwork for the bundled themes (CC0)."""

import argparse
import configparser
import io
import random
import math
from pathlib import Path

from PIL import Image, ImageColor
from functools import lru_cache

if __package__:
    from .make_theme_navigation import SmoothDraw, smooth_export
    from .theme_materials import texture_component
    from .theme_selections import selection_surface
else:
    from make_theme_navigation import SmoothDraw, smooth_export
    from theme_materials import texture_component
    from theme_selections import selection_surface

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
    "update-available",
    "check-off",
    "check-on",
    "toggle-off",
    "toggle-on",
    "arrow-left",
    "arrow-right",
    "arrow-up",
    "arrow-down",
    "scroll-track",
    "scroll-thumb",
    "busy",
)


def component_image(name, style, colors, theme="copper"):
    """Broad material shapes, drawn at 4x; no bevel grids or hairline frames."""
    image = Image.new("RGBA", (128, 128))
    d = SmoothDraw(image)
    bg, face, accent, muted = (
        colors[k] for k in ("background", "surface", "accent", "muted")
    )
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
    elif name.startswith("arrow-"):
        points = [(21, 5), (10, 16), (21, 27)]
        if name == "arrow-right":
            points = [(32 - x, y) for x, y in points]
        elif name == "arrow-up":
            points = [(y, x) for x, y in points]
        elif name == "arrow-down":
            points = [(y, 32 - x) for x, y in points]
        if theme in ("prism", "nocturne", "eclipse"):
            # Soft button caps rather than the expedition packs' open chevrons.
            d.ellipse((1, 1, 30, 30), fill=face)
            d.line(points, fill=accent, width=5)
        elif theme in ("copper", "garnet", "ember"):
            d.polygon(
                [
                    (1, 8),
                    (8, 1),
                    (24, 1),
                    (31, 8),
                    (31, 24),
                    (24, 31),
                    (8, 31),
                    (1, 24),
                ],
                fill=face,
            )
            d.line(points, fill=accent, width=3)
        else:
            d.line(points, fill=accent, width=4)
    elif name.startswith("check-"):
        d.rounded_rectangle(
            (3, 3, 28, 28),
            radius=min(radius, 7),
            fill=accent if name == "check-on" else face,
            outline=muted,
            width=2,
        )
        if name == "check-on":
            d.line([(8, 16), (13, 22), (24, 10)], fill=colors["ink"], width=4)
    elif name.startswith("toggle-"):
        enabled = name == "toggle-on"
        d.rounded_rectangle(
            (1, 5, 30, 26),
            radius=max(3, min(radius, 10)),
            fill=accent if enabled else muted,
        )
        x = 19 if enabled else 3
        knob = colors["ink"] if enabled else face
        if theme in ("folio", "copper", "ember"):
            d.rounded_rectangle((x, 7, x + 9, 24), radius=2, fill=knob)
        else:
            d.ellipse((x, 7, x + 9, 24), fill=knob)
    elif name.startswith("scroll-"):
        d.rounded_rectangle(
            (0, 0, 31, 31), radius=4, fill=accent if name == "scroll-thumb" else face
        )
    elif name == "update-available":
        d.ellipse((1, 1, 30, 30), fill=accent)
        d.line([(9, 15), (16, 8), (23, 15)], fill=colors["ink"], width=3)
        d.line((16, 9, 16, 24), fill=colors["ink"], width=3)
    elif name == "busy":

        for i in range(8):
            x = 16 + 10 * math.cos(i * math.tau / 8)
            y = 16 + 10 * math.sin(i * math.tau / 8)
            d.ellipse((x - 2, y - 2, x + 2, y + 2), fill=accent if i < 3 else muted)
    elif name in ("row-selected", "card-selected"):
        image = selection_surface(theme)
    elif name in ("icon-frame", "artwork-frame"):
        # A wide material rim with an empty center protects live cover/banner art.
        d.rounded_rectangle((0, 0, 31, 31), radius=min(radius, 7), fill=face)
        d.rounded_rectangle((4, 4, 27, 27), radius=2, fill=(0, 0, 0, 0))
    else:
        fill = accent if name == "tab-selected" else face
        if name in ("header", "footer"):
            fill = bg
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
        elif theme == "glacier":
            # Folded travel-paper corners, distinct from a rounded glass card.
            d.polygon([(0, 0), (24, 0), (31, 7), (31, 31), (7, 31), (0, 24)], fill=fill)
            d.polygon([(24, 0), (24, 7), (31, 7)], fill=muted)
        elif theme == "garnet":
            # Arched shoulders and a flat foot echo the stained-glass icons.
            d.rounded_rectangle((0, 0, 31, 31), radius=9, fill=fill)
            d.rectangle((0, 8, 31, 31), fill=fill)
        elif theme == "eclipse":
            # One rounded end and one straight edge: an offset orbital capsule.
            d.rounded_rectangle((0, 0, 31, 31), radius=12, fill=fill)
            d.rectangle((0, 0, 16, 31), fill=fill)
        elif theme == "ember":
            d.rounded_rectangle((0, 0, 31, 31), radius=3, fill=fill)
            for x, y in ((3, 3), (28, 3), (3, 28), (28, 28)):
                d.ellipse((x - 1, y - 1, x + 1, y + 1), fill=muted)
        elif theme == "copper":
            d.polygon(
                [
                    (3, 0),
                    (28, 0),
                    (31, 3),
                    (31, 28),
                    (28, 31),
                    (3, 31),
                    (0, 28),
                    (0, 3),
                ],
                fill=fill,
            )
        elif theme == "nocturne":
            d.rounded_rectangle((0, 1, 31, 30), radius=7, fill=fill)
            for x in (5, 26):
                d.ellipse((x - 2, 0, x + 2, 4), fill=fill)
                d.ellipse((x - 2, 27, x + 2, 31), fill=fill)
        else:
            d.rounded_rectangle((0, 0, 31, 31), radius=radius, fill=fill)
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
                if a and min(x, y, 127 - x, 127 - y) < 14:
                    grain = rng.choice((-2, -1, 0, 0, 1, 2))
                    pixels[x, y] = tuple(
                        max(0, min(255, c + grain)) for c in (r, g, b)
                    ) + (a,)
    if name in ("panel", "card", "icon-frame", "artwork-frame"):
        finish_material(image, theme, colors, name)
    # Downsampling and alpha quantization happen here/on the PC, never at runtime.
    native = smooth_export(image)
    if name in COMPONENTS[:10]:
        return texture_component(native, theme, name)
    return native


# Rims are authored into the existing 32px sprites. The central stretch region
# stays quiet; only the perimeter carries texture, highlights and material color.
RIMS = {
    "copper": ("#b38c59", "#72563f"),
    "slate": ("#4d505d", "#20232e"),
    "folio": ("#c6a66b", "#9e7b46"),
    "nocturne": ("#6d657f", "#24233f"),
    "tide": ("#b8d7ce", "#64989e"),
    "phosphor": ("#718575", "#263d3d"),
    "eclipse": ("#af85a9", "#513c67"),
    "prism": ("#ffd0e0", "#e873a0"),
    "ember": ("#c3a471", "#96734c"),
    "glacier": ("#e9f7f4", "#8bbdc9"),
    "garnet": ("#c8a26a", "#77604d"),
}


@lru_cache(maxsize=11)
def material_grain(theme):
    path = ROOT / "assets/theme-art" / (theme + ".png")
    if not path.exists():
        return [128] * (128 * 128)
    with Image.open(path) as master:
        # Quiet left-hand material, without stamping a miniature hero picture
        # into each corner. Only its broad tonal variation is used.
        sample = master.crop((0, 40, 160, 200)).convert("L")
        return list(
            sample.resize((128, 128), Image.Resampling.LANCZOS).get_flattened_data()
        )


def finish_material(image, theme, colors, name):
    light, shadow = (ImageColor.getrgb(c) for c in RIMS[theme])
    grain = material_grain(theme)
    pixels = image.load()
    for y in range(128):
        for x in range(128):
            r, g, b, a = pixels[x, y]
            if not a:
                continue
            edge = min(x, y, 127 - x, 127 - y) / 4
            # A broad three-pixel rim; no repeated hairline outlines.
            weight = max(0, (3.5 - edge) / 3.5)
            target = light if y < 64 else shadow
            texture = (grain[y * 128 + x] - 128) * (0.12 if weight else 0)
            rgb = tuple(
                max(0, min(255, round(c * (1 - weight) + t * weight + texture)))
                for c, t in zip((r, g, b), target)
            )
            pixels[x, y] = rgb + (a,)
    if theme == "ember" and name == "panel":
        # Sparse seam marks remain in the corners, outside the stretch/text area.
        d = SmoothDraw(image)
        for x in (5, 9, 23, 27):
            d.line((x, 2, x + 1, 2), fill="#95764d", width=1)
    if theme == "glacier" and name in ("panel", "card-selected"):
        SmoothDraw(image).polygon([(25, 0), (31, 0), (31, 6)], fill=colors["accent"])


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
