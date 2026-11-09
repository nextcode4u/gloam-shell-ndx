#!/usr/bin/env python3
"""Export approved artwork as a 32x32 DS banner icon with 15 opaque colors."""

import argparse
from pathlib import Path

from PIL import Image


def pack_icon(source: Path, output: Path) -> None:
    with Image.open(source) as artwork:
        icon = artwork.convert("RGBA")
    if icon.width != icon.height:
        raise ValueError("Banner artwork must be square")
    icon = icon.resize((32, 32), Image.Resampling.LANCZOS)
    # DS banners reserve palette index zero for fully transparent pixels.
    # Quantize without dithering so tiny details do not become isolated noise.
    pixels = list(icon.get_flattened_data())
    samples = []
    for red, green, blue, alpha in pixels:
        if alpha >= 128:
            color = (red, green, blue)
            # Give small bright strokes a fair share of the limited palette.
            samples.extend([color] * (1 + (max(color) // 32) ** 2))
    if not samples:
        raise ValueError("Banner artwork has no visible pixels")
    training = Image.new("RGB", (len(samples), 1))
    training.putdata(samples)
    colors = training.quantize(
        colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE
    )
    palette = colors.getpalette()[:45]
    # Match the hardware's RGB555 channels before writing the source icon.
    palette = [(channel >> 3) * 255 // 31 for channel in palette]
    choices = [tuple(palette[i:i + 3]) for i in range(0, 45, 3)]
    indices = []
    for red, green, blue, alpha in pixels:
        if alpha < 128:
            indices.append(0)
            continue
        color = (red, green, blue)
        nearest = min(
            range(15),
            key=lambda index: sum((a - b) ** 2 for a, b in zip(color, choices[index])),
        )
        indices.append(nearest + 1)
    result = Image.new("P", icon.size)
    result.putpalette([0, 0, 0] + palette + [0] * (768 - 48))
    result.putdata(indices)
    output.parent.mkdir(parents=True, exist_ok=True)
    result.save(output, transparency=0, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pack_icon(args.source, args.output)


if __name__ == "__main__":
    main()
