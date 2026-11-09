#!/usr/bin/env python3
"""Build a native 7x12 ASCII or Latin-1 theme font, rejecting glyph overflow before packing."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def glyph_bitmap(font: ImageFont.FreeTypeFont, character: str, baseline: int) -> Image.Image:
    """Measure the actual monochrome ink before placing it in the native cell."""
    if math.ceil(font.getlength(character, mode="1")) > 7:
        raise ValueError(f"Glyph {character!r} does not fit the native 7x12 envelope")
    # Grayscale metrics differ from monochrome hinting (notably lowercase w).
    # Render the complete hinted mask first, including any negative bearing.
    left, top, right, bottom = font.getbbox(character, mode="1", anchor="ls")
    mask = Image.new("L", (max(1, right - left), max(1, bottom - top)))
    drawing = ImageDraw.Draw(mask)
    drawing.fontmode = "1"
    drawing.text((-left, -top), character, font=font, fill=255, anchor="ls")
    bitmap = Image.new("L", (7, 12))
    bounds = mask.getbbox()
    if bounds is None:
        return bitmap
    ink_left, ink_top = left + bounds[0], top + bounds[1]
    ink_right, ink_bottom = left + bounds[2], top + bounds[3]
    if ink_right - ink_left > 7 or ink_top + baseline < 0 or ink_bottom + baseline > 12:
        raise ValueError(f"Glyph {character!r} does not fit the native 7x12 envelope")
    # Keep normal placement unless ink would cross either edge of the cell.
    x_offset = max(-ink_left, min(0, 7 - ink_right))
    bitmap.paste(mask.crop(bounds), (ink_left + x_offset, ink_top + baseline))
    return bitmap


def pack_font(source: Path, size: int, baseline: int, latin1: bool = False) -> bytes:
    font = ImageFont.truetype(str(source), size)
    header = (
        b"DSFONT02" + bytes([7, 12, 32, 95, 160, 96, 0, 0])
        if latin1
        else (b"DSFONT01" + bytes([7, 12, 32, 95, 0, 0, 0, 0]))
    )
    output = bytearray(header)
    codepoints = list(range(32, 127)) + (list(range(160, 256)) if latin1 else [])
    for codepoint in codepoints:
        character = chr(codepoint)
        bitmap = glyph_bitmap(font, character, baseline)
        for row in range(12):
            bits = sum(
                (3 if bitmap.getpixel((column, row)) else 0) << (column * 2) for column in range(7)
            )
            output.extend(struct.pack("<H", bits))
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, default=11)
    parser.add_argument("--baseline", type=int, default=9)
    parser.add_argument("--latin1", action="store_true", help="Include U+00A0 through U+00FF")
    parser.add_argument("--fallback-header", type=Path, help="Write the Latin-1 fallback C table")
    args = parser.parse_args()
    if args.fallback_header and not args.latin1:
        parser.error("--fallback-header requires --latin1")
    try:
        data = pack_font(args.source, args.size, args.baseline, args.latin1)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    if args.fallback_header:
        rows = struct.unpack("<1152H", data[16 + 95 * 24 :])
        lines = [
            f"/* Generated Latin-1 fallback: {args.source.name}, size {args.size}, baseline {args.baseline}.",
            " * Rebuild with pack_theme_font.py --latin1. License is retained with Copper/fonts. */",
            "static const uint16_t ui_font_latin[96][12] = {",
        ]
        for glyph in range(96):
            lines.append(
                "    {"
                + ", ".join(f"0x{x:04x}" for x in rows[glyph * 12 : (glyph + 1) * 12])
                + "},"
            )
        lines.append("};")
        args.fallback_header.parent.mkdir(parents=True, exist_ok=True)
        args.fallback_header.write_text("\n".join(lines) + "\n")
    print(f"Wrote {len(data)} bytes to {args.output}")


if __name__ == "__main__":
    main()
