#!/usr/bin/env python3
"""Write a synthetic animated DSi banner; the fixture is not runnable software."""
import argparse
import struct
from pathlib import Path


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def write_fixture(path: Path) -> None:
    rom = bytearray(0x200 + 0x23C0)
    rom[12:16] = b"####"
    struct.pack_into("<I", rom, 0x68, 0x200)
    banner = memoryview(rom)[0x200:]
    struct.pack_into("<H", banner, 0, 0x103)
    banner[0x20:0x220] = bytes([0x11]) * 512
    struct.pack_into("<H", banner, 0x222, 0x001F)
    banner[0x1240:0x1440] = bytes([0x11]) * 512
    # A single opaque pixel lets tests check both flip bits and transparency.
    banner[0x1440] = 0x01
    struct.pack_into("<H", banner, 0x2242, 0x001F)
    struct.pack_into("<H", banner, 0x2262, 0x7C00)
    struct.pack_into("<HHH", banner, 0x2340, 2, 0xC901, 0)
    struct.pack_into("<H", banner, 8, crc16(banner[0x1240:0x23C0]))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(rom)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    write_fixture(args.output)
    print("Wrote original animated-banner fixture; it is not runnable")


if __name__ == "__main__":
    main()
