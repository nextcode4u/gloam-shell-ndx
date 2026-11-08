#!/usr/bin/env python3
"""Check the reviewed public file inventory and local documentation links."""

import json
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]


def check_tree(root: Path = ROOT) -> None:
    catalog = json.loads((root / "config/package-inputs.json").read_text())
    reviewed = set(catalog["source_files"])
    if len(reviewed) != len(catalog["source_files"]):
        raise ValueError("Duplicate source inputs")
    for name in sorted(reviewed):
        path = root / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Missing or symlink public input: {name}")
        if any(part in {".git", "REF", "backups", "testing", "build", "dist", "__pycache__"}
               for part in Path(name).parts):
            raise ValueError(f"Private/generated public input: {name}")
    if (root / ".git").exists():
        names = subprocess.check_output(
            ["git", "-C", str(root), "ls-files", "--cached", "--others", "--exclude-standard", "-z"]
        ).decode().split("\0")
        actual = {n for n in names if n}
        if actual != reviewed:
            raise ValueError(f"Public file catalog differs: add {sorted(actual - reviewed)}, "
                             f"remove {sorted(reviewed - actual)}")
    template = {p.relative_to(root).as_posix() for p in (root / "sdroot").rglob("*")
                if p.is_file() and p.name != ".gitkeep"}
    if template != set(catalog["sd_files"]):
        raise ValueError("SD template differs from reviewed inputs")
    missing = []
    for name in sorted(reviewed):
        if not name.endswith(".md"):
            continue
        # Upstream README links retain their original repository context.
        if name.startswith("third_party/"):
            continue
        text = (root / name).read_text()
        text = re.sub(r"```.*?```", "", text, flags=re.S)
        targets = re.findall(r"\]\(([^)\n]+)\)", text)
        targets += re.findall(r'(?:src|href)="([^"]+)"', text)
        for target in targets:
            target = unquote(target.strip("<> ").split("#", 1)[0])
            if not target or re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target):
                continue
            path = (root / name).parent / target
            if not path.exists():
                missing.append(f"{name}: {target}")
    if missing:
        raise ValueError("Broken documentation links:\n" + "\n".join(missing))
    print(f"Public tree: {len(reviewed)} reviewed files; SD inventory and documentation links pass.")


if __name__ == "__main__":
    try:
        check_tree()
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
