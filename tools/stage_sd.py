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

    def install_file(target: Path, *, source=None, data=None, replace=False,
                     prefix=".gloam-asset-") -> None:
        """Publish a complete file; a failed copy must remain retryable."""
        if target.exists() and not replace:
            return
        target.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(prefix=prefix, dir=target.parent)
        os.close(descriptor)
        try:
            if source is not None:
                shutil.copy2(source, temporary)
            else:
                Path(temporary).write_bytes(data)
            if target.exists() and not replace:
                return
            os.replace(temporary, target)
        finally:
            Path(temporary).unlink(missing_ok=True)

    destination.mkdir(parents=True, exist_ok=True)
    for source in sources:
        relative = source.relative_to(SD_TEMPLATE)
        target = destination / relative
        if source.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        elif source.name != ".gitkeep" and not target.exists():
            install_file(target, source=source)

    for relative, data in forwarder.items():
        target = destination / relative
        install_file(target, data=data, replace=True)

    # This function is for our release/test trees. It intentionally updates the
    # boot file; it must never be pointed at an unrelated SD card without a backup.
    install_file(boot, source=rom, replace=True, prefix=".gloam-stage-")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=PROJECT_ROOT / "dist/sdroot")
    args = parser.parse_args()
    stage_sd(args.destination, PROJECT_ROOT / "gloam-shell-ndx.nds")
    print(f"Staged BOOT.NDS and SD directories in {args.destination}")


if __name__ == "__main__":
    main()
