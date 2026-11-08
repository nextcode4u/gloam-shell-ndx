#!/usr/bin/env python3
"""Stage our launcher into a conventional SD tree without replacing other apps."""

import argparse
import shutil
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SD_TEMPLATE = PROJECT_ROOT / "sdroot"


def stage_sd(destination: Path, rom: Path) -> None:
    """Merge the template, preserve existing assets, then update our BOOT.NDS."""
    if not rom.is_file():
        raise FileNotFoundError(f"Build the launcher first: {rom}")

    destination.mkdir(parents=True, exist_ok=True)
    for source in SD_TEMPLATE.rglob("*"):
        relative = source.relative_to(SD_TEMPLATE)
        target = destination / relative
        if source.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        elif source.name != ".gitkeep" and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    if (
        rom.resolve() == (PROJECT_ROOT / "gloam-shell-ndx.nds").resolve()
        and (PROJECT_ROOT / "dist/forwarder/manifest.json").is_file()
    ):
        if __package__:
            from .build_forwarder import verified_files
        else:
            from build_forwarder import verified_files
        for relative, data in verified_files(PROJECT_ROOT).items():
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)

    # This function is for our release/test trees. It intentionally updates the
    # boot file; it must never be pointed at an unrelated SD card without a backup.
    shutil.copy2(rom, destination / "BOOT.NDS")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=PROJECT_ROOT / "dist/sdroot")
    args = parser.parse_args()
    stage_sd(args.destination, PROJECT_ROOT / "gloam-shell-ndx.nds")
    print(f"Staged BOOT.NDS and SD directories in {args.destination}")


if __name__ == "__main__":
    main()
