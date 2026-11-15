"""Prepare small original physical-material repeats on the authoring PC."""

from functools import lru_cache
from pathlib import Path
import math

from PIL import Image, ImageColor, ImageFilter, ImageStat

ROOT = Path(__file__).resolve().parents[1]
# Atlas cells describe different physical structures, not palette swaps.
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
def material_tile(theme, selected=False):
    """Keep a small, native-scale grain; soften both joins before palette reduction."""
    cell, _ = MATERIALS[theme]
    if selected:
        # Focus changes the physical surface as well as its trim: frosted
        # ceramic over Slate's felt, linen applique over Nocturne's denim,
        # and a satin inset over Garnet's velvet. Others use a separate sample.
        cell = {"slate": 7, "nocturne": 11, "garnet": 6}.get(theme, cell)
    with Image.open(ROOT / "assets/theme-art/materials.png") as master:
        if master.size != (1024, 768):
            raise ValueError("Material master must be a 4x3 atlas of 256px squares")
        offset_x, offset_y = (48, 160) if selected else (112, 112)
        x, y = (cell % 4) * 256 + offset_x, (cell // 4) * 256 + offset_y
        tile = master.crop((x, y, x + 32, y + 32)).convert("RGB")
    tile = tile.resize((16, 16), Image.Resampling.LANCZOS)
    # Remove broad photographic lighting before repeating a small sample. It
    # would otherwise become an obvious checkerboard across a long text band.
    lighting = tile.filter(ImageFilter.GaussianBlur(1.5))
    detail = Image.new("RGB", tile.size)
    detail.putdata(
        [
            tuple(max(0, min(255, 128 + c - low)) for c, low in zip(pixel, blurred))
            for pixel, blurred in zip(
                tile.get_flattened_data(), lighting.get_flattened_data()
            )
        ]
    )
    tile = detail
    # Equal endpoints avoid a hard seam. The adjacent columns/rows keep the
    # material's own fibers, pores or glaze rather than adding procedural noise.
    for axis in range(2):
        pixels = tile.load()
        for line in range(16):
            for edge in range(2):
                a = (edge, line) if not axis else (line, edge)
                b = (15 - edge, line) if not axis else (line, 15 - edge)
                left, right = pixels[a], pixels[b]
                mix = 0.5 if edge == 0 else 0.2
                pixels[a] = tuple(
                    round(l * (1 - mix) + r * mix) for l, r in zip(left, right)
                )
                pixels[b] = tuple(
                    round(r * (1 - mix) + l * mix) for l, r in zip(left, right)
                )
    return tile


def tinted_material(theme, color, size, strength=1.0, *, selected=False):
    """Tint a real material while keeping its mean under the theme's text palette."""
    tile = material_tile(theme, selected)
    mean = ImageStat.Stat(tile).mean
    deviation = max(1, math.sqrt(sum(ImageStat.Stat(tile).var) / 3))
    amplitude = MATERIALS[theme][1] * strength
    base = ImageColor.getrgb(color) if isinstance(color, str) else color[:3]
    pixels = [
        tuple(
            max(0, min(255, round(b + (c - m) / deviation * amplitude)))
            for b, c, m in zip(base, pixel, mean)
        )
        for pixel in tile.get_flattened_data()
    ]
    texture = Image.new("RGB", (16, 16))
    texture.putdata(pixels)
    output = Image.new("RGB", size)
    for y in range(0, size[1], 16):
        for x in range(0, size[0], 16):
            output.paste(texture, (x, y))
    return output


def texture_component(image, theme, name):
    """Texture the existing fill only; retain alpha, trim, cutouts and selection marks."""
    native = image.copy()
    source = native.load()
    output = image.load()
    # Less contrast in modal text areas; slightly stronger in short chrome bands.
    strength = 1.1 if name in ("header", "footer") else 0.8
    selected = name in ("row-selected", "card-selected")
    grain = tinted_material(
        theme, (128, 128, 128), (32, 32), strength, selected=selected
    ).load()
    for y in range(32):
        for x in range(32):
            r, g, b, alpha = source[x, y]
            if not alpha:
                continue
            delta = tuple(c - 128 for c in grain[(x - 8) % 16, (y - 8) % 16])
            # The tint is an additive detail over the already authored material.
            output[x, y] = tuple(
                max(0, min(255, c + d)) for c, d in zip((r, g, b), delta)
            ) + (alpha,)
    return image
