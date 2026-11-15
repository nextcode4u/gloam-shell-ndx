#!/usr/bin/env python3
"""Prepare native icons from the original, independently authored theme atlases."""

import argparse
import io
from pathlib import Path

from PIL import Image, ImageDraw

if __package__:
    from .make_theme_navigation import TAB_NAMES
else:
    from make_theme_navigation import TAB_NAMES

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
ART = ROOT / "assets/theme-art"
ICON_NAMES = (
    "file",
    "folder",
    "game",
    "application",
    "audio",
    "video",
    "book",
    "text",
    "image",
    "save",
    "config",
    "archive",
    "shortcut",
    "system",
    "missing",
    "settings",
    "camera",
    "sound",
    "pictochat",
    "download-play",
    "system-menu",
    "slot1",
)
# Each theme has sixteen original objects; related entry points intentionally
# share their meaning within that theme, rather than borrowing another pack's art.
CELLS = {
    "file": 6,
    "folder": 0,
    "game": 1,
    "application": 2,
    "audio": 3,
    "video": 4,
    "book": 5,
    "text": 6,
    "image": 7,
    "save": 8,
    "config": 9,
    "archive": 10,
    "shortcut": 11,
    "system": 12,
    "missing": 6,
    "settings": 12,
    "camera": 13,
    "sound": 3,
    "pictochat": 14,
    "download-play": 4,
    "system-menu": 15,
    "slot1": 1,
    "tab-home": 15,
    "tab-games": 1,
    "tab-apps": 2,
    "tab-media": 3,
    "tab-system": 12,
    "tab-files": 0,
    "tab-folder": 0,
}


def atlas_icon(atlas, cell):
    """Fit the full object inside a two-pixel safety margin, without stretching."""
    if atlas.size != (512, 512) or atlas.mode != "RGBA":
        raise ValueError("Icon masters must be 512x512 RGBA, in a 4x4 grid")
    x, y = (cell % 4) * 128, (cell // 4) * 128
    tile = atlas.crop((x, y, x + 128, y + 128))
    # Ignore near-transparent resampling fringes when measuring the object.
    bounds = (
        tile.getchannel("A").point(lambda value: 255 if value >= 16 else 0).getbbox()
    )
    if bounds is None:
        raise ValueError(f"Empty icon cell: {cell}")
    tile = tile.crop(bounds)
    tile.thumbnail((28, 28), Image.Resampling.LANCZOS)
    tile.putalpha(
        tile.getchannel("A").point(lambda a: 0 if a < 16 else 255 if a > 239 else a)
    )
    output = Image.new("RGBA", (32, 32))
    output.paste(tile, ((32 - tile.width) // 2, (32 - tile.height) // 2))
    return output


def assets():
    for manifest in sorted(THEMES.glob("*/theme.ini")):
        with Image.open(ART / (manifest.parent.name + "-icons.png")) as atlas:
            for kind in ICON_NAMES + TAB_NAMES:
                image = atlas_icon(atlas, CELLS[kind])
                if kind == "missing":
                    # A small, high-contrast mark distinguishes a missing target
                    # while retaining the pack's document artwork.
                    draw = ImageDraw.Draw(image)
                    draw.ellipse((20, 20, 30, 30), fill="#b62e3b", outline="#fff4df")
                    draw.line((23, 23, 27, 27), fill="#fff4df", width=2)
                    draw.line((27, 23, 23, 27), fill="#fff4df", width=2)
                output = io.BytesIO()
                image.save(output, format="PNG", optimize=True)
                yield manifest.parent / "icons" / (kind + ".png"), output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    count = 0
    for path, data in assets():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise SystemExit(f"Icon differs: {path.relative_to(ROOT)}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        count += 1
    print(f'{"Checked" if args.check else "Wrote"} {count} theme icons')


if __name__ == "__main__":
    main()
