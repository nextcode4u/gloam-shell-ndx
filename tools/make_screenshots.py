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
    parser.add_argument(
        "--theme-variants",
        nargs="?",
        const="boundary",
        choices=("boundary", "atmosphere", "personality"),
        help="Render a theme gallery",
    )
    parser.add_argument(
        "--theme-dialogs", action="store_true", help="Render settings and cover pickers"
    )
    args = parser.parse_args()
    if args.theme_dialogs and args.theme_variants:
        parser.error("Choose either --theme-dialogs or --theme-variants")
    preview = ROOT / "build/host/preview"
    if not preview.is_file():
        raise SystemExit("Run tools/host-check.sh to build the host renderer first.")
    output = ROOT / "docs/images"
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="screenshots-", dir=ROOT / "build"
    ) as temporary:
        card = Path(temporary) / "sd"
        shutil.copytree(ROOT / "sdroot", card)
        state = card / "_nds/dsi-frontend/state.ini"
        examples = [("browser", "nocturne", []), ("settings", "tide", ["settings"])]
        if args.theme_variants:
            names = (
                (
                    "copper",
                    "slate",
                    "folio",
                    "nocturne",
                    "tide",
                    "phosphor",
                    "eclipse",
                    "prism",
                    "ember",
                    "glacier",
                    "garnet",
                )
                if args.theme_variants == "personality"
                else (
                    ("ember", "glacier", "garnet")
                    if args.theme_variants == "atmosphere"
                    else ("phosphor", "eclipse", "prism")
                )
            )
            examples = [(theme, theme, []) for theme in names]
        if args.theme_dialogs:
            examples = [
                (f"{theme}-{mode}", theme, [mode])
                for mode in ("settings", "picker")
                for theme in ("copper", "folio", "tide", "prism")
            ]
            artwork = card / "media/artwork"
            artwork.mkdir(parents=True, exist_ok=True)
            for filename in ("Botanical cover.png", "Coastal cover.png"):
                shutil.copy2(
                    ROOT / "tests/fixtures/cover-example.png", artwork / filename
                )
        columns = 4 if args.theme_dialogs or args.theme_variants == "personality" else 3
        rows = (len(examples) + columns - 1) // columns
        gallery = Image.new("RGB", (256 * columns, 420 * rows), "#090d12")
        for index, (name, theme, extra) in enumerate(examples):
            views = {
                "phosphor": (0, 0),
                "eclipse": (3, 1),
                "prism": (3, 1),
                "ember": (1, 1),
                "glacier": (3, 1),
                "garnet": (2, 0),
            }
            view, size = views.get(theme, (1, 0)) if args.theme_variants else (0, 0)
            state.write_text(
                f"version=1\ntheme_id={theme}\nbrowser_style={view}\nicon_size={size}\n"
            )
            prefix = Path(temporary) / name
            subprocess.run(
                [str(preview), str(card), str(prefix), "1", *extra], check=True
            )
            combined = Image.new("RGB", (256, 396), "#090d12")
            for screen, y in [("top", 0), ("bottom", 204)]:
                with Image.open(str(prefix) + f"-{screen}.ppm") as image:
                    combined.paste(image, (0, y))
            if args.theme_variants or args.theme_dialogs:
                x, y = (index % columns) * 256, (index // columns) * 420
                gallery.paste(combined, (x, y + 24))
                ImageDraw.Draw(gallery).text(
                    (x + 8, y + 6),
                    (
                        name.replace("-", " / ").title()
                        if args.theme_dialogs
                        else theme.title()
                    ),
                    fill="white",
                )
            else:
                combined.save(output / f"{name}.png", optimize=True)
        if args.theme_variants or args.theme_dialogs:
            filename = (
                "theme-personalities.png"
                if args.theme_variants == "personality"
                else (
                    "theme-atmosphere.png"
                    if args.theme_variants == "atmosphere"
                    else "theme-variants.png"
                )
            )
            if args.theme_dialogs:
                filename = "theme-controls.png"
            gallery.save(output / filename, optimize=True)
    print("Updated documentation screenshots from the host renderer.")


if __name__ == "__main__":
    main()
