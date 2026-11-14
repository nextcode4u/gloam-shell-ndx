"""Synthetic native-decoder fixtures, independent of the contributor converter.

Only solid colour tiles and constant PCM are written here. The image/animation
authoring tools and their tests live in gloam-shell-theme-directory.
"""

from pathlib import Path
import struct
import sys
import zlib


def package(*, direct=False, bottom_only=False, sound=False):
    layers = [
        (0, 256, 192, 0, 0, 0, 0x801F),
        (1, 256, 192, 0, 0, 0, 0xFC00),
        (0, 32, 32, 112, 80, 1, 0x83E0),
    ]
    if bottom_only:
        layers = layers[1:2]
    data = bytearray(32 + len(layers) * 32)
    for index, (screen, width, height, x, y, effect, colour) in enumerate(layers):
        if direct:
            payload = struct.pack("<H", colour) * (width * height)
        else:
            payload = struct.pack("<256H", 0, colour, *([0] * 254))
            payload += bytes([1]) * (width * height)
        struct.pack_into(
            "<4B4H2h2H3I",
            data,
            32 + index * 32,
            screen,
            int(not direct),
            effect,
            0,
            width,
            height,
            1,
            3,
            x,
            y,
            0,
            30 if effect else 0,
            len(data),
            len(payload),
            0,
        )
        data.extend(payload)
        data.extend(bytes(-len(data) % 4))
    audio = struct.pack("<h", 2000) * 16000 if sound else b""
    offset = len(data) if audio else 0
    data.extend(audio)
    struct.pack_into(
        "<8sIIHHB3xII",
        data,
        0,
        b"GLOMSP01",
        len(data),
        0,
        120,
        119,
        len(layers),
        offset,
        len(audio),
    )
    struct.pack_into("<I", data, 12, zlib.crc32(data[16:]))
    return data


def fixture(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    data = package()
    (root / "valid.gsp").write_bytes(data)
    (root / "sound.gsp").write_bytes(package(sound=True))
    theme = root / "_nds/dsi-frontend/themes/probe"
    default = root / "_nds/dsi-frontend/splashes"
    theme.mkdir(parents=True, exist_ok=True)
    default.mkdir(parents=True, exist_ok=True)
    (theme / "splash.gsp").write_bytes(package(direct=True))
    (default / "default.gsp").write_bytes(package(bottom_only=True))
    for name, offset, fmt, value in (
        ("duration", 16, "H", 0),
        ("layers", 20, "B", 5),
        ("width", 36, "H", 257),
        ("frames", 40, "H", 65535),
        ("offset", 52, "I", 0),
        ("sound", 28, "I", 4),
    ):
        bad = bytearray(data)
        struct.pack_into("<" + fmt, bad, offset, value)
        struct.pack_into("<I", bad, 12, zlib.crc32(bad[16:]))
        (root / ("bad-" + name + ".gsp")).write_bytes(bad)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tests/splash_fixture.py TEMPORARY_DIRECTORY")
    fixture(sys.argv[1])
