#!/usr/bin/env python3
"""Capture the actual host renderer with authored SD content for documentation."""

import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    preview = ROOT / "build/host/preview"
    if not preview.is_file():
        raise SystemExit("Run tools/host-check.sh to build the host renderer first.")
    output = ROOT / "docs/images"
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="screenshots-", dir=ROOT / "build") as temporary:
        card = Path(temporary) / "sd"
        shutil.copytree(ROOT / "sdroot", card)
        state = card / "_nds/dsi-frontend/state.ini"
        for name, theme, extra in [("browser", "nocturne", []), ("settings", "tide", ["settings"])]:
            state.write_text(f"version=1\ntheme_id={theme}\nbrowser_style=0\nicon_size=0\n")
            prefix = Path(temporary) / name
            subprocess.run([str(preview), str(card), str(prefix), "1", *extra], check=True)
            combined = Image.new("RGB", (256, 396), "#090d12")
            for screen, y in [("top", 0), ("bottom", 204)]:
                with Image.open(str(prefix) + f"-{screen}.ppm") as image:
                    combined.paste(image, (0, y))
            combined.save(output / f"{name}.png", optimize=True)
    print("Updated documentation screenshots from the host renderer.")


if __name__ == "__main__":
    main()
