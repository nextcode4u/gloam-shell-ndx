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


def motion_package():
    # Green square zooms/fades in and out; a blue lower-screen layer crosses edges.
    layers = [(0, 32, 32, 112, 80, 0x83E0), (1, 64, 48, -32, 175, 0xFC00)]
    data = bytearray(32 + 64 * len(layers))
    for i, (screen, width, height, x, y, color) in enumerate(layers):
        payload = struct.pack("<H", color) * width * height
        struct.pack_into("<4B4H2h2H3I", data, 32 + i * 64, screen, 0, 0, 0,
                         width, height, 1, 3, x, y, 12 if i == 0 else 0, 30,
                         len(data), len(payload), 0)
        struct.pack_into("<4h2H5B", data, 64 + i * 64,
                         48 if i == 0 else -50, -100 if i == 0 else 30, 0, 0,
                         0 if i == 0 else 100, 200 if i == 0 else 100,
                         0 if i == 0 else 32, 32, 2, 2, 0)
        data.extend(payload)
    struct.pack_into("<8sIIHHBBHII", data, 0, b"GLOMSP02", len(data), 0,
                     180, 90, len(layers), 0, 0x801F, 0, 0)
    struct.pack_into("<I", data, 12, zlib.crc32(data[16:]))
    return data


def fixture(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    modern = motion_package()
    (root / "motion.gsp").write_bytes(modern)
    for name, offset, fmt, value in (
        ("scale", 72, "H", 401), ("alpha", 76, "B", 33),
        ("easing", 78, "B", 4), ("repeat", 79, "B", 3),
        ("sequence", 80, "B", 3), ("reserved", 81, "B", 1),
        ("position", 64, "h", -513), ("background", 22, "H", 0),
        ("zero-time", 50, "H", 0)):
        bad = bytearray(modern)
        struct.pack_into("<" + fmt, bad, offset, value)
        struct.pack_into("<I", bad, 12, zlib.crc32(bad[16:]))
        (root / ("bad-motion-" + name + ".gsp")).write_bytes(bad)
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
