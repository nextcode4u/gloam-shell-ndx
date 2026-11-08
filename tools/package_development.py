#!/usr/bin/env python3
"""Build reproducible local SD/source archives from explicitly reviewed inputs."""

import argparse
import hashlib
import json
import posixpath
import re
import tempfile
import zipfile
from pathlib import Path, PurePosixPath
from urllib.parse import unquote

PROJECT = Path(__file__).resolve().parents[1]
CATALOG = "config/package-inputs.json"
STAMP = (1980, 1, 1, 0, 0, 0)


def relative_path(name: str) -> PurePosixPath:
    """Validate an archive member before it can become an input or ZIP path."""
    path = PurePosixPath(name)
    if not name or path.is_absolute() or any(part in ("", ".", "..") for part in name.split("/")):
        raise ValueError(f"Invalid package input: {name}")
    if "\\" in name or ":" in name:
        raise ValueError(f"Invalid package input: {name}")
    return path


def read_input(root: Path, name: str) -> bytes:
    """Reject traversal and symlinks before reading an approved relative file."""
    path = relative_path(name)
    current = root
    for part in path.parts:
        current /= part
        if current.is_symlink():
            raise ValueError(f"Package inputs must not be symlinks: {name}")
    if not current.is_file():
        raise ValueError(f"Missing package input: {name}")
    return current.read_bytes()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(data: object) -> bytes:
    return (json.dumps(data, indent=2, sort_keys=True) + "\n").encode()


def sd_documentation(name: str, data: bytes, sd_names: set[str], source_names: set[str]) -> bytes:
    """Keep navigation usable without pretending source-only files are on SD."""

    def reference(match: re.Match) -> str:
        label, target = match.groups()
        path = target.strip("<>").split("#", 1)[0]
        if not path or path.startswith("/") or re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", path):
            return match.group(0)
        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(path)))
        if resolved in sd_names:
            return match.group(0)
        location = "source package" if resolved in source_names else "local reference"
        return f"{label} ({location}: `{resolved}`)"

    return re.sub(r"\[([^\]\n]+)\]\(([^)\n]+)\)", reference, data.decode()).encode()


def write_archive(
    path: Path, files: dict[str, bytes], directories: list[str], modes: dict[str, int] | None = None
) -> None:
    with zipfile.ZipFile(path, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        entries = {**{name.rstrip("/") + "/": b"" for name in directories}, **files}
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(name, STAMP)
            info.create_system = 3
            directory = name.endswith("/")
            mode = 0o40755 if directory else 0o100755 if name.endswith(".sh") else 0o100644
            mode = (modes or {}).get(name, mode)
            info.external_attr = (mode << 16) | (0x10 if directory else 0)
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data, compresslevel=9)
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise ValueError("Archive CRC verification failed")
        for name, data in files.items():
            if archive.read(name) != data:
                raise ValueError(f"Archive readback failed: {name}")


def package(root: Path, destination: Path, version: str) -> Path:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+-dev(?:\.[0-9]+)?", version):
        raise ValueError("Use a development version such as 0.1.0-dev.1")
    if destination.exists():
        raise ValueError("Destination already exists; choose a new package directory")
    catalog = json.loads(read_input(root, CATALOG))
    sd_names = catalog["sd_files"]
    for name in sd_names:
        relative_path(name)
        if not name.startswith("sdroot/"):
            raise ValueError("SD inputs must be inside the reviewed template")
    for name in catalog["sd_directories"]:
        relative_path(name)
    source_names = catalog["source_files"]
    for name in source_names:
        relative_path(name)
        if PurePosixPath(name).parts[0].lower() in ("ref", "backups", "testing", "build", "dist"):
            raise ValueError("Private/generated trees cannot enter the source archive")
        if PurePosixPath(name).suffix.lower() in (
            ".nds",
            ".dsi",
            ".sav",
            ".pub",
            ".prv",
            ".img",
            ".elf",
            ".pyc",
        ):
            raise ValueError("Executables/private runtime files cannot enter the source archive")
    for names in (sd_names, source_names):
        if len({name.casefold() for name in names}) != len(names):
            raise ValueError("Case-colliding package inputs")
    if len(set(sd_names)) != len(sd_names) or len(set(source_names)) != len(source_names):
        raise ValueError("Duplicate package input")
    # A user may add private material to a local template. Fail closed until
    # that change has been deliberately reviewed in the input catalog.
    actual = set()
    for path in (root / "sdroot").rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Template symlink: {path}")
        if path.is_file() and path.name != ".gitkeep":
            actual.add(path.relative_to(root).as_posix())
    if actual != set(sd_names):
        raise ValueError("SD template differs from its reviewed file list")
    rom = read_input(root, "gloam-shell-ndx.nds")
    if len(rom) < 512:
        raise ValueError("Build the frontend first")
    build = json.loads(read_input(root, "dist/build-info.json"))
    if build["sha256"] != sha256(rom) or build["bytes"] != len(rom):
        raise ValueError("Build manifest is stale")
    source = {name: read_input(root, name) for name in source_names}
    if CATALOG not in source:
        raise ValueError("Source archive must contain its input catalog")
    sd = {name.removeprefix("sdroot/"): read_input(root, name) for name in sd_names}
    sd["BOOT.NDS"] = rom
    if catalog.get("include_forwarder"):
        if __package__:
            from .build_forwarder import verified_files
        else:
            from build_forwarder import verified_files
        generated = verified_files(root)
        if set(generated) & set(sd):
            raise ValueError("Forwarder output conflicts with the SD template")
        sd.update(generated)
    # Full dependency sources/notices and the original-code license accompany
    # the binary in each development snapshot.
    notices = [name for name in source if name.startswith("third_party/")]
    for name in notices:
        basename = Path(name).name.lower()
        if name.endswith(("UPSTREAM.md", "README.md")) or any(
            marker in basename for marker in ("license", "copying")
        ):
            sd["docs/notices/" + name] = source[name]
    # Keep the reviewed documentation at its source-relative paths. Moving
    # README into docs/ used to break its links and omit referenced guides.
    for name, data in source.items():
        if name in ("README.md", "CREDITS.md", "THIRD_PARTY.md", "LICENSE") or name.startswith(
            "docs/"
        ):
            sd[name] = data
    available = set(sd)
    for name, data in sd.items():
        if name.endswith(".md") and not name.startswith("docs/notices/"):
            sd[name] = sd_documentation(name, data, available, set(source))
    manifest = {
        "format": 1,
        "version": version,
        "status": "development; physical hardware validation pending",
        "original_source_license": "GPL-3.0-or-later",
        "backend": "template snapshot; use build_release.py to bundle official launch backends",
        "build": build,
        "sd_files": {name: sha256(data) for name, data in sorted(sd.items())},
        "source_files": {name: sha256(data) for name, data in sorted(source.items())},
    }
    sd["package-manifest.json"] = json_bytes(manifest)
    source["package-manifest.json"] = json_bytes(manifest)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".frontend-package-", dir=destination.parent
    ) as temporary:
        output = Path(temporary)
        archives = [output / f"gloam-shell-ndx-{version}-{kind}.zip" for kind in ("sd", "source")]
        write_archive(archives[0], sd, catalog["sd_directories"])
        write_archive(archives[1], source, [])
        checksums = {path.name: sha256(path.read_bytes()) for path in archives}
        (output / "SHA256SUMS").write_text(
            "".join(f"{value}  {name}\n" for name, value in sorted(checksums.items()))
        )
        # Publication is local and never replaces an existing review artifact.
        output.rename(destination)
    return destination


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--destination", required=True, type=Path)
    args = parser.parse_args()
    try:
        output = package(PROJECT, args.destination.resolve(), args.version)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"Package not created: {error}\n")
    print(f"Verified development SD/source archives: {output}")


if __name__ == "__main__":
    main()
