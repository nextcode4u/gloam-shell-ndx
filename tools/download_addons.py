#!/usr/bin/env python3
"""Download optional Gloam applications directly from upstream onto an SD card."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import tempfile
import urllib.request
import zipfile

MAX_DOWNLOAD = 64 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def destination(root, name):
    parts = PurePosixPath(name).parts
    if (not name or name.startswith("/") or "\\" in name or ":" in name
            or any(p in {"", ".", ".."} for p in name.split("/"))):
        raise ValueError(f"Unsafe destination: {name}")
    application = name.startswith("_nds/dsi-frontend/plugins/apps/") and name.endswith((".nds", ".srl", ".ini"))
    config = re.fullmatch(r"_gba/configs/[A-Z0-9]{6}\.json", name)
    if not (application or config):
        raise ValueError(f"Destination outside add-on folders: {name}")
    path = root
    for part in parts:
        path /= part
        if path.is_symlink():
            raise ValueError(f"Destination contains a symlink: {path}")
    return path


def application_files(app):
    files = list(app["files"])
    configs = app.get("game_configs")
    if configs:
        files.extend({"download": configs["download"], "format": "zip", "member": name,
                      "destination": name, "sha256": checksum}
                     for name, checksum in configs["files"].items())
    return files


def fetch(item, cache, offline):
    name = item["asset"]
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or name in {".", ".."}:
        raise ValueError("Unsafe cache filename")
    if not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
        raise ValueError("Invalid download hash")
    if not item["url"].startswith("https://"):
        raise ValueError("Downloads require HTTPS")
    path = cache / name
    if path.is_symlink():
        raise ValueError("Download cache entry is a symlink")
    if path.exists():
        data = path.read_bytes()
    elif offline:
        raise ValueError(f"Offline cache missing: {name}")
    else:
        request = urllib.request.Request(item["url"], headers={"User-Agent": "Gloam-addon-installer"})
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read(MAX_DOWNLOAD + 1)
        if len(data) > MAX_DOWNLOAD or digest(data) != item["sha256"]:
            raise ValueError(f"Download size/hash mismatch: {name}")
        cache.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    if len(data) > MAX_DOWNLOAD or digest(data) != item["sha256"]:
        raise ValueError(f"Download size/hash mismatch: {name}")
    return data


def stage(manifest, target, cache, only=None, noncommercial=False, overwrite=False,
          offline=False, dry_run=False):
    if manifest["format"] != 1:
        raise ValueError("Unsupported download manifest")
    target = Path(target).resolve(strict=True)
    if not target.is_dir():
        raise ValueError("Target must be an existing SD root or staging directory")
    apps = {a["id"]: a for a in manifest["applications"]}
    if len(apps) != len(manifest["applications"]):
        raise ValueError("Duplicate application ID")
    wanted = list(dict.fromkeys(only)) if only else list(apps)
    selected = []
    for name in wanted:
        if name not in apps:
            raise ValueError(f"No pinned download for: {name}; use --list for manual entries")
        app = apps[name]
        if app.get("noncommercial") and not noncommercial:
            if only:
                raise ValueError(f"{name} requires --noncommercial")
            continue
        selected.append(app)
    planned, seen = [], set()
    for app in selected:
        for item in application_files(app):
            path = destination(target, item["destination"])
            key = item["destination"].casefold()
            if key in seen:
                raise ValueError("Applications have overlapping destinations")
            seen.add(key)
            planned.append((app["id"], item, path))
    fetched, installed, skipped = {}, 0, 0
    for name, item, path in planned:
        if path.exists() and not overwrite:
            print(f"Keep existing: {item['destination']}")
            skipped += 1
            continue
        if dry_run:
            print(f"Would install {name}: {item['destination']}")
            continue
        download = item["download"]
        identity = (download["asset"], download["sha256"])
        if identity not in fetched:
            fetched[identity] = fetch(download, Path(cache), offline)
        data = fetched[identity]
        if item["format"] == "zip":
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                entries = [m for m in archive.infolist() if m.filename == item["member"]]
                if len(entries) != 1 or entries[0].file_size > MAX_DOWNLOAD:
                    raise ValueError("Missing, duplicate or oversized archive member")
                data = archive.read(entries[0])
        elif item["format"] != "raw":
            raise ValueError("Unsupported download format")
        if digest(data) != item["sha256"]:
            raise ValueError(f"Member hash mismatch: {item['destination']}")
        if item["destination"].endswith(".json") and not isinstance(json.loads(data), dict):
            raise ValueError("Game config must be a JSON object")
        destination(target, item["destination"])
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as output:
                temporary = Path(output.name)
                output.write(data)
            if path.exists() and not overwrite:
                skipped += 1
                continue
            os.replace(temporary, path)
            installed += 1
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    print(f"Installed {installed} files; preserved {skipped} existing files.")
    return installed, skipped


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", type=Path, help="Existing SD root or staging directory")
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("downloads.json"))
    parser.add_argument("--cache", type=Path, default=Path.home() / ".gloam-addons-cache")
    parser.add_argument("--only", nargs="+")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--noncommercial", action="store_true")
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        if args.list:
            for app in manifest["applications"]:
                suffix = "; needs --noncommercial" if app.get("noncommercial") else ""
                print(f"{app['id']}: {app['version']}{suffix}; {app['terms']}")
            for app in manifest.get("manual", []):
                print(f"{app['id']}: manual setup; {app['reason']}; {app['url']}")
            return
        if args.target is None:
            parser.error("--target is required unless using --list")
        stage(manifest, args.target, args.cache, args.only, args.noncommercial,
              args.overwrite, args.offline, args.dry_run)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Installation stopped: {error}\n")


if __name__ == "__main__":
    main()
