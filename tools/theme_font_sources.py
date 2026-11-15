"""Resolve immutable OFL font inputs for authoring; normal builds use prepared packs."""

import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config/theme-fonts.json"
CACHE = ROOT / "build/theme-font-sources"


def verified_source(family, kind, record, *, download=False, cache=CACHE):
    """Reject changed inputs, and never download unless the author requests it."""
    if not family.isascii() or not family.isalnum() or kind not in ("font", "license"):
        raise ValueError("Invalid font source identifier")
    suffix = ".ttf" if kind == "font" else ".OFL.txt"
    path = cache / (family + suffix)
    if path.is_file():
        if hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError(f"Font source checksum differs: {path}")
        return path
    if not download:
        raise FileNotFoundError(
            "Missing font authoring source; run tools/make_theme_samples.py --download-fonts"
        )
    url = record["url"]
    if not url.startswith("https://raw.githubusercontent.com/google/fonts/"):
        raise ValueError("Font downloads must use the reviewed Google Fonts source")
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read(16 * 1024 * 1024 + 1)
    if (
        len(data) > 16 * 1024 * 1024
        or hashlib.sha256(data).hexdigest() != record["sha256"]
    ):
        raise ValueError(f"Downloaded font source checksum differs: {family}/{kind}")
    cache.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)
    return path


def theme_fonts(download=False):
    manifest = json.loads(MANIFEST.read_text())
    for theme, roles in manifest["themes"].items():
        resolved = {}
        for role, settings in roles.items():
            family = settings["family"]
            face = manifest["faces"][family]
            source = verified_source(family, "font", face["font"], download=download)
            license_path = verified_source(
                family, "license", face["license"], download=download
            )
            resolved[role] = (
                source,
                {key: value for key, value in settings.items() if key != "family"},
                face["name"],
                license_path.read_text(),
            )
        yield theme, resolved
