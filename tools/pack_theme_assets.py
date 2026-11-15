#!/usr/bin/env python3
"""Prepare bounded RGB555 theme icons/components; PNGs remain authoring sources."""

import argparse
import configparser
import importlib.util
from pathlib import Path
import struct
import zlib

from PIL import Image

if __package__:
    from .theme_prepared_surfaces import screens, materials
else:
    # Authoring repositories also load this converter by file path.
    spec = importlib.util.spec_from_file_location(
        "gloam_prepared_surfaces",
        Path(__file__).with_name("theme_prepared_surfaces.py"),
    )
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    screens, materials = helper.screens, helper.materials

ICONS = "file folder game application audio video book text image save config archive shortcut system missing settings camera sound pictochat download-play system-menu slot1 tab-home tab-games tab-apps tab-media tab-system tab-files tab-folder".split()
COMPONENTS = "header footer panel row-selected tab-selected card card-selected row icon-frame artwork-frame battery-empty battery-low battery-half battery-high battery-full battery-charging battery-unknown wireless-idle update-available check-off check-on toggle-off toggle-on arrow-left arrow-right arrow-up arrow-down scroll-track scroll-thumb busy".split()


def inside(root, relative):
    path = (root / relative).resolve()
    if Path(relative).is_absolute() or ":" in relative or not path.is_relative_to(root):
        raise ValueError(f"Asset path leaves the theme: {relative}")
    return path


def sprite(image, component=True):
    if image.size != (32, 32):
        raise ValueError("Theme sprites must be 32x32")
    colors = bytearray()
    coverage = bytearray(256)
    for i, (r, g, b, a) in enumerate(image.convert("RGBA").getdata()):
        alpha = (a * 3 + 127) // 255 if component else (3 if a >= 128 else 0)
        color = 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
        colors += struct.pack("<H", color if alpha else 0)
        coverage[i // 4] |= alpha << (i % 4 * 2)
    return bytes(colors + coverage if component else colors)


def pack(root):
    root = Path(root).resolve()
    config = configparser.ConfigParser()
    config.read(root / "theme.ini")
    payload, count = bytearray(), 0
    for group, section, names in ((0, "icons", ICONS), (1, "components", COMPONENTS)):
        directory = config.get(section, "directory", fallback="")
        if not directory:
            continue
        for index, name in enumerate(names):
            path = inside(root, f"{directory}/{name}.png")
            if not path.exists():
                continue
            if path.stat().st_size > 65536:
                raise ValueError(f"Sprite exceeds 64 KiB: {path.name}")
            with Image.open(path) as image:
                data = sprite(image, bool(group))
            payload += struct.pack("<BBHI", group, 0, index, len(data)) + data
            count += 1
    for group, index, data in (
        *screens(root, config, inside),
        *materials(root, config, inside),
    ):
        payload += struct.pack("<BBHI", group, 0, index, len(data)) + data
        count += 1
    if not count or len(payload) > 512 * 1024:
        raise ValueError("Prepared asset set is empty or exceeds 512 KiB")
    return (
        b"GLMART02"
        + struct.pack("<II", len(payload), count)
        + zlib.compress(payload, 9)
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("theme", type=Path)
    args = parser.parse_args()
    root = args.theme.resolve()
    config = configparser.ConfigParser()
    config.read(root / "theme.ini")
    target = inside(root, config.get("assets", "prepared", fallback="assets.gfx"))
    data = pack(root)
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix(target.suffix + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(target)
    print(
        f"Prepared {target.name}: {len(data):,} bytes. Set [assets] prepared={target.relative_to(root)}"
    )


if __name__ == "__main__":
    main()
