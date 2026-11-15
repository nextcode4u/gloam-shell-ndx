#!/usr/bin/env python3
"""Build original example-theme assets without modifying personal settings."""

import argparse
from pathlib import Path

if __package__:
    from .make_theme_scenes import assets as scene_assets
    from .make_audio_samples import tones
    from .pack_theme_font import pack_font
    from .theme_font_sources import theme_fonts
else:
    from make_theme_scenes import assets as scene_assets
    from make_audio_samples import tones
    from pack_theme_font import pack_font
    from theme_font_sources import theme_fonts

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
FONT_DIRECTORY = Path("/usr/share/fonts/truetype/dejavu")


def assets(download_fonts=False) -> dict[Path, bytes]:
    result = {}
    for name, pitch in [
        ("folio", 0.65),
        ("nocturne", 0.8),
        ("tide", 1.2),
        ("phosphor", 0.5),
        ("eclipse", 0.7),
        ("prism", 1.4),
        ("ember", 0.6),
        ("glacier", 1.05),
        ("garnet", 0.45),
    ]:
        for filename, notes, duration, level in [
            ("cursor", [880 * pitch], 0.025, 0.075),
            ("confirm", [660 * pitch, 990 * pitch], 0.065, 0.10),
            ("back", [550 * pitch, 330 * pitch], 0.045, 0.075),
        ]:
            result[Path(name) / "sounds" / (filename + ".wav")] = tones(
                notes, duration, level
            )
    for name, roles in theme_fonts(download_fonts):
        notices = []
        for role, (source, settings, family, notice) in roles.items():
            filename = "ui" if role == "body" else "heading"
            result[Path(name) / "fonts" / (filename + ".dsfont")] = pack_font(
                source, latin1=True, proportional=True, **settings
            )
            notices.append(f"{role.title()} face: {family}\n\n{notice.strip()}\n")
        if name == "copper":
            # The legacy Latin fixture remains for the built-in fallback tests.
            notices.append(
                "Legacy latin.dsfont / DejaVu notice:\n"
                + (THEMES / "copper/fonts/DEJAVU-LICENSE.txt").read_text()
            )
        result[Path(name) / "fonts/LICENSE.txt"] = (
            "\n".join(
                line.rstrip() for line in "\n".join(notices).splitlines()
            ).rstrip()
            + "\n"
        ).encode()
    result.update({path.relative_to(THEMES): data for path, data in scene_assets()})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Check assets without changing them"
    )
    parser.add_argument(
        "--download-fonts",
        action="store_true",
        help="Fetch checksum-pinned OFL authoring inputs",
    )
    args = parser.parse_args()
    generated = assets(args.download_fonts)
    for relative, data in generated.items():
        destination = THEMES / relative
        if args.check:
            if not destination.is_file() or destination.read_bytes() != data:
                raise SystemExit(f"Theme asset differs: {relative}")
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
    print(
        f"{'Checked' if args.check else 'Wrote'} {len(generated)} assets, "
        f"{sum(map(len, generated.values())):,} bytes"
    )


if __name__ == "__main__":
    main()
