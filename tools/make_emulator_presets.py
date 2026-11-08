#!/usr/bin/env python3
"""Generate emulator profiles and empty system folders from the default catalog."""

import argparse
import json
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
CATALOG = PROJECT_ROOT / "config/emulator-presets.json"


def profile_text(profile: dict) -> str:
    """Keep mode/argv/backend choices together in one named profile."""
    lines = [
        "; Editable defaults modeled on AKMenu-compatible associations.",
        "; Application paths may be changed to match your installation.",
        "[plugin]",
        "version=1",
        "name=" + profile["application_name"],
        "extensions=" + ",".join(profile["extensions"]),
    ]
    for key, value in profile["settings"].items():
        output_key = "application" if key == "path" else key
        if key in ("path", "dsipath", "ntrpath"):
            value = "sd:/" + value.lstrip("/")
        lines.append(output_key + "=" + value)
    return "\n".join(lines) + "\n"


def generate(check: bool = False) -> None:
    catalog = json.loads(CATALOG.read_text())
    if catalog["version"] != 1:
        raise ValueError("Unsupported emulator catalog version")
    template = PROJECT_ROOT / "sdroot"
    expected = {
        template / "_nds/dsi-frontend/plugins" / (profile["id"] + ".ini"): profile_text(profile)
        for profile in catalog["profiles"]
    }
    expected.update({template / folder / ".gitkeep": "" for folder in catalog["rom_directories"]})
    for path, content in expected.items():
        if check:
            if not path.is_file() or path.read_text() != content:
                raise ValueError(f"Preset output is missing or outdated: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
    action = "Checked" if check else "Generated"
    print(
        f"{action} {len(catalog['profiles'])} profiles and {len(catalog['rom_directories'])} ROM directories"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Verify checked-in output without writing"
    )
    args = parser.parse_args()
    generate(args.check)


if __name__ == "__main__":
    main()
