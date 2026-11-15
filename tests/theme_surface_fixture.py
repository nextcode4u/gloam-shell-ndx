# SPDX-License-Identifier: GPL-3.0-or-later
"""Original synthetic pixels exercise prepared-screen parity and overrides."""

from pathlib import Path
import sys
import struct
import zlib
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.pack_theme_assets import pack

root = Path(sys.argv[1]) / "_nds/dsi-frontend/themes/prepared"
root.mkdir(parents=True)
(root / "theme.ini").write_text("""[theme]
version=2
name=Prepared
[colors]
background=#17334f
[backgrounds]
strength=73
top=top.png
bottom=bottom.png
top_surface=surface.png
bottom_surface=surface.png
[layout]
top_layout=scene
content_opacity=64
top_content_opacity=52
bottom_content_opacity=81
[components]
directory=components
alpha=coverage
material=material.png
focus_material=focus.png
[assets]
prepared=assets.gfx
""")
for name, size in [
    ("top.png", (320, 240)),
    ("bottom.png", (300, 300)),
    ("surface.png", (256, 192)),
    ("material.png", (256, 192)),
    ("focus.png", (256, 192)),
]:
    image = Image.new("RGBA", size)
    if name in ("material.png", "focus.png"):
        image.putdata(
            [
                ((x * 17 + y * 11) % 256,) * 3 + (255,)
                for y in range(size[1])
                for x in range(size[0])
            ]
        )
    else:
        image.putdata(
            [
                (
                    (x * 13) % 256,
                    (y * 7) % 256,
                    ((x + y) * 19) % 256,
                    (x * 5 + y * 3) % 256,
                )
                for y in range(size[1])
                for x in range(size[0])
            ]
        )
    image.save(root / name)
(root / "components").mkdir()
Image.new("RGBA", (32, 32), (91, 103, 131, 255)).save(
    root / "components/row-selected.png"
)
data = pack(root)
(root / "assets.gfx").write_bytes(data)
bad = bytearray(data)
bad[-1] ^= 1
(root / "broken.gfx").write_bytes(bad)
print("Prepared synthetic screen/material fixture")

raw = zlib.decompress(data[16:])
count = struct.unpack_from("<I", data, 12)[0]
offset = 0
while raw[offset] != 2:
    offset += 8 + struct.unpack_from("<I", raw, offset + 4)[0]
length = struct.unpack_from("<I", raw, offset + 4)[0]


def save(name, payload, records=count):
    (root / name).write_bytes(
        b"GLMART02" + struct.pack("<II", len(payload), records) + zlib.compress(payload)
    )


wrong = bytearray(raw)
struct.pack_into("<I", wrong, offset + 12, 0x80)
save("invalid-flags.gfx", wrong)
wrong = bytearray(raw)
struct.pack_into("<I", wrong, offset + 4, 1)
save("invalid-length.gfx", wrong)
save("duplicate-screen.gfx", raw + raw[offset : offset + 8 + length], count + 1)
(root / "oversized-v2.gfx").write_bytes(
    b"GLMART02" + struct.pack("<II", 512 * 1024 + 1, 1)
)
