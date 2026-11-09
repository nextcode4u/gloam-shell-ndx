#!/usr/bin/env python3
"""Build the reviewed emulator/player overlay and matching release companions."""

import argparse
import io
import json
import re
import subprocess
import tarfile
import zipfile
from pathlib import Path
from urllib.parse import urlparse

if __package__:
    from .package_development import json_bytes, read_input, relative_path, sha256
else:
    from package_development import json_bytes, read_input, relative_path, sha256

ROOT = Path(__file__).resolve().parents[1]
LOCK = "config/addon-dependencies.json"
MAX_MEMBER = 64 * 1024 * 1024
MAX_ARCHIVE = 256 * 1024 * 1024


def selected_applications(lock: dict, noncommercial: bool = False,
                          only: list[str] | None = None) -> list[dict]:
    """Never let an explicit selection bypass the recorded distribution decision."""
    if lock["format"] != 1:
        raise ValueError("Unsupported add-on lock format")
    if any(not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", app["id"])
           for app in lock["applications"]):
        raise ValueError("Unsafe add-on ID")
    applications = {app["id"]: app for app in lock["applications"]}
    if len(applications) != len(lock["applications"]):
        raise ValueError("Duplicate add-on ID")
    allowed = {"approved"}
    if noncommercial:
        allowed.add("noncommercial")
    if only:
        for name in only:
            if name not in applications:
                raise ValueError(f"Unknown add-on: {name}")
            app = applications[name]
            if app["redistribution"] not in allowed or app.get("permission_confirmed") is False:
                raise ValueError(f"Add-on not eligible: {name}; see docs/guides/ADDONS.md")
            if "package" not in app:
                raise ValueError(f"No pinned binary package available: {name}; see docs/guides/ADDONS.md")
        selected = [applications[name] for name in dict.fromkeys(only)]
    else:
        selected = [app for app in applications.values()
                    if app["redistribution"] in allowed and "package" in app
                    and app.get("permission_confirmed") is not False]
    # The shim is useful as an add-on only with its reviewed matching payload.
    if any(app["id"] == "snemulds-shim" for app in selected):
        if "snemulds" not in {app["id"] for app in selected}:
            payload = applications.get("snemulds", {})
            if (payload.get("redistribution") not in allowed or "package" not in payload
                    or payload.get("permission_confirmed") is False):
                raise ValueError("No eligible SNEmulDS payload for the selected shim")
            selected.append(payload)
    if not selected:
        raise ValueError("No eligible add-ons selected")
    return sorted(selected, key=lambda app: app["id"])


def download_manifest(lock: dict, bundled: set[str]) -> dict:
    """Publish download instructions, never the excluded applications' bytes."""
    if __package__:
        from .download_addons import CATALOG_URL, reviewed_apps
    else:
        from download_addons import CATALOG_URL, reviewed_apps
    updates = reviewed_apps(lock)
    ready = {app["id"] for app in updates}
    manual = [{"id": app["id"], "url": app.get("release") or app["repository"],
               "reason": app["distribution_notes"]}
              for app in lock["applications"] if app["id"] not in ready]
    return {"format": 1, "reviewed": lock["reviewed"],
            "applications": [a for a in updates if a["id"] not in bundled],
            "updates": updates, "catalog_url": CATALOG_URL, "manual": manual}


def archive_members(data: bytes, kind: str) -> dict[str, bytes]:
    """Read bounded regular members without extracting paths onto the host."""
    result = {}
    seen = set()
    total = 0

    def accept(name: str, size: int) -> None:
        nonlocal total
        relative_path(name)
        if name.casefold() in seen:
            raise ValueError("Duplicate/case-colliding add-on archive member")
        seen.add(name.casefold())
        total += size
        if size > MAX_MEMBER or total > MAX_ARCHIVE or len(seen) > 30000:
            raise ValueError("Oversized add-on archive")

    if kind == "tar.gz":
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            for member in archive:
                if member.isdir():
                    relative_path(member.name.rstrip("/"))
                    continue
                if not member.isfile():
                    raise ValueError("Non-regular add-on archive member")
                accept(member.name, member.size)
                result[member.name] = archive.extractfile(member).read()
    elif kind == "zip":
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for member in archive.infolist():
                if member.is_dir():
                    relative_path(member.filename.rstrip("/"))
                    continue
                if (member.external_attr >> 28) == 0xA:
                    raise ValueError("Add-on archive symlink")
                accept(member.filename, member.file_size)
                result[member.filename] = archive.read(member)
    else:
        raise ValueError(f"Unsupported add-on archive format: {kind}")
    return result


def gba_config_files(data: bytes, hashes: dict[str, str]) -> dict[str, bytes]:
    """Keep only explicitly pinned GBARunner3 game configs, never firmware."""
    members = archive_members(data, "zip")
    result = {}
    if not hashes:
        raise ValueError("Missing GBARunner3 game config pins")
    for name, digest in hashes.items():
        if not re.fullmatch(r"_gba/configs/[A-Z0-9]{6}\.json", name):
            raise ValueError("Invalid GBARunner3 game config path")
        content = members[name]
        if sha256(content) != digest:
            raise ValueError(f"GBARunner3 game config hash mismatch: {name}")
        if not isinstance(json.loads(content), dict):
            raise ValueError("GBARunner3 game config must be a JSON object")
        result[name] = content
    return result


def addon_files(root: Path, offline: bool = False, noncommercial: bool = False,
                only: list[str] | None = None) -> tuple[dict, dict, dict, dict]:
    if __package__:
        from .build_release import download
    else:
        from build_release import download

    lock = json.loads((root / LOCK).read_text())
    selected = selected_applications(lock, noncommercial, only)
    relative_path(lock["application_directory"])
    runtime, sources, notices = {}, {}, {}
    cache = root / "build/addons-cache"
    downloaded = {}

    def fetch(item: dict) -> bytes:
        name = item["asset"]
        if Path(name).name != name or not re.fullmatch(r"[A-Za-z0-9_.-]+", name):
            raise ValueError("Unsafe add-on cache filename")
        if not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
            raise ValueError("Invalid add-on SHA-256")
        if urlparse(item["url"]).scheme != "https":
            raise ValueError("Add-on downloads require HTTPS")
        identity = (name, item["sha256"])
        if identity not in downloaded:
            downloaded[identity] = download({**item, "id": name}, cache, offline).read_bytes()
        return downloaded[identity]

    for app in selected:
        print(f"Preparing add-on {app['id']} {app['version']}", flush=True)
        package = app["package"]
        source_data = fetch(package["source"])
        members = archive_members(source_data, "tar.gz")
        prefix = "dependencies/addons/" + app["id"] + "/"
        source_role = package.get("source_role", "upstream")
        if source_role not in {"upstream", "reference-only"}:
            raise ValueError("Unsupported add-on source role")
        source_name = "reference-source.tar.gz" if source_role == "reference-only" else "source.tar.gz"
        sources[prefix + source_name] = source_data
        sources[prefix + "release.json"] = json_bytes(app)
        if not package["notices"]:
            raise ValueError(f"Missing add-on notices: {app['id']}")

        def keep_notices(items: list[dict], archive: dict[str, bytes], folder: str) -> None:
            for item in items:
                relative_path(item["destination"])
                content = archive[item["member"]]
                if item.get("header_only"):
                    start = content.find(b"/*")
                    end = content.find(b"*/", start)
                    if start < 0 or end < 0:
                        raise ValueError("Missing firmware license comment")
                    content = content[start:end + 2] + b"\n"
                if not content.strip():
                    raise ValueError("Empty add-on notice")
                target = f"addons/{app['id']}/{folder}{item['destination']}"
                if target in notices:
                    raise ValueError("Overlapping add-on notices")
                notices[target] = content

        keep_notices(package["notices"], members, "")
        for extra in package.get("extra_sources", []):
            relative_path(extra["path"])
            blob = fetch(extra["download"])
            target = prefix + extra["path"]
            if not extra["path"].startswith("submodules/") or target in sources:
                raise ValueError("Overlapping or misplaced add-on source archive")
            sources[target] = blob
            extra_members = archive_members(blob, "tar.gz")
            keep_notices(extra.get("notices", []), extra_members,
                         extra["path"].removesuffix(".tar.gz") + "/")
        for item in package.get("runtime_notices", []):
            archive = archive_members(fetch(item["download"]), item["format"])
            keep_notices([item], archive, "")
        for item in package.get("nested_notices", []):
            relative_path(item["destination"])
            nested = archive_members(members[item["archive_member"]], "tar.gz")
            notices[f"addons/{app['id']}/{item['destination']}"] = nested[item["member"]]
        for item in package.get("project_notices", []):
            relative_path(item["destination"])
            target = f"addons/{app['id']}/{item['destination']}"
            if target in notices:
                raise ValueError("Overlapping add-on notices")
            content = read_input(root, item["path"])
            if not content.strip():
                raise ValueError("Empty add-on notice")
            notices[target] = content
        for item in package["files"]:
            target = item["destination"]
            relative_path(target)
            if (not target.startswith(lock["application_directory"] + "/")
                    or Path(target).suffix.lower() not in {".nds", ".srl", ".ini"}):
                raise ValueError("Add-on executable outside configured application folder")
            if target.casefold() in {name.casefold() for name in runtime}:
                raise ValueError("Add-ons install overlapping paths")
            data = fetch(item["download"])
            if item["format"] != "raw":
                data = archive_members(data, item["format"])[item["member"]]
            if sha256(data) != item["sha256"]:
                raise ValueError(f"Add-on member hash mismatch: {target}")
            runtime[target] = data
        if "game_configs" in package:
            if app["id"] != "gbarunner3":
                raise ValueError("GBARunner3 game configs assigned to another application")
            configs = package["game_configs"]
            for target, content in gba_config_files(fetch(configs["download"]), configs["files"]).items():
                if target.casefold() in {name.casefold() for name in runtime}:
                    raise ValueError("Add-ons install overlapping paths")
                runtime[target] = content
    for notice in lock.get("common_notices", []):
        notices["addons/common/" + notice["asset"]] = fetch(notice)
    downloads = {"applications": []}
    if lock.get("download_installer"):
        downloads = download_manifest(lock, {a["id"] for a in selected})
        runtime["addon-installer/downloads.json"] = json_bytes(downloads)
        for output, source in lock["download_installer"].items():
            relative_path(output)
            runtime["addon-installer/" + output] = read_input(root, source)
    report = {"format": 1, "reviewed": lock["reviewed"], "applications": selected,
              "noncommercial": any(a["redistribution"] == "noncommercial" for a in selected),
              "download_only": [a["id"] for a in downloads["applications"]],
              "runtime_files": {n: sha256(b) for n, b in sorted(runtime.items())}}
    notices["addons/README.txt"] = (
        "Gloam optional add-ons: keep this licenses.zip and the combined source.zip\n"
        "with addons.zip when redistributing. Each app retains its own terms;\n"
        "Gloam's GPL does not replace them. Exact versions, sources and hashes\n"
        "are recorded in source.zip/dependencies/addons/manifest.json.\n"
        "Applications with unconfirmed permission are not bundled. Use the shared\n"
        "Python or PowerShell downloader in addon-installer for direct upstream downloads.\n"
        + ("This selection includes noncommercial apps: no sale/profit.\n"
           if report["noncommercial"] else "This selection excludes the noncommercial-only apps.\n")
    ).encode()
    sources["dependencies/addons/manifest.json"] = json_bytes(report)
    return runtime, sources, notices, report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--noncommercial", action="store_true",
                        help="Also include reviewed packages restricted to no-profit distribution")
    parser.add_argument("--only", nargs="+", help="Select eligible add-on IDs")
    parser.add_argument("--list", action="store_true", help="Show all audit entries without downloading")
    args = parser.parse_args()
    if args.list:
        lock = json.loads((ROOT / LOCK).read_text())
        for app in lock["applications"]:
            eligible = (app["redistribution"] in {"approved", "noncommercial"}
                        and app.get("permission_confirmed") is not False)
            availability = ("bundle eligible" if eligible else "direct download") if "package" in app else "manual; no pinned package"
            print(f"{app['id']}: argv={app['argv']}; {app['redistribution']}; {availability}")
        return
    try:
        selected_applications(json.loads((ROOT / LOCK).read_text()), args.noncommercial, args.only)
        if __package__:
            from .build_release import build_release
        else:
            from build_release import build_release
        if not args.skip_build:
            subprocess.run(["make", "-j4"], cwd=ROOT, check=True)
            subprocess.run(["python3", "tools/write_build_info.py"], cwd=ROOT, check=True)
        build_release(ROOT, args.offline, addons=True, noncommercial=args.noncommercial,
                      only_addons=args.only)
    except (OSError, ValueError, KeyError, tarfile.TarError, zipfile.BadZipFile,
            subprocess.CalledProcessError) as error:
        parser.exit(1, f"Add-on package not created: {error}\n")


if __name__ == "__main__":
    main()
