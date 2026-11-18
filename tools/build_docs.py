#!/usr/bin/env python3
"""Stage reviewed guides for Zensical, preserving repository Markdown links."""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "https://github.com/nextcode4u/gloam-shell-ndx"


def rewrite_links(text: str, source: Path, root: Path) -> str:
    """Leave guide links local and send source-code links to GitHub."""
    def target(value: str) -> str:
        parts = urlsplit(value.strip("<>"))
        if parts.scheme or parts.netloc or not parts.path:
            return value
        resolved = (source.parent / unquote(parts.path)).resolve()
        try:
            relative = resolved.relative_to(root.resolve())
        except ValueError:
            raise ValueError(f"Link escapes repository in {source}: {value}") from None
        if relative.parts and relative.parts[0] == "docs":
            return value
        if not resolved.exists():
            raise ValueError(f"Missing repository link in {source}: {value}")
        kind = "tree" if resolved.is_dir() else "blob"
        suffix = (f"?{parts.query}" if parts.query else "") + (f"#{parts.fragment}" if parts.fragment else "")
        return f"{REPOSITORY}/{kind}/main/{quote(relative.as_posix(), safe='/')}{suffix}"

    # Fenced examples are literal instructions, not navigation links.
    pieces = re.split(r"(```.*?```|~~~.*?~~~)", text, flags=re.S)
    for i in range(0, len(pieces), 2):
        piece = re.sub(r"(\]\()([^\s)]+)(\))",
                       lambda m: m[1] + target(m[2]) + m[3], pieces[i])
        piece = re.sub(r'((?:href|src)=")([^"]+)(")',
                       lambda m: m[1] + target(m[2]) + m[3], piece)
        pieces[i] = re.sub(r"(?m)^(\s*\[[^\]]+\]:\s*)(\S+)",
                           lambda m: m[1] + target(m[2]), piece)
    return "".join(pieces)


def stage(root: Path = ROOT) -> Path:
    """Copy only catalogued docs; never publish a working-tree directory scan."""
    root = root.resolve()
    destination = root / "build/docs-source"
    catalog = json.loads((root / "config/package-inputs.json").read_text())
    entries = [Path(name) for name in catalog["source_files"] if name.startswith("docs/")]
    # Validate all inputs before replacing the previous staged documentation.
    for name in entries + [Path("forwarder/icon-source.png")]:
        source = root / name
        if (not source.is_file() or source.is_symlink() or
                not source.resolve().is_relative_to(root)):
            raise ValueError(f"Unsafe or missing documentation input: {name}")
    if destination.is_symlink():
        raise ValueError("Documentation staging directory must not be a symlink")
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name in entries:
        source = root / name
        output = destination / name.relative_to("docs")
        output.parent.mkdir(parents=True, exist_ok=True)
        if source.suffix == ".md":
            output.write_text(rewrite_links(source.read_text(encoding="utf-8"), source, root), encoding="utf-8")
        else:
            shutil.copyfile(source, output)
    logo = destination / "images/gloam.png"
    logo.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / "forwarder/icon-source.png", logo)
    return destination


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serve", action="store_true", help="Preview at the local address printed by Zensical")
    args = parser.parse_args()
    stage()
    command = [sys.executable, "-m", "zensical"]
    command += ["serve"] if args.serve else ["build", "--clean", "--strict"]
    subprocess.run(command, cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
