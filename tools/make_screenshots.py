#!/usr/bin/env python3
"""Capture the actual host renderer with authored SD content for documentation."""

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--theme-variants", action="store_true", help="Render the boundary-theme gallery")
    args = parser.parse_args()
    preview = ROOT / "build/host/preview"
    if not preview.is_file():
        raise SystemExit("Run tools/host-check.sh to build the host renderer first.")
    output = ROOT / "docs/images"
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="screenshots-", dir=ROOT / "build") as temporary:
        card = Path(temporary) / "sd"
        shutil.copytree(ROOT / "sdroot", card)
        state = card / "_nds/dsi-frontend/state.ini"
        examples = [("browser", "nocturne", []), ("settings", "tide", ["settings"])]
        if args.theme_variants:
            examples = [(theme, theme, []) for theme in ("phosphor", "eclipse", "prism")]
        gallery = Image.new("RGB", (256 * 3, 420), "#090d12")
        for index, (name, theme, extra) in enumerate(examples):
            grid = args.theme_variants and theme != "phosphor"
            state.write_text(f"version=1\ntheme_id={theme}\nbrowser_style={3 if grid else 0}\nicon_size={1 if grid else 0}\n")
            prefix = Path(temporary) / name
            subprocess.run([str(preview), str(card), str(prefix), "1", *extra], check=True)
            combined = Image.new("RGB", (256, 396), "#090d12")
            for screen, y in [("top", 0), ("bottom", 204)]:
                with Image.open(str(prefix) + f"-{screen}.ppm") as image:
                    combined.paste(image, (0, y))
            if args.theme_variants:
                gallery.paste(combined, (index * 256, 24))
                ImageDraw.Draw(gallery).text((index * 256 + 8, 6), theme.title(), fill="white")
            else:
                combined.save(output / f"{name}.png", optimize=True)
        if args.theme_variants:
            gallery.save(output / "theme-variants.png", optimize=True)
    print("Updated documentation screenshots from the host renderer.")


if __name__ == "__main__":
    main()
