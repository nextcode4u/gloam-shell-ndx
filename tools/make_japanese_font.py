#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare shared Japanese fallback rows from pinned M+ bitmap sources."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "third_party/mplus-bitmap"
OUTPUT = ROOT / "sdroot/_nds/dsi-frontend/fonts/japanese.gsf"


def read_glyphs(path, halfwidth=False):
    for block in path.read_text().split("STARTCHAR ")[1:]:
        lines = block.splitlines()
        code = int(next(line[9:] for line in lines if line.startswith("ENCODING ")))
        if halfwidth:
            if not 0xA1 <= code <= 0xDF:
                continue
            character = code - 0xA1 + 0xFF61
        else:
            try:
                text = bytes([(code >> 8) + 128, (code & 255) + 128]).decode("euc_jp")
            except (UnicodeDecodeError, ValueError):
                continue
            character = ord(text)
            if character < 0x2000:
                continue  # Existing theme/Latin/Greek/Cyrillic faces keep priority.
        width, height, x_offset, y_offset = map(
            int, next(line[4:] for line in lines if line.startswith("BBX ")).split()
        )
        advance = int(
            next(line[7:] for line in lines if line.startswith("DWIDTH ")).split()[0]
        )
        assert width <= 12 and height <= 13 and advance in (6, 12)
        start = lines.index("BITMAP") + 1
        rows = [0] * 12
        for index, line in enumerate(lines[start : lines.index("ENDCHAR")]):
            # BDF rows are MSB-left and baseline-relative. Preserve all ink;
            # 42 full-height rule/bracket glyphs fold the last row into row 11.
            y = 11 - y_offset - height + index
            bits = int(line, 16)
            padded_width = ((width + 7) // 8) * 8
            for column in range(width):
                x = column + x_offset
                if bits & (1 << (padded_width - 1 - column)):
                    assert 0 <= x < advance and 0 <= y <= 12
                    rows[min(y, 11)] |= 1 << x
        yield character, advance, rows


def generate():
    manifest = json.loads((SOURCE / "UPSTREAM.json").read_text())
    for name, record in manifest["files"].items():
        if hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("Font source hash mismatch: " + name)
    glyphs = list(read_glyphs(SOURCE / "mplus_j12r.bdf"))
    glyphs += list(read_glyphs(SOURCE / "mplus_f12r-jisx0201.bdf", True))
    glyphs.sort()
    assert len({code for code, _, _ in glyphs}) == len(glyphs)
    data = b"".join(struct.pack("<IB12H", code, width, *rows) for code, width, rows in glyphs)
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    return struct.pack("<8sIIBB2x", b"GSFONT01", len(glyphs), checksum, 12, 1) + data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    text = generate()
    if args.check:
        if OUTPUT.read_bytes() != text:
            raise SystemExit(
                "Regenerate the Japanese fallback with tools/make_japanese_font.py"
            )
    else:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_bytes(text)
    print("Japanese fallback source and external pack verified.")


if __name__ == "__main__":
    main()
