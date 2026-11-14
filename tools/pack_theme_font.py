#!/usr/bin/env python3
"""Build fixed-cell or proportional theme fonts, rejecting overflow before packing."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def glyph_bitmap(
    font: ImageFont.FreeTypeFont, character: str, baseline: int
) -> Image.Image:
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


def proportional_glyph(font, character, baseline, height):
    """Keep hinted bearings and coverage; never crop or stretch an overflowing glyph."""
    left, top, right, bottom = font.getbbox(character, anchor="ls")
    mask = Image.new("L", (max(1, right - left), max(1, bottom - top)))
    ImageDraw.Draw(mask).text(
        (-left, -top), character, font=font, anchor="ls", fill=255
    )
    bounds = mask.getbbox()
    advance = max(1, math.ceil(font.getlength(character)))
    bitmap = Image.new("L", (16, height))
    if bounds:
        ink_left, ink_top = left + bounds[0], top + bounds[1]
        ink_right, ink_bottom = left + bounds[2], top + bounds[3]
        shift = max(0, -ink_left)
        advance = max(advance + shift, ink_right + shift)
        if ink_top + baseline < 0 or ink_bottom + baseline > height:
            raise ValueError(
                f"Glyph {character!r} exceeds the {height}-pixel line height"
            )
        bitmap.paste(mask.crop(bounds), (ink_left + shift, ink_top + baseline))
    if advance > 16:
        raise ValueError(f"Glyph {character!r} exceeds the 16-pixel advance limit")
    return bitmap, advance


def pack_font(
    source: Path,
    size: int,
    baseline: int,
    latin1: bool = False,
    *,
    proportional: bool = False,
    height: int = 12,
) -> bytes:
    if not 12 <= height <= 16 or (not proportional and height != 12):
        raise ValueError(
            "Fixed fonts use height 12; proportional fonts use heights 12–16"
        )
    font = ImageFont.truetype(str(source), size)
    if proportional:
        output = bytearray(
            b"DSFONT03"
            + bytes(
                [16, height, 32, 95, 160 if latin1 else 0, 96 if latin1 else 0, 0, 0]
            )
        )
        for codepoint in list(range(32, 127)) + (
            list(range(160, 256)) if latin1 else []
        ):
            bitmap, advance = proportional_glyph(font, chr(codepoint), baseline, height)
            output.extend((advance, 0))
            for y in range(height):
                bits = sum(
                    min(3, round((bitmap.getpixel((x, y)) / 255) ** 0.82 * 3))
                    << (2 * x)
                    for x in range(advance)
                )
                output.extend(struct.pack("<I", bits))
        return bytes(output)
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
                (3 if bitmap.getpixel((column, row)) else 0) << (column * 2)
                for column in range(7)
            )
            output.extend(struct.pack("<H", bits))
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, default=11)
    parser.add_argument("--baseline", type=int, default=9)
    parser.add_argument(
        "--latin1", action="store_true", help="Include U+00A0 through U+00FF"
    )
    parser.add_argument(
        "--proportional",
        action="store_true",
        help="Write DSFONT03 variable advances and grayscale coverage",
    )
    parser.add_argument(
        "--height", type=int, default=12, help="Line height: body 12; headings 12–16"
    )
    parser.add_argument(
        "--fallback-header", type=Path, help="Write the Latin-1 fallback C table"
    )
    args = parser.parse_args()
    if args.fallback_header and not args.latin1:
        parser.error("--fallback-header requires --latin1")
    if args.fallback_header and args.proportional:
        parser.error("--fallback-header requires the legacy fixed-cell format")
    try:
        data = pack_font(
            args.source,
            args.size,
            args.baseline,
            args.latin1,
            proportional=args.proportional,
            height=args.height,
        )
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
