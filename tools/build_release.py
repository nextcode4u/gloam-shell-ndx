#!/usr/bin/env python3
"""Build an SD package with pinned upstream backends and matching source archives."""

import argparse
import json
import os
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path

if __package__:
    from .package_development import json_bytes, package, relative_path, sha256, write_archive
else:
    from package_development import json_bytes, package, relative_path, sha256, write_archive

ROOT = Path(__file__).resolve().parents[1]
LOCK = "config/release-dependencies.json"
MAX_DOWNLOAD = 64 * 1024 * 1024


def git(directory: Path, *arguments: str) -> str:
    return subprocess.check_output(["git", "-C", str(directory), *arguments], text=True).strip()


def download(dependency: dict, cache: Path, offline: bool) -> Path:
    """Verify cached or downloaded bytes before parsing any archive members."""
    cache.mkdir(parents=True, exist_ok=True)
    destination = cache / dependency["asset"]
    expected = dependency["sha256"]
    if destination.exists():
        if destination.is_symlink() or sha256(destination.read_bytes()) != expected:
            raise ValueError(f"Dependency cache hash mismatch: {destination}")
        return destination
    if offline:
        raise ValueError(f"Offline cache missing: {destination}")
    request = urllib.request.Request(dependency["url"], headers={"User-Agent": "Gloam-build"})
    with urllib.request.urlopen(request, timeout=60) as response:
        data = response.read(MAX_DOWNLOAD + 1)
    if len(data) > MAX_DOWNLOAD or sha256(data) != expected:
        raise ValueError(f"Downloaded dependency hash mismatch: {dependency['id']}")
    destination.write_bytes(data)
    return destination


def release_files(archive: Path, dependency: dict) -> dict[str, bytes]:
    """Install only reviewed members, never extract an upstream ZIP to disk."""
    result = {}
    with zipfile.ZipFile(archive) as source:
        seen = set()
        for info in source.infolist():
            relative_path(info.filename.rstrip("/"))
            name = info.filename.casefold()
            if name in seen:
                raise ValueError("Duplicate dependency archive member")
            seen.add(name)
            if info.file_size > MAX_DOWNLOAD or (info.external_attr >> 28) == 0xA:
                raise ValueError("Oversized or symlink dependency archive member")
        for item in dependency["files"]:
            relative_path(item["member"])
            relative_path(item["destination"])
            data = source.read(item["member"])
            if sha256(data) != item["sha256"]:
                raise ValueError(f"Installed member hash mismatch: {item['member']}")
            if item["destination"] in result:
                raise ValueError("Duplicate installed destination")
            result[item["destination"]] = data
    return result


def source_checkout(dependency: dict, cache: Path, offline: bool) -> Path:
    """Keep exact, unmodified upstream source independent of private references."""
    destination = cache / dependency["id"]
    if not destination.exists():
        if offline:
            raise ValueError(f"Offline source checkout missing: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["git", "clone", "--depth", "1", "--branch", dependency["tag"],
             "--recurse-submodules", "--shallow-submodules", dependency["repository"],
             str(destination)], check=True,
        )
    if destination.is_symlink() or git(destination, "rev-parse", "HEAD") != dependency["commit"]:
        raise ValueError(f"Source revision mismatch: {destination}; use a fresh cache")
    if git(destination, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError(f"Source checkout changed: {destination}; use a fresh cache")
    return destination


def export_source(directory: Path, prefix: str, modes: dict[str, int] | None = None) -> tuple[dict[str, bytes], dict[str, str]]:
    """Include every tracked file and initialized submodule at its recorded revision."""
    if modes is None:
        modes = {}
    files = {}
    revisions = {prefix: git(directory, "rev-parse", "HEAD")}
    records = subprocess.check_output(["git", "-C", str(directory), "ls-files", "--stage", "-z"])
    for record in records.decode().split("\0"):
        if not record:
            continue
        metadata, name = record.split("\t", 1)
        mode, commit, stage = metadata.split()
        relative_path(name)
        if stage != "0":
            raise ValueError("Unmerged dependency source")
        target = directory / name
        output = prefix + "/" + name
        if mode == "160000":
            if not target.is_dir() or git(target, "rev-parse", "HEAD") != commit:
                raise ValueError(f"Missing or changed source submodule: {target}")
            child_files, child_revisions = export_source(target, output, modes)
            files.update(child_files)
            revisions.update(child_revisions)
        elif mode in ("100644", "100755"):
            if target.is_symlink():
                raise ValueError(f"Unexpected source symlink: {target}")
            modes[output] = int(mode, 8)
            files[output] = target.read_bytes()
        else:
            raise ValueError(f"Unsupported source mode {mode}: {target}")
    return files, revisions


def archive_files(path: Path) -> tuple[dict[str, bytes], list[str]]:
    with zipfile.ZipFile(path) as source:
        return ({n: source.read(n) for n in source.namelist() if not n.endswith("/")},
                [n.rstrip("/") for n in source.namelist() if n.endswith("/")])


def build_release(root: Path, offline: bool = False) -> None:
    lock = json.loads((root / LOCK).read_text())
    if lock["format"] != 1:
        raise ValueError("Unsupported dependency lock format")
    installed, sources, provenance = {}, {}, []
    source_modes = {}
    for dependency in lock["dependencies"]:
        print(f"Preparing {dependency['id']} {dependency['tag']}", flush=True)
        asset = download(dependency, root / "build/release-cache", offline)
        members = release_files(asset, dependency)
        if set(installed) & set(members):
            raise ValueError("Dependencies install overlapping paths")
        installed.update(members)
        checkout = source_checkout(dependency, root / "build/release-sources", offline)
        source, revisions = export_source(checkout, dependency["id"], source_modes)
        sources.update(source)
        provenance.append({**dependency, "source_revisions": revisions, "local_changes": []})
    sources["source-manifest.json"] = json_bytes({"format": 1, "dependencies": provenance})
    sources["README.md"] = (
        "# Gloam launch dependencies\n\nExact unmodified upstream source for the binaries in sd.zip.\n"
        "Revisions, releases and hashes: source-manifest.json.\n"
        "Each tree retains its own build instructions, licenses and notices.\n"
        "nds-bootstrap uses devkitARM; Slot-1 Launcher uses BlocksDS.\n"
        "SDK/runtime source references are retained in Gloam's third_party documentation.\n"
    ).encode()
    dist = root / "dist"
    dist.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".release-", dir=dist) as temporary:
        output = Path(temporary)
        base = package(root, output / "base", "0.1.0-dev")
        sd, directories = archive_files(base / "gloam-shell-ndx-0.1.0-dev-sd.zip")
        frontend, _ = archive_files(base / "gloam-shell-ndx-0.1.0-dev-source.zip")
        sd.pop("package-manifest.json", None)
        frontend.pop("package-manifest.json", None)
        if set(installed) & set(sd):
            raise ValueError("Backend files conflict with the checked-in template")
        sd.update(installed)
        sd["DEPENDENCY_SOURCES.txt"] = (
            "Matching source: dependencies-source.zip, alongside this SD download.\n"
            "See package-manifest.json for exact revisions, hashes, repository and release URLs.\n"
            "Redistributors must make that matching source available with the binaries.\n"
        ).encode()
        write_archive(output / "dependencies-source.zip", sources, [], source_modes)
        manifest = {
            "format": 1, "status": "development; physical hardware validation pending",
            "build": json.loads((root / "dist/build-info.json").read_text()),
            "dependencies": provenance,
            "dependency_source": {"file": "dependencies-source.zip",
                                  "sha256": sha256((output / "dependencies-source.zip").read_bytes())},
            "sd_files": {n: sha256(data) for n, data in sorted(sd.items())},
            "source_files": {n: sha256(data) for n, data in sorted(frontend.items())},
        }
        sd["package-manifest.json"] = json_bytes(manifest)
        frontend["package-manifest.json"] = json_bytes(manifest)
        write_archive(output / "sd.zip", sd, directories)
        write_archive(output / "source.zip", frontend, [])
        names = ["sd.zip", "source.zip", "dependencies-source.zip"]
        sums = "".join(f"{sha256((output / n).read_bytes())}  {n}\n" for n in names)
        (output / "SHA256SUMS").write_text(sums)
        for name in [*names, "SHA256SUMS"]:
            os.replace(output / name, dist / name)
    print(f"Built SD and matching source archives in {dist}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="Use the current verified frontend build")
    parser.add_argument("--offline", action="store_true", help="Require all dependencies in the local cache")
    args = parser.parse_args()
    try:
        if not args.skip_build:
            subprocess.run(["make", "-j4"], cwd=ROOT, check=True)
            subprocess.run(["python3", "tools/write_build_info.py"], cwd=ROOT, check=True)
        build_release(ROOT, args.offline)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Release package not created: {error}\n")


if __name__ == "__main__":
    main()
