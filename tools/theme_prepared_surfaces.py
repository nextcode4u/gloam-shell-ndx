# SPDX-License-Identifier: GPL-3.0-or-later
"""Preblend screens and quantize material maps using the native integer rules."""

from pathlib import Path
import struct
from PIL import Image

PIXELS = 256 * 192


class UnsupportedPreparedImage(ValueError):
    """Unsupported conversions retain the native decoder quantization."""


def signature(values=(), paths=(), root=None):
    data = bytearray().join(struct.pack("<I", value) for value in values)
    for relative in paths:
        path = root / relative if relative else None
        normalized = (
            path.resolve().relative_to(root.resolve()).as_posix() if path else ""
        )
        data += normalized.encode() + b"\0"
        data += struct.pack("<I", path.stat().st_size if path and path.is_file() else 0)
    result = 2166136261
    for value in data:
        result = ((result ^ value) * 16777619) & 0xFFFFFFFF
    return result


def color555(color):
    value = int(color.removeprefix("#"), 16)
    return (
        0x8000 | ((value >> 19) & 31) | ((value >> 6) & 0x3E0) | ((value << 7) & 0x7C00)
    )


def rgba(root, relative, inside):
    if not relative:
        return None
    path = inside(root, relative)
    if not path.is_file():
        raise ValueError("Missing prepared surface input: " + relative)
    with path.open("rb") as source:
        header = source.read(25)
    # Pillow and the native decoder can quantize 16-bit channels differently.
    # Keep those sources on the established runtime path, as with JPEG.
    if len(header) < 25 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[24] == 16:
        raise UnsupportedPreparedImage(relative)
    with Image.open(path) as image:
        if image.format != "PNG":
            raise UnsupportedPreparedImage(relative)
        if not 1 <= image.width <= 512 or not 1 <= image.height <= 512:
            raise ValueError("Surface exceeds native decoder bounds: " + relative)
        return image.convert("RGBA")


def screens(root, config, inside):
    matte = color555(config.get("colors", "background", fallback="#0c1018"))
    strength = config.getint("backgrounds", "strength", fallback=25)
    default_opacity = config.getint("layout", "content_opacity", fallback=0)
    for screen, name in enumerate(("top", "bottom")):
        opacity = config.getint(
            "layout", name + "_content_opacity", fallback=default_opacity
        )
        paths = [
            config.get("backgrounds", name, fallback=""),
            config.get("backgrounds", name + "_surface", fallback=""),
        ]
        if not any(paths):
            continue
        if not all(0 <= n <= 100 for n in (strength, opacity)):
            raise ValueError("Invalid background strength or opacity")
        try:
            background, surface = (rgba(root, path, inside) for path in paths)
        except UnsupportedPreparedImage:
            continue
        pixels = [matte] * PIXELS
        wallpaper = background is not None and strength != 0
        if wallpaper:
            width, height = background.size
            crop_w, crop_h = width, height
            if width * 192 > height * 256:
                crop_w = height * 256 // 192
            else:
                crop_h = width * 192 // 256
            crop_w, crop_h = max(1, crop_w), max(1, crop_h)
            x0, y0 = (width - crop_w) // 2, (height - crop_h) // 2
            source = list(background.getdata())
            bg = [((matte >> shift) & 31) * 255 // 31 for shift in (0, 5, 10)]
            for y in range(192):
                for x in range(256):
                    r, g, b, a = source[
                        (y0 + y * crop_h // 192) * width + x0 + x * crop_w // 256
                    ]
                    alpha = a * strength
                    rgb = [
                        (channel * alpha + base * (25500 - alpha)) // 25500
                        for channel, base in zip((r, g, b), bg)
                    ]
                    pixels[y * 256 + x] = (
                        0x8000
                        | (rgb[0] >> 3)
                        | ((rgb[1] >> 3) << 5)
                        | ((rgb[2] >> 3) << 10)
                    )
        scene_requested = (
            screen == 0
            and config.get("layout", "top_layout", fallback="sidebar") == "scene"
        )
        scene = wallpaper and scene_requested
        illustration = pixels[24 * 256 : 118 * 256] if scene else []
        if surface:
            if surface.size != (256, 192):
                raise ValueError("Reading surfaces must be 256x192")
            for i, (r, g, b, a) in enumerate(surface.getdata()):
                alpha = a * opacity
                if alpha:
                    old = pixels[i]
                    channels = [
                        (
                            (
                                (channel >> 3) * alpha
                                + ((old >> shift) & 31) * (25500 - alpha)
                                + 12750
                            )
                            // 25500
                        )
                        << shift
                        for channel, shift in zip((r, g, b), (0, 5, 10))
                    ]
                    pixels[i] = 0x8000 | channels[0] | channels[1] | channels[2]
        elif wallpaper:
            for y in range(4, 188):
                for x in range(6, 250):
                    old = pixels[y * 256 + x]
                    pixels[y * 256 + x] = 0x8000 | sum(
                        (
                            (
                                ((old >> shift) & 31) * (100 - opacity)
                                + ((matte >> shift) & 31) * opacity
                            )
                            // 100
                        )
                        << shift
                        for shift in (0, 5, 10)
                    )
        flags = 1 | (2 if wallpaper else 0) | (4 if scene else 0)
        key = signature((matte, strength, opacity, int(scene_requested)), paths, root)
        data = struct.pack("<II", key, flags) + struct.pack(
            "<" + "H" * (len(pixels) + len(illustration)), *(pixels + illustration)
        )
        yield 2, screen, data


def materials(root, config, inside):
    for index, option in enumerate(("material", "focus_material")):
        path = config.get("components", option, fallback="")
        if not path:
            continue
        try:
            image = rgba(root, path, inside)
        except UnsupportedPreparedImage:
            continue
        if image.size != (256, 192):
            raise ValueError("Material maps must be 256x192")
        packed = bytearray(PIXELS // 2)
        for i, (r, g, b, a) in enumerate(image.getdata()):
            if a != 255:
                raise ValueError("Material maps must be opaque")
            shade = ((77 * r + 150 * g + 29 * b) >> 8) >> 4
            packed[i // 2] |= shade << ((i % 2) * 4)
        yield 3, index, struct.pack("<I", signature(paths=[path], root=root)) + packed
