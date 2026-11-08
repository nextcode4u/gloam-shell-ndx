#!/usr/bin/env python3
"""Record the built frontend identity without staging an SD or emulator."""

import hashlib
import json
import os
import re
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]


def main() -> None:
    rom = PROJECT / "gloam-shell-ndx.nds"
    data = rom.read_bytes()
    if len(data) < 512:
        raise SystemExit("Build the frontend first.")
    sdk = Path(os.environ.get("BLOCKSDS", "/opt/blocksds/core"))
    header = (sdk / "libs/version/include/blocksds_version.h").read_text()
    version = re.search(r'BLOCKSDS_VERSION_STRING\s+"([^"]+)"', header)
    if not version:
        raise SystemExit("Cannot identify the installed BlocksDS version.")
    manifest = {
        "rom": "BOOT.NDS",
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "status": "development; backend and physical-hardware gates remain",
        "blocksds_recorded": version[1],
        "support": "docs/CURRENT_SUPPORT.md",
    }
    elf = PROJECT / "build/gloam-shell-ndx.elf"
    if elf.is_file():
        manifest["elf_sha256"] = hashlib.sha256(elf.read_bytes()).hexdigest()
    destination = PROJECT / "dist/build-info.json"
    destination.parent.mkdir(exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", dir=destination.parent, delete=False) as output:
        temporary = Path(output.name)
        json.dump(manifest, output, indent=2)
        output.write("\n")
    try:
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Recorded {len(data)}-byte BOOT.NDS identity ({version[1]} SDK installed).")


if __name__ == "__main__":
    main()
