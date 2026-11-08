#!/usr/bin/env python3
"""Generate ID3 parser fixtures containing our own test images, not audio tracks."""

import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def syncsafe(size: int) -> bytes:
    return bytes((size >> shift) & 127 for shift in (21, 14, 7, 0))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    jpeg = (ROOT / "tests/fixtures/cover.jpg").read_bytes()
    png = (ROOT / "tests/fixtures/album-example.png").read_bytes()
    for version, mime, data, encoding, description in (
        (3, b"image/jpeg", jpeg, 0, b""),
        (4, b"image/png", png, 1, "Album".encode("utf-16") + b"\0"),
    ):
        picture = bytes([encoding]) + mime + b"\0\x03" + description + b"\0" + data
        length = struct.pack(">I", len(picture)) if version == 3 else syncsafe(len(picture))
        frame = b"APIC" + length + b"\0\0" + picture
        tag = b"ID3" + bytes([version, 0, 0]) + syncsafe(len(frame)) + frame
        (args.destination / f"album-v{version}.mp3").write_bytes(tag)
        broken = bytearray(tag)
        broken[6] = 255
        (args.destination / f"invalid-v{version}.mp3").write_bytes(broken)
    print("Wrote tagged image fixtures; they contain no playable audio")


if __name__ == "__main__":
    main()
