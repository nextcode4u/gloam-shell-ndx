#!/usr/bin/env python3
"""Build the unsigned, installable Gloam tile; never install or modify a NAND."""

import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TITLE_ID = 0x00030004474C4458
FILES = ("gloam-forwarder.dsi", "gloam-forwarder.tmd")


def unsigned_tmd(rom: bytes) -> bytes:
    if len(rom) < 0x4000 or rom[12:16] != b"GLDX" or rom[0x12] != 3:
        raise ValueError("Not the DSi-only Gloam forwarder")
    if struct.unpack_from("<Q", rom, 0x230)[0] != TITLE_ID:
        raise ValueError("Unexpected forwarder title ID")
    if any(rom[0x238:0x240]):
        raise ValueError("The forwarder must not request public/private saves")
    # A standard single-content record with an explicitly absent RSA signature.
    tmd = bytearray(0x208)
    struct.pack_into(">I", tmd, 0, 0x10001)
    issuer = b"Root-CA00000001-CP00000004"
    tmd[0x140 : 0x140 + len(issuer)] = issuer
    struct.pack_into(">Q", tmd, 0x18C, TITLE_ID)
    tmd[0x198:0x19A] = rom[0x10:0x12]
    tmd[0x1AA:0x1BA] = b"\x80" * 16
    struct.pack_into(">HH", tmd, 0x1DC, 1, 1)
    struct.pack_into(">IHHQ", tmd, 0x1E4, 0, 0, 1, len(rom))
    tmd[0x1F4:0x208] = hashlib.sha1(rom).digest()
    return bytes(tmd)


def verified_files(root: Path = ROOT) -> dict[str, bytes]:
    directory = root / "dist/forwarder"
    for path in [root / "dist", directory, directory / "manifest.json", *(directory / n for n in FILES)]:
        if path.is_symlink():
            raise ValueError("Forwarder artifacts must not be symlinks")
    manifest = json.loads((directory / "manifest.json").read_text())
    data = {name: (directory / name).read_bytes() for name in FILES}
    for name, contents in data.items():
        if hashlib.sha256(contents).hexdigest() != manifest["sha256"][name]:
            raise ValueError(f"Forwarder artifact changed: {name}; rebuild it")
    if unsigned_tmd(data[FILES[0]]) != data[FILES[1]]:
        raise ValueError("Forwarder metadata does not match its executable")
    return {f"_nds/dsi-frontend/install/{name}": contents for name, contents in data.items()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-only", action="store_true")
    args = parser.parse_args()
    if not args.package_only:
        subprocess.run(["make", "-C", str(ROOT / "forwarder")], check=True)
    rom = (ROOT / "forwarder/gloam-forwarder.nds").read_bytes()
    data = {FILES[0]: rom, FILES[1]: unsigned_tmd(rom)}
    directory = ROOT / "dist/forwarder"
    directory.mkdir(parents=True, exist_ok=True)
    for name, contents in data.items():
        (directory / name).write_bytes(contents)
    manifest = {
        "title_id": f"{TITLE_ID:016x}",
        "unsigned": True,
        "target": "sd:/BOOT.NDS",
        "requires": "Unlaunch with launcher patches",
        "status": "development; physical DSi and SDNAND validation pending",
        "sha256": {n: hashlib.sha256(b).hexdigest() for n, b in data.items()},
    }
    (directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    verified_files()
    print(f"Built unsigned forwarder: {directory / FILES[0]} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
