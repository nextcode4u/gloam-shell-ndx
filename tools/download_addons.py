#!/usr/bin/env python3
"""Download optional Gloam applications directly from upstream onto an SD card."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

MAX_DOWNLOAD = 64 * 1024 * 1024
CATALOG_URL = "https://raw.githubusercontent.com/nextcode4u/gloam-shell-ndx/main/config/addon-dependencies.json"


READER_FONTS = frozenset("data/dslibris/font/" + name for name in (
    "LiberationSerif-Regular.ttf", "LiberationSerif-Bold.ttf", "LiberationSerif-Italic.ttf",
    "LiberationSerif-BoldItalic.ttf", "LiberationSans-Regular.ttf"))


ADDON_PROFILES = frozenset("_nds/dsi-frontend/plugins/" + name for name in (
    "dslibris-books.ini", "dslibris-documents.ini", "picodrivetwl.ini"))


def runtime_path_allowed(name):
    application = name.startswith("_nds/dsi-frontend/plugins/apps/") and name.endswith((".nds", ".srl", ".ini"))
    config = re.fullmatch(r"_gba/configs/[A-Z0-9]{6}\.json", name)
    return bool(application or config or name in READER_FONTS or name in ADDON_PROFILES
                or name in {"roms/ScummVM/scummvm.nds", "roms/bios/gbc_bios.bin"})


def reviewed_apps(catalog):
    """Convert reviewed build recipes to portable runtime-only instructions."""
    if catalog["format"] != 1:
        raise ValueError("Unsupported update catalog")
    if "updates" in catalog:
        return catalog["updates"]
    result = []
    for app in catalog["applications"]:
        package = app.get("package", {})
        files = package.get("files", [])
        if not files or any(f["format"] not in {"raw", "zip", "tar.gz"} for f in files):
            continue
        defaults = [{"format": "inline", "content": f["content"], "sha256": f["sha256"],
                     "destination": f["destination"]} for f in package.get("defaults", [])]
        item = {"id": app["id"], "version": app["version"], "repository": app["repository"],
                "terms": app["license"], "noncommercial": app["redistribution"] == "noncommercial"
                or app.get("download_noncommercial", False), "files": [*files, *defaults]}
        if app.get("experimental"):
            item.update(experimental=True, notes=app.get("compatibility_notes", "Experimental build"))
        if "game_configs" in package:
            item["game_configs"] = package["game_configs"]
        if app["id"] in {"snemulds", "snemulds-shim"}:
            item["requires"] = ["snemulds", "snemulds-shim"]
        release = app.get("release") or ""
        if "/releases/tag/" in release:
            item["release_tag"] = release.split("/releases/tag/", 1)[1]
        result.append(item)
    return result


def read_remote_json(url):
    if not url.startswith("https://"):
        raise ValueError("Update catalogs require HTTPS")
    request = urllib.request.Request(url, headers={"User-Agent": "Gloam-addon-installer"})
    with urllib.request.urlopen(request, timeout=30) as response:
        data = response.read(2 * 1024 * 1024 + 1)
    if len(data) > 2 * 1024 * 1024:
        raise ValueError("Oversized update response")
    return json.loads(data)


def update_manifest(manifest, catalog=None, offline=False):
    if catalog is not None:
        apps = reviewed_apps(json.loads(Path(catalog).read_text(encoding="utf-8")))
    elif offline:
        apps = manifest.get("updates", manifest["applications"])
    else:
        try:
            remote = read_remote_json(manifest.get("catalog_url", CATALOG_URL))
            apps = reviewed_apps(remote)
            if remote.get("reviewed", "") < manifest.get("reviewed", ""):
                print("Remote catalog is older than this package; keeping packaged reviewed pins.")
                apps = manifest.get("updates", manifest["applications"])
        except OSError as error:
            print(f"Remote catalog unavailable ({error}); using packaged reviewed pins.")
            apps = manifest.get("updates", manifest["applications"])
    return {"format": 1, "applications": apps, "manual": manifest.get("manual", [])}


def select_apps(manifest, only, noncommercial):
    apps = {a["id"]: a for a in manifest["applications"]}
    if len(apps) != len(manifest["applications"]):
        raise ValueError("Duplicate application ID")
    wanted = list(dict.fromkeys(only)) if only else list(apps)
    selected = []
    for name in wanted:
        if name not in apps:
            raise ValueError(f"No pinned download for: {name}; use --list for manual entries")
        app = apps[name]
        if app.get("experimental") and not only:
            continue
        if app.get("noncommercial") and not noncommercial:
            if only:
                raise ValueError(f"{name} requires --noncommercial")
            continue
        selected.append(app)
        for required in app.get("requires", []):
            if required not in apps:
                raise ValueError(f"Missing paired application: {required}")
            if required not in wanted:
                wanted.append(required)
    return selected


def check_updates(manifest, target, only=None, noncommercial=False, upstream=False):
    root = Path(target).resolve(strict=True)
    if not root.is_dir():
        raise ValueError("Target must be an existing SD root or staging directory")
    for app in select_apps(manifest, only, noncommercial):
        files = [f for f in app["files"] if f["destination"].endswith((".nds", ".srl"))]
        paths = [destination(root, f["destination"]) for f in files]
        status = "not installed"
        if any(p.exists() for p in paths):
            current = all(p.is_file() and digest(p.read_bytes()) == f["sha256"]
                          for p, f in zip(paths, files))
            status = "up to date" if current else "differs from reviewed executable"
        support = [f for f in app["files"] if not f["destination"].endswith((".nds", ".srl"))]
        if status == "up to date" and any(not destination(root, f["destination"]).is_file() for f in support):
            status = "missing support files"
        print(f"{app['id']}: {status}; reviewed {app['version']}")
        if upstream and app.get("release_tag"):
            repository = app["repository"].removeprefix("https://github.com/").removesuffix(".git")
            if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
                raise ValueError("Invalid upstream repository")
            try:
                release = read_remote_json(f"https://api.github.com/repos/{repository}/releases/latest")
                if release["tag_name"] != app["release_tag"]:
                    print(f"  Other upstream release: {release['tag_name']} (not selected; review required).")
            except OSError as error:
                print(f"  Upstream check unavailable: {error}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def destination(root, name):
    parts = PurePosixPath(name).parts
    if (not name or name.startswith("/") or "\\" in name or ":" in name
            or any(p in {"", ".", ".."} for p in name.split("/"))):
        raise ValueError(f"Unsafe destination: {name}")
    if not runtime_path_allowed(name):
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
    path = cache / (item["sha256"] + "-" + name)
    legacy = cache / name
    if not path.exists() and legacy.exists():
        if legacy.is_symlink():
            raise ValueError("Download cache entry is a symlink")
        if digest(legacy.read_bytes()) == item["sha256"]:
            path = legacy
        elif offline:
            raise ValueError(f"Download cache hash mismatch: {name}")
    if path.is_symlink():
        raise ValueError("Download cache entry is a symlink")
    if path.exists():
        data = path.read_bytes()
    elif offline:
        raise ValueError(f"Offline cache missing: {name}")
    else:
        print(f"Downloading {item['asset']}...")
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
          offline=False, dry_run=False, update=False):
    if manifest["format"] != 1:
        raise ValueError("Unsupported download manifest")
    target = Path(target).resolve(strict=True)
    if not target.is_dir():
        raise ValueError("Target must be an existing SD root or staging directory")
    selected = select_apps(manifest, only, noncommercial)
    if update and not only:
        installed = {app["id"] for app in selected if any(
            destination(target, f["destination"]).is_file() for f in app["files"]
            if f["destination"].endswith((".nds", ".srl")))}
        selected = select_apps(manifest, list(installed), noncommercial) if installed else []
    planned, seen = [], set()
    for app in selected:
        if app.get("experimental"):
            print(f"Experimental {app['id']}: {app.get('notes', 'Experimental build')}")
        for item in application_files(app):
            path = destination(target, item["destination"])
            key = item["destination"].casefold()
            if key in seen:
                raise ValueError("Applications have overlapping destinations")
            seen.add(key)
            planned.append((app["id"], item, path))
    fetched, installed, skipped = {}, 0, 0
    for name, item, path in planned:
        replace = overwrite or (update and path.suffix.lower() in {".nds", ".srl"})
        if path.is_file() and update and digest(path.read_bytes()) == item["sha256"]:
            skipped += 1
            continue
        if path.exists() and not replace:
            print(f"Keep existing: {item['destination']}")
            skipped += 1
            continue
        if dry_run:
            print(f"Would install {name}: {item['destination']}")
            continue
        inline = item["format"] == "inline"
        if inline:
            if item["destination"] not in ADDON_PROFILES or len(item["content"].encode("utf-8")) > 4096:
                raise ValueError("Unsupported inline add-on default")
            data = item["content"].encode("utf-8")
        else:
            download = item["download"]
        identity = ("inline", item["sha256"]) if inline else (download["asset"], download["sha256"])
        if not inline:
            if identity not in fetched:
                fetched[identity] = fetch(download, Path(cache), offline)
            data = fetched[identity]
        if item["format"] == "zip":
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                entries = [m for m in archive.infolist() if m.filename == item["member"]]
                if len(entries) != 1 or entries[0].file_size > MAX_DOWNLOAD:
                    raise ValueError("Missing, duplicate or oversized archive member")
                data = archive.read(entries[0])
        elif item["format"] == "tar.gz":
            found, total = [], 0
            with tarfile.open(fileobj=io.BytesIO(data), mode="r|gz") as archive:
                for member in archive:
                    total += member.size
                    if total > 256 * 1024 * 1024:
                        raise ValueError("Oversized tar archive")
                    if member.name == item["member"]:
                        if not member.isfile() or member.size > MAX_DOWNLOAD:
                            raise ValueError("Non-regular or oversized archive member")
                        found.append(archive.extractfile(member).read())
            if len(found) != 1:
                raise ValueError("Missing or duplicate archive member")
            data = found[0]
        elif item["format"] not in {"raw", "inline"}:
            raise ValueError("Unsupported download format")
        if digest(data) != item["sha256"]:
            raise ValueError(f"Member hash mismatch: {item['destination']}")
        if item["destination"].endswith(".json") and not isinstance(json.loads(data), dict):
            raise ValueError("Game config must be a JSON object")
        destination(target, item["destination"])
        if update and path.is_file():
            previous = path.read_bytes()
            backup = Path(cache) / "previous-files" / digest(previous) / path.name
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                backup.write_bytes(previous)
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as output:
                temporary = Path(output.name)
                output.write(data)
            if path.exists() and not replace:
                skipped += 1
                continue
            os.replace(temporary, path)
            installed += 1
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    print(f"Installed {installed} files; preserved {skipped} existing files.")
    return installed, skipped


def removable_drives():
    """Suggest ready removable Windows drives; other targets can be entered."""
    if os.name != "nt":
        return []
    import ctypes
    drive_type = ctypes.windll.kernel32.GetDriveTypeW
    drive_type.argtypes = [ctypes.c_wchar_p]
    drive_type.restype = ctypes.c_uint
    drives = []
    for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        path = Path(letter + ":/")
        if drive_type(path.anchor) == 2 and path.is_dir():
            try:
                label = f"{shutil.disk_usage(path).free / 1024**3:.1f} GB free"
            except OSError:
                label = "removable drive"
            drives.append((path, label))
    return drives


def choose_target():
    drives = removable_drives()
    print("\nChoose the SD card root or an existing staging folder.")
    for number, (path, label) in enumerate(drives, 1):
        print(f"  {number}. {path} ({label})")
    while True:
        answer = input("Drive number or folder path (blank to go back): ").strip().strip('"')
        if not answer:
            return None
        if answer.isdigit():
            if not 1 <= int(answer) <= len(drives):
                print("Choose a listed drive number or enter a folder path.")
                continue
            path = drives[int(answer) - 1][0]
        else:
            if os.name == "nt" and re.fullmatch(r"[A-Za-z]:", answer):
                answer += "/"
            path = Path(answer).expanduser()
        try:
            path = path.resolve(strict=True)
            if path.is_dir():
                return path
        except OSError:
            pass
        print("That folder is unavailable. Choose an existing SD root or staging folder.")


def installed_apps(apps, target):
    return [app for app in apps if any(
        destination(target, item["destination"]).is_file() for item in app["files"]
        if item["destination"].endswith((".nds", ".srl")))]


def show_applications(apps, target=None):
    installed = {a["id"] for a in installed_apps(apps, target)} if target else set()
    for number, app in enumerate(apps, 1):
        labels = []
        if app["id"] in installed:
            labels.append("installed")
        if app.get("noncommercial"):
            labels.append("noncommercial")
        if app.get("experimental"):
            labels.append("experimental")
        suffix = " [" + ", ".join(labels) + "]" if labels else ""
        print(f"  {number:2}. {app['id']}  {app['version']}{suffix}")


def parse_selection(answer, apps):
    if answer.lower() == "all":
        return [a["id"] for a in apps if not a.get("noncommercial") and not a.get("experimental")]
    names = {a["id"] for a in apps}
    selected = []
    for token in re.split(r"[,\s]+", answer.strip()):
        if token.isdigit() and 1 <= int(token) <= len(apps):
            token = apps[int(token) - 1]["id"]
        if token not in names:
            raise ValueError("Choose listed numbers or application IDs, separated by spaces or commas.")
        if token not in selected:
            selected.append(token)
    return selected


def choose_applications(apps, target):
    show_applications(apps, target)
    print("Enter numbers or IDs. 'all' selects standard apps, excluding noncommercial/experimental entries.")
    while True:
        answer = input("Applications (blank or 0 to go back): ").strip()
        if answer in {"", "0"}:
            return None
        try:
            return parse_selection(answer, apps)
        except ValueError as error:
            print(error)


def interactive_menu(manifest, args):
    """Collect choices, then use the same checked install/update paths as the CLI."""
    if manifest["format"] != 1:
        raise ValueError("Unsupported download manifest")
    target = args.target.expanduser().resolve(strict=True) if args.target else None
    while True:
        print("\nGloam Add-on Installer\n======================")
        print(f"Target: {target or 'not selected'}")
        print("1. Install applications\n2. Check installed apps for updates\n3. Update installed apps")
        print("4. Choose SD card or staging folder\n5. Show applications\n0. Exit")
        action = input("Choose: ").strip()
        if action == "0":
            return
        if action == "4":
            target = choose_target() or target
            continue
        if action == "5":
            show_applications(sorted(manifest.get("updates", manifest["applications"]), key=lambda a: a["id"]), target)
            input("Press Enter to return to the menu...")
            continue
        if action not in {"1", "2", "3"}:
            print("Choose a menu number.")
            continue
        if target is None:
            target = choose_target()
            if target is None:
                continue
        try:
            print("Reading reviewed application versions...")
            current = update_manifest(manifest, args.catalog, args.offline)
            apps = sorted(current["applications"], key=lambda a: a["id"])
            if action in {"2", "3"}:
                apps = installed_apps(apps, target)
            if not apps:
                print("No matching applications are installed." if action != "1" else "No downloads are available.")
            elif action == "2":
                check_updates(current, target, [a["id"] for a in apps], noncommercial=True)
            else:
                names = choose_applications(apps, target)
                if names:
                    selected = select_apps(current, names, noncommercial=True)
                    print(f"\nTarget: {target}")
                    for app in selected:
                        print(f"  {app['id']} {app['version']}: {app['terms']}")
                        if app.get("experimental"):
                            print("  Experimental: " + app.get("notes", "Experimental build"))
                    allowed = args.noncommercial
                    if any(a.get("noncommercial") for a in selected) and not allowed:
                        allowed = input("Use selected noncommercial apps within their stated terms? [y/N]: ").strip().lower() in {"y", "yes"}
                        if not allowed:
                            print("Cancelled.")
                            input("Press Enter to return to the menu...")
                            continue
                    operation = "Update" if action == "3" else "Install"
                    if input(f"{operation} these applications? [y/N]: ").strip().lower() in {"y", "yes"}:
                        stage(current, target, args.cache, [a["id"] for a in selected], allowed,
                              args.overwrite, args.offline, args.dry_run, action == "3")
                    else:
                        print("Cancelled.")
        except (OSError, ValueError, KeyError, zipfile.BadZipFile, tarfile.TarError) as error:
            print(f"Operation stopped: {error}")
        input("Press Enter to return to the menu...")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interactive", action="store_true", help="Open the interactive installer menu")
    parser.add_argument("--target", type=Path, help="Existing SD root or staging directory")
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("downloads.json"))
    parser.add_argument("--cache", type=Path, default=Path.home() / ".gloam-addons-cache")
    parser.add_argument("--only", nargs="+")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--noncommercial", action="store_true")
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--check-updates", action="store_true", help="Compare installed apps with current reviewed pins")
    parser.add_argument("--update", action="store_true", help="Stage reviewed updates, backing up replaced files")
    parser.add_argument("--catalog", type=Path, help="Use a local reviewed catalog instead of refreshing online")
    parser.add_argument("--upstream", action="store_true", help="Also report other upstream release tags; never auto-install them")
    args = parser.parse_args()
    menu_mode = args.interactive or len(sys.argv) == 1
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        if menu_mode:
            interactive_menu(manifest, args)
            return
        if args.list:
            for app in manifest["applications"]:
                suffix = "; needs --noncommercial" if app.get("noncommercial") else ""
                if app.get("experimental"):
                    suffix += f"; experimental, select with --only {app['id']}"
                print(f"{app['id']}: {app['version']}{suffix}; {app['terms']}")
            for app in manifest.get("manual", []):
                print(f"{app['id']}: manual setup; {app['reason']}; {app['url']}")
            return
        if args.target is None:
            parser.error("--target is required unless using --list")
        if args.check_updates and args.update:
            parser.error("choose --check-updates or --update, not both")
        if args.upstream and (not args.check_updates or args.offline):
            parser.error("--upstream requires --check-updates and online access")
        if args.check_updates or args.update:
            manifest = update_manifest(manifest, args.catalog, args.offline)
        if args.check_updates:
            check_updates(manifest, args.target, args.only, args.noncommercial, args.upstream)
            return
        stage(manifest, args.target, args.cache, args.only, args.noncommercial,
              args.overwrite, args.offline, args.dry_run, args.update)
    except (EOFError, KeyboardInterrupt):
        print("\nInstaller closed.")
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, tarfile.TarError) as error:
        if menu_mode:
            print(f"Installation stopped: {error}")
            try:
                input("Press Enter to close...")
            except (EOFError, KeyboardInterrupt):
                pass
            raise SystemExit(1)
        parser.exit(1, f"Installation stopped: {error}\n")


if __name__ == "__main__":
    main()
