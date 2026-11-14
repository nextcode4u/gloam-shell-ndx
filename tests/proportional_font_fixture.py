"""Independent DSFONT03 fixtures with known advances and no font-library dependency."""

from pathlib import Path
import struct
import sys


def font(height):
    data = bytearray(b"DSFONT03" + bytes((16, height, 32, 95, 0, 0, 0, 0)))
    for cp in range(32, 127):
        advance = 3 if chr(cp) in " il" else 10 if cp == ord("W") else 7
        data.extend((advance, 0))
        for y in range(height):
            row = (
                sum(3 << (x * 2) for x in range(advance - 1))
                if cp != 32 and 2 <= y < height - 2
                else 0
            )
            data.extend(struct.pack("<I", row))
    return data


if __name__ == "__main__":
    folder = Path(sys.argv[1]) / "_nds/dsi-frontend/themes/type"
    folder.mkdir(parents=True)
    (folder / "body.dsfont").write_bytes(font(12))
    (folder / "heading.dsfont").write_bytes(font(16))
    (folder / "theme.ini").write_text(
        "[theme]\nversion=2\n[fonts]\nui=body.dsfont\nheading=heading.dsfont\n"
    )
