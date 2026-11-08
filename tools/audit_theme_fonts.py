#!/usr/bin/env python3
"""Probe bitmap-font metrics against current UI regions; does not load fonts on DS."""

import argparse
import json
import math
from pathlib import Path

from PIL import ImageFont, features

# These regions expose current layout assumptions, not final theme constraints.
# Dynamic names may truncate; navigation labels must remain understandable.
SAMPLES = (
    ("category", "System", 58, 16, "must fit"),
    ("quick_label", "Reduced motion", 174, 14, "must fit"),
    ("quick_value", "Classic", 49, 14, "must fit"),
    ("browser_name", "A long game title with spaces.nds", 198, 18, "ellipsize"),
    ("top_name", "Nintendo DS title with a long name", 156, 12, "ellipsize"),
    ("footer", "A Open B Back Y Info SEL Actions", 236, 13, "must fit or reflow"),
    ("descenders", "Agjpqy 0123456789", 198, 12, "must fit vertically"),
)


def measure_unshaped(font: ImageFont.FreeTypeFont, text: str) -> tuple[int, tuple]:
    """Match a first bitmap renderer with integer advances and no pair kerning."""
    pen = 0
    ink = None
    for character in text:
        left, top, right, bottom = font.getbbox(character, mode="1", anchor="ls")
        if right > left and bottom > top:
            bounds = (pen + left, top, pen + right, bottom)
            if ink is None:
                ink = bounds
            else:
                ink = (
                    min(ink[0], bounds[0]),
                    min(ink[1], top),
                    max(ink[2], bounds[2]),
                    max(ink[3], bottom),
                )
        pen += math.ceil(font.getlength(character, mode="1"))
    return pen, ink or (0, 0, 0, 0)


def probe_font(path: Path, requested_size: int) -> dict:
    result = {"font": str(path), "requested_size": requested_size}
    try:
        result["file_bytes"] = path.stat().st_size
        # A PCF may contain only one bitmap strike. Prefer the requested size,
        # then report a supported strike rather than pretending it was scaled.
        candidates = [requested_size] + [size for size in range(8, 25) if size != requested_size]
        font = None
        for size in candidates:
            try:
                font = ImageFont.truetype(str(path), size)
                result["accepted_request_size"] = size
                break
            except OSError:
                continue
        if font is None:
            result["error"] = "No supported face/strike found by the host FreeType probe"
            return result
        ascent, descent = font.getmetrics()
        result["ascent"] = ascent
        result["descent"] = descent
        samples = []
        for name, text, width, height, policy in SAMPLES:
            advance, bounds = measure_unshaped(font, text)
            extent = max(advance, bounds[2]) - min(0, bounds[0])
            line_height = max(ascent, -bounds[1]) + max(descent, bounds[3])
            samples.append(
                {
                    "region": name,
                    "text": text,
                    "available_pixels": [width, height],
                    "policy": policy,
                    "advance_pixels": advance,
                    "ink_bounds_from_baseline": list(bounds),
                    "required_extent_pixels": [extent, line_height],
                    "fits_width": extent <= width,
                    "fits_height": line_height <= height,
                }
            )
        result["samples"] = samples
    except (OSError, ValueError) as error:
        result["error"] = str(error)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fonts", type=Path, nargs="+")
    parser.add_argument("--size", type=int, default=12)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not 8 <= args.size <= 48:
        parser.error("Use a requested size between 8 and 48 pixels")
    if args.output and any(args.output.resolve() == font.resolve() for font in args.fonts):
        parser.error("The report must not replace an input font")
    report = {
        "scope": "Unshaped integer-advance host probe; not native PCF compatibility or glyph-coverage validation",
        "freetype": features.version_module("freetype2"),
        "current_builtin": {"advance_pixels": 7, "cell_height_pixels": 12, "baseline_pixels": 9},
        "fonts": [probe_font(path, args.size) for path in args.fonts],
    }
    output = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output)
        print(f"Wrote {len(report['fonts'])} host font probes to {args.output}")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
