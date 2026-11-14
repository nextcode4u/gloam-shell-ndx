"""Authored RGBA component fixtures; no external assets or Python dependencies."""
from pathlib import Path
import struct
import sys
import zlib


def png(path, translucent):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = bytearray()
    for y in range(32):
        rows.append(0)
        for x in range(32):
            rows.extend((255, 0, 0, (x // 8) * 85 if translucent else 255))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", 32, 32, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    folder = Path(sys.argv[1]) / "_nds/dsi-frontend/themes/glass"
    (folder / "components").mkdir(parents=True)
    (folder / "theme.ini").write_text("[theme]\nversion=2\n[components]\ndirectory=components\nalpha=coverage\n")
    png(folder / "components/row-selected.png", True)
    png(folder / "components/header.png", False)
