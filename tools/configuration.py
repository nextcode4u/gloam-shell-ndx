#!/usr/bin/env python3
"""Export, restore or recover frontend configuration without moving apps or saves."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import stat
import tempfile
import uuid
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath

OWNER = Path("_nds/dsi-frontend")
MAX_FILES = 1024
MAX_FILE_BYTES = 65536
MAX_TOTAL_BYTES = 4 * 1024 * 1024
ROOT_RECORDS = {
    "state.ini",
    "tabs.ini",
    "dashboard.ini",
    "recent.ini", "language.ini",
    "appearance-1.ini",
    "appearance-2.ini",
    "appearance-3.ini",
}
HASH_RECORD = re.compile(r"[0-9a-f]{16}(?:\.cheats)?\.ini\Z")
EXTENSION_RECORD = re.compile(r"[a-z0-9_-]{1,15}\.ini\Z")


def allowed_record(name: str) -> bool:
    if "\\" in name or any(ord(char) < 32 for char in name):
        return False
    path = PurePosixPath(name)
    if path.as_posix() != name or path.is_absolute() or ".." in path.parts:
        return False
    parts = path.parts
    if len(parts) == 1:
        return name in ROOT_RECORDS
    if len(parts) == 2 and parts[0] in {"games", "items"}:
        return bool(HASH_RECORD.fullmatch(parts[1]))
    return (
        len(parts) == 3
        and parts[:2] == ("plugins", "user")
        and bool(EXTENSION_RECORD.fullmatch(parts[2]))
    )


def private_directory(sd: Path) -> Path:
    root = sd.resolve(strict=True)
    for component in [root / "_nds", root / OWNER]:
        if component.is_symlink():
            raise ValueError(f"Refusing redirected configuration directory: {component}")
    private = root / OWNER
    private.mkdir(parents=True, exist_ok=True)
    return private


def record_path(private: Path, name: str) -> Path:
    if private.is_symlink():
        raise ValueError("Redirected configuration base directory")
    if not allowed_record(name):
        raise ValueError(f"Unsupported configuration path: {name!r}")
    target = private
    for component in PurePosixPath(name).parts:
        target /= component
        if target.is_symlink():
            raise ValueError(f"Refusing redirected configuration record: {target}")
    return target


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_record(path: Path) -> bytes:
    with path.open("rb") as stream:
        data = stream.read(MAX_FILE_BYTES + 1)
    if len(data) > MAX_FILE_BYTES or b"\0" in data:
        raise ValueError(f"Oversized or non-text configuration record: {path}")
    return data


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=".frontend-config-", dir=path.parent)
    try:
        with os.fdopen(handle, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def export_configuration(sd: Path, output: Path) -> int:
    private = private_directory(sd)
    if output.suffix.lower() not in {".zip", ".dsconfig"}:
        raise ValueError("Configuration exports must use .zip or .dsconfig")
    candidates = [private / name for name in ROOT_RECORDS]
    for subdirectory in ["games", "items", "plugins/user"]:
        directory = private / subdirectory
        if directory.is_symlink():
            raise ValueError(f"Refusing redirected configuration folder: {directory}")
        if directory.exists():
            candidates.extend(directory.glob("*.ini"))
    records: dict[str, bytes] = {}
    total = 0
    for candidate in sorted(candidates):
        name = candidate.relative_to(private).as_posix()
        if not allowed_record(name) or not candidate.exists():
            continue
        path = record_path(private, name)
        if not path.is_file():
            raise ValueError(f"Configuration record is not a file: {path}")
        data = read_record(path)
        records[name] = data
        total += len(data)
        if len(records) > MAX_FILES or total > MAX_TOTAL_BYTES:
            raise ValueError("Configuration exceeds the portable archive limits")
    if output.suffix.lower() == ".dsconfig":
        from native_configuration import encode_records

        atomic_write(output, encode_records(records))
        return len(records)
    manifest = {
        "version": 1,
        "files": [
            {"path": name, "bytes": len(data), "sha256": digest(data)}
            for name, data in records.items()
        ],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=".frontend-export-", dir=output.parent)
    os.close(handle)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
            for name, data in records.items():
                archive.writestr(name, data)
        os.replace(temporary, output)
    finally:
        Path(temporary).unlink(missing_ok=True)
    return len(records)


def read_archive(source: Path) -> dict[str, bytes]:
    with source.open("rb") as stream:
        native = stream.read(8) == b"DSCFG001"
    if native:
        from native_configuration import decode_records

        if source.stat().st_size > MAX_TOTAL_BYTES + MAX_FILES * 108:
            raise ValueError("Oversized native configuration archive")
        return decode_records(source.read_bytes())
    with zipfile.ZipFile(source) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries]
        if len(entries) > MAX_FILES + 1 or len(set(name.casefold() for name in names)) != len(
            names
        ):
            raise ValueError("Duplicate paths or too many archive records")
        for entry in entries:
            mode = entry.external_attr >> 16
            if (
                stat.S_ISLNK(mode)
                or entry.is_dir()
                or entry.flag_bits & 1
                or entry.file_size
                > (MAX_TOTAL_BYTES if entry.filename == "manifest.json" else MAX_FILE_BYTES)
            ):
                raise ValueError("Unsupported or oversized archive entry")
        manifest = json.loads(archive.read("manifest.json"))
        if manifest.get("version") != 1 or not isinstance(manifest.get("files"), list):
            raise ValueError("Unsupported configuration archive version")
        if len(manifest["files"]) > MAX_FILES:
            raise ValueError("Too many configuration records")
        records: dict[str, bytes] = {}
        total = 0
        for record in manifest["files"]:
            name = record["path"]
            if not isinstance(name, str) or not allowed_record(name) or name in records:
                raise ValueError("Unsupported or duplicate manifest path")
            data = archive.read(name)
            total += len(data)
            if (
                len(data) > MAX_FILE_BYTES
                or total > MAX_TOTAL_BYTES
                or b"\0" in data
                or record["bytes"] != len(data)
                or record["sha256"] != digest(data)
            ):
                raise ValueError(f"Invalid configuration data: {name}")
            records[name] = data
        if set(names) != {"manifest.json", *records}:
            raise ValueError("Archive contains undeclared files")
        return records


def write_journal(backup: Path, journal: dict) -> None:
    atomic_write(backup / "transaction.json", (json.dumps(journal, indent=2) + "\n").encode())


def rollback(private: Path, backup: Path, journal: dict) -> None:
    # Validate every current/old record before changing anything during recovery.
    plans: list[tuple[Path, bytes | None]] = []
    for record in journal["files"]:
        name = record["path"]
        target = record_path(private, name)
        current = digest(read_record(target)) if target.exists() else None
        if current not in {None, record["old_sha256"], record["new_sha256"]}:
            raise ValueError(f"Record changed outside this import; preserved: {target}")
        old = None
        if record["old_sha256"] is not None:
            old_path = record_path(backup / "old", name)
            if old_path.is_symlink():
                raise ValueError("Redirected recovery file")
            old = read_record(old_path)
            if digest(old) != record["old_sha256"]:
                raise ValueError("Recovery backup checksum differs")
        plans.append((target, old))
    for target, old in plans:
        if old is None:
            target.unlink(missing_ok=True)
        else:
            atomic_write(target, old)
    journal["state"] = "rolled_back"
    write_journal(backup, journal)


def recover_configuration(sd: Path) -> int:
    private = private_directory(sd)
    count = 0
    backups = private / "configuration-backups"
    if backups.is_symlink():
        raise ValueError("Redirected backup directory")
    for path in sorted(backups.glob("*/transaction.json")):
        if path.is_symlink() or path.parent.is_symlink():
            raise ValueError("Redirected recovery journal")
        if path.stat().st_size > MAX_TOTAL_BYTES:
            raise ValueError("Oversized recovery journal")
        journal = json.loads(path.read_text())
        if (
            journal.get("version") != 1
            or not isinstance(journal.get("files"), list)
            or len(journal["files"]) > MAX_FILES
        ):
            raise ValueError("Invalid recovery journal")
        if journal.get("state") == "prepared":
            rollback(private, path.parent, journal)
            count += 1
    return count


def import_configuration(sd: Path, source: Path) -> Path:
    records = read_archive(source)  # Entire archive is checked before any target write.
    private = private_directory(sd)
    recover_configuration(sd)
    # Resolve/check every destination before creating the backup transaction.
    targets = {name: record_path(private, name) for name in records}
    old = {name: read_record(path) if path.exists() else None for name, path in targets.items()}
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    backups = private / "configuration-backups"
    if backups.is_symlink():
        raise ValueError("Redirected backup directory")
    backup = backups / f"{stamp}-{uuid.uuid4().hex[:8]}"
    backup.mkdir(parents=True)
    journal = {"version": 1, "state": "prepared", "files": []}
    for name, data in records.items():
        if old[name] is not None:
            atomic_write(backup / "old" / name, old[name])
        journal["files"].append(
            {
                "path": name,
                "old_sha256": digest(old[name]) if old[name] is not None else None,
                "new_sha256": digest(data),
            }
        )
    write_journal(backup, journal)
    try:
        for name, data in records.items():
            atomic_write(targets[name], data)
    except BaseException:
        rollback(private, backup, journal)
        raise
    journal["state"] = "complete"
    write_journal(backup, journal)
    return backup


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["export", "import", "recover"])
    parser.add_argument("--sd", type=Path, required=True, help="SD root; close the emulator first")
    parser.add_argument(
        "--archive", type=Path, help="ZIP or native .dsconfig path for export/import"
    )
    args = parser.parse_args()
    if args.action != "recover" and not args.archive:
        parser.error("--archive is required for export/import")
    try:
        if args.action == "export":
            print(
                f"Exported {export_configuration(args.sd, args.archive)} records to {args.archive}"
            )
        elif args.action == "import":
            backup = import_configuration(args.sd, args.archive)
            print(
                f"Restored configuration; original files are backed up at {backup}. Restart the frontend."
            )
        else:
            print(f"Recovered {recover_configuration(args.sd)} interrupted imports")
    except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Configuration operation stopped: {error}\n")


if __name__ == "__main__":
    main()
