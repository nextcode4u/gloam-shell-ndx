#!/usr/bin/env python3
"""Write an original synthetic R4 database and header; no real game/cheat data."""
import argparse
import struct
import zlib
from pathlib import Path


def align(data: bytes) -> bytes:
    return data + bytes(-len(data) % 4)


def cheat(name: str, note: str, address: int) -> bytes:
    body = align(name.encode() + b"\0" + note.encode() + b"\0")
    body += struct.pack("<III", 2, address, 0)
    return struct.pack("<I", len(body) // 4) + body


def write_fixture(root: Path) -> None:
    game = bytearray(512)
    game[12:16] = b"TST0"
    record = align(b"Original parser fixture\0") + struct.pack("<9I", 4, *([0] * 8))
    record += struct.pack("<I", 0x11000002) + align(b"Choose one\0Exclusive fixture group\0")
    record += cheat("First choice", "First original parser test", 0x02000000)
    record += cheat("Second choice", "Second original parser test", 0x02000004)
    record += cheat("Independent", "Independent original parser test", 0x02000008)
    database = bytearray(0x120)
    database[:12] = b"R4 CheatCode"
    struct.pack_into("<IIQ", database, 0x100, int.from_bytes(b"TST0", "little"),
                     zlib.crc32(game) ^ 0xFFFFFFFF, 0x120)
    database += record
    (root / "roms").mkdir(parents=True, exist_ok=True)
    extras = root / "_nds/dsi-frontend/extras"
    extras.mkdir(parents=True, exist_ok=True)
    (root / "roms/Cheat parser fixture.nds").write_bytes(game)
    (extras / "usrcheat.dat").write_bytes(database)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    write_fixture(args.destination)
    print("Wrote synthetic cheat fixture; it is not a runnable game")


if __name__ == "__main__":
    main()
