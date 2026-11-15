"""Prepare full-image material artwork on the authoring PC; never repeat small tiles."""

import io
from functools import lru_cache
from pathlib import Path
import math

from PIL import Image, ImageColor, ImageStat

ROOT = Path(__file__).resolve().parents[1]
MATERIALS = {
    "copper": (0, 9),
    "slate": (1, 7),
    "folio": (2, 8),
    "nocturne": (3, 7),
    "tide": (4, 7),
    "phosphor": (5, 9),
    "eclipse": (6, 7),
    "prism": (7, 4),
    "ember": (8, 8),
    "glacier": (9, 6),
    "garnet": (10, 8),
}


@lru_cache(maxsize=22)
def material_image(theme, selected=False):
    """Use a complete native-size crop of the material study, without tiling it."""
    cell, _ = MATERIALS[theme]
    if selected:
        cell = {"slate": 7, "nocturne": 11, "garnet": 6}.get(theme, cell)
    with Image.open(ROOT / "assets/theme-art/materials.png") as master:
        if master.size != (1024, 768):
            raise ValueError("Material master must be a 4x3 atlas of 256px squares")
        x = (cell % 4) * 256
        y = (cell // 4) * 256 + (64 if selected else 32)
        return master.crop((x, y, x + 256, y + 192)).convert("RGB")


def tinted_material(theme, color, size, strength=1.0, *, selected=False):
    """Keep the complete material composition, with contrast subordinate to text."""
    source = material_image(theme, selected)
    stats = ImageStat.Stat(source)
    deviation = max(1, math.sqrt(sum(stats.var) / 3))
    amplitude = MATERIALS[theme][1] * strength
    base = ImageColor.getrgb(color) if isinstance(color, str) else color[:3]
    image = Image.new("RGB", source.size)
    image.putdata(
        [
            tuple(
                max(0, min(255, round(b + (c - m) / deviation * amplitude)))
                for b, c, m in zip(base, pixel, stats.mean)
            )
            for pixel in source.get_flattened_data()
        ]
    )
    return image if size == image.size else image.resize(size, Image.Resampling.LANCZOS)


def material_field(theme, selected=False):
    """Sixteen shade levels, stored as an ordinary opaque grayscale indexed PNG."""
    source = tinted_material(
        theme, (128, 128, 128), (256, 192), selected=selected
    ).convert("L")
    result = Image.new("P", source.size)
    result.putpalette(
        [channel for shade in range(256) for channel in (min(shade, 15) * 17,) * 3]
    )
    result.putdata(
        [
            max(0, min(15, round((value - 128) / 8) + 8))
            for value in source.get_flattened_data()
        ]
    )
    return result


def assets():
    root = ROOT / "sdroot/_nds/dsi-frontend/themes"
    for theme in MATERIALS:
        for selected, name in ((False, "material"), (True, "focus-material")):

            output = io.BytesIO()
            material_field(theme, selected).save(
                output, format="PNG", bits=4, optimize=True
            )
            yield root / theme / "components" / (name + ".png"), output.getvalue()
