"""Authored PNG fixtures; no external assets or Python dependencies."""

from pathlib import Path
import struct
import sys
import zlib


def write_png(path, width, height, pixel):
    def chunk(kind, data):
        return (
            struct.pack(">I", len(data))
            + kind
            + data
            + struct.pack(">I", zlib.crc32(kind + data))
        )

    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x, y))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def grayscale(shade):
    return lambda x, y: (shade(x, y),) * 3 + (255,)


def component_pixel(x, y):
    if (x, y) == (0, 0):
        return (0, 0, 0, 0)
    if (x, y) == (1, 1):
        return (255, 0, 0, 255)
    return (64, 96, 128, 255)


if __name__ == "__main__":
    themes = Path(sys.argv[1]) / "_nds/dsi-frontend/themes"
    folder = themes / "glass"
    (folder / "components").mkdir(parents=True)
    (folder / "theme.ini").write_text(
        "[theme]\nversion=2\n[components]\ndirectory=components\nalpha=coverage\nfill=tile\n"
    )
    write_png(
        folder / "components/row-selected.png",
        32,
        32,
        lambda x, y: (255, 0, 0, (x // 8) * 85),
    )
    write_png(
        folder / "components/header.png",
        32,
        32,
        lambda x, y: (x * 8, y * 8, (x + y) * 4, 255),
    )

    folder = themes / "material"
    (folder / "components").mkdir(parents=True)
    (folder / "theme.ini").write_text(
        "[theme]\nversion=2\n[components]\ndirectory=components\nalpha=coverage\n"
        "material=normal.png\nfocus_material=focus.png\n"
    )
    write_png(
        folder / "normal.png", 256, 192, grayscale(lambda x, y: x * 15 // 255 * 17)
    )
    write_png(
        folder / "focus.png",
        256,
        192,
        grayscale(lambda x, y: (15 - y * 15 // 191) * 17),
    )
    write_png(folder / "neutral.png", 256, 192, grayscale(lambda x, y: 136))
    write_png(
        folder / "invalid.png",
        256,
        192,
        lambda x, y: (136, 136, 136, 0 if x == 0 else 255),
    )
    for name in ("row", "row-selected", "header"):
        write_png(folder / "components" / (name + ".png"), 32, 32, component_pixel)
