#!/usr/bin/env python3
"""Build a native 7x12 ASCII or Latin-1 theme font, rejecting glyph overflow before packing."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


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
        left, top, right, bottom = font.getbbox(character, anchor="ls")
        x_offset = max(0, -left)
        if (
            right + x_offset > 7
            or top + baseline < 0
            or bottom + baseline > 12
            or math.ceil(font.getlength(character)) > 7
        ):
            raise ValueError(f"Glyph {character!r} does not fit the native 7x12 envelope")
        bitmap = Image.new("L", (7, 12))
        drawing = ImageDraw.Draw(bitmap)
        drawing.fontmode = "1"
        drawing.text((x_offset, baseline), character, font=font, fill=255, anchor="ls")
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
