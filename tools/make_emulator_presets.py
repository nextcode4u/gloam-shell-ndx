#!/usr/bin/env python3
"""Generate emulator profiles and empty system folders from the default catalog."""

import argparse
import json
import textwrap
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


def collection_readme(folder: str, catalog: dict, addons: dict) -> str:
    """Keep formats, actual argv behavior and BIOS setup beside each collection."""
    collection = next((c for c in catalog["collections"] if c["folder"] == folder), None)
    applications = {a["id"]: a for a in addons["applications"]}
    lines = [folder.removeprefix("roms/") if folder != "roms" else "Game collections",
             "=" * 40, "", "SD folder: /" + folder + "/", ""]

    def paragraph(text: str) -> None:
        lines.extend(textwrap.wrap(text, width=78))
        lines.append("")

    if folder == "roms/bios":
        paragraph("Support firmware only, not a game collection. Use each emulator's exact "
                  "filename and lookup path. Original commercial BIOS files are not supplied. "
                  "GBA firmware belongs at /_gba/bios.bin, not in this folder.")
        for app in addons["applications"]:
            if app["id"] in {c.get("addon") for c in catalog["collections"]}:
                paragraph(app["id"] + ": " + app["bios"])
    elif collection and collection["formats"]:
        paragraph("Content: " + collection["formats"])
        paragraph("Application: " + collection["application"])
        app = applications.get(collection.get("addon"))
        if app:
            paragraph("Argv in the audited upstream build: " + app["argv"] + ". "
                      + app.get("argv_notes", ""))
            paragraph("Firmware: " + app["bios"])
            paragraph("Add-on package: " + ("available" if "package" in app else "manual / held")
                      + "; terms: " + app["license"] + ".")
            lines.extend(["Upstream: " + app["repository"], "Release: "
                          + (app["release"] or app["repository"] + "/releases"), ""])
        if collection.get("notes"):
            paragraph(collection["notes"])
        paragraph("Keep required companion files and saves together. Other emulator-supported "
                  "formats may need its own browser or a per-file application choice. "
                  "Changing an extension is not a file-format conversion.")
    else:
        children = sorted({str(Path(p).relative_to(folder)).split("/")[0]
                           for p in catalog["rom_directories"]
                           if p.startswith(folder + "/")})
        paragraph("Organize content in the system folders below. Each folder has its own "
                  "README.txt with configured formats and application/firmware notes.")
        lines.extend("- " + child for child in children)
        lines.append("")
    lines.extend(["Full setup and compatibility audit:",
                  "https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md", ""])
    return "\n".join(lines)


def generate(check: bool = False) -> None:
    catalog = json.loads(CATALOG.read_text())
    if catalog["version"] != 1:
        raise ValueError("Unsupported emulator catalog version")
    template = PROJECT_ROOT / "sdroot"
    addons = json.loads((PROJECT_ROOT / "config/addon-dependencies.json").read_text())
    expected = {
        template / "_nds/dsi-frontend/plugins" / (profile["id"] + ".ini"): profile_text(profile)
        for profile in catalog["profiles"]
    }
    expected.update({template / folder / ".gitkeep": "" for folder in catalog["rom_directories"]})
    folders = {"roms", *catalog["rom_directories"]}
    expected.update({template / folder / "README.txt": collection_readme(folder, catalog, addons)
                     for folder in folders})
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
