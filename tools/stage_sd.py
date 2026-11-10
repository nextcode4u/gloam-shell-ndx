#!/usr/bin/env python3
"""Stage our launcher into a conventional SD tree without replacing other apps."""

import argparse
import shutil
import os
import tempfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SD_TEMPLATE = PROJECT_ROOT / "sdroot"


def stage_sd(destination: Path, rom: Path) -> None:
    """Merge the template, preserve existing assets, then update our BOOT.NDS."""
    if not rom.is_file():
        raise FileNotFoundError(f"Build the launcher first: {rom}")

    destination = destination.resolve()

    def target_path(relative: Path) -> Path:
        target = destination
        for index, part in enumerate(relative.parts):
            target /= part
            if target.is_symlink():
                raise ValueError(f"SD staging path contains a symlink: {target}")
            if index < len(relative.parts) - 1 and target.exists() and not target.is_dir():
                raise NotADirectoryError(target)
        return target

    sources = list(SD_TEMPLATE.rglob("*"))
    for source in sources:
        if source.is_symlink():
            raise ValueError(f"SD template contains a symlink: {source}")
        target = target_path(source.relative_to(SD_TEMPLATE))
        if source.is_dir() and target.exists() and not target.is_dir():
            raise NotADirectoryError(target)
        if source.is_file() and target.exists() and not target.is_file():
            raise IsADirectoryError(target)
    boot = target_path(Path("BOOT.NDS"))
    if boot.exists() and not boot.is_file():
        raise IsADirectoryError(boot)

    forwarder = {}
    if (rom.resolve() == (PROJECT_ROOT / "gloam-shell-ndx.nds").resolve()
            and (PROJECT_ROOT / "dist/forwarder/manifest.json").is_file()):
        if __package__:
            from .build_forwarder import verified_files
        else:
            from build_forwarder import verified_files
        forwarder = verified_files(PROJECT_ROOT)
        for relative in forwarder:
            target = target_path(Path(relative))
            if target.exists() and not target.is_file():
                raise IsADirectoryError(target)

    destination.mkdir(parents=True, exist_ok=True)
    for source in sources:
        relative = source.relative_to(SD_TEMPLATE)
        target = destination / relative
        if source.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        elif source.name != ".gitkeep" and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    for relative, data in forwarder.items():
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)

    # This function is for our release/test trees. It intentionally updates the
    # boot file; it must never be pointed at an unrelated SD card without a backup.
    descriptor, temporary = tempfile.mkstemp(prefix=".gloam-stage-", dir=destination)
    os.close(descriptor)
    try:
        shutil.copy2(rom, temporary)
        os.replace(temporary, boot)
    finally:
        Path(temporary).unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=PROJECT_ROOT / "dist/sdroot")
    args = parser.parse_args()
    stage_sd(args.destination, PROJECT_ROOT / "gloam-shell-ndx.nds")
    print(f"Staged BOOT.NDS and SD directories in {args.destination}")


if __name__ == "__main__":
    main()
