#!/usr/bin/env python3
"""Build the pinned HTTPS library without replacing the installed SDK package."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import shlex
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MAX_ARCHIVE = 16 * 1024 * 1024
MAX_SOURCE = 128 * 1024 * 1024
LIBRARIES = ("libmbedtls.a", "libmbedx509.a", "libmbedcrypto.a")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def source_archive(root: Path, offline: bool = False) -> tuple[Path, dict]:
    pin = json.loads((root / "third_party/mbedtls/UPSTREAM.json").read_text())
    cache = root / "build/release-cache"
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / pin["archive"]
    if archive.is_symlink():
        raise ValueError("Mbed TLS archive must not be a symlink")
    if archive.exists():
        if archive.stat().st_size > MAX_ARCHIVE:
            raise ValueError("Mbed TLS archive exceeds its size limit")
        with archive.open("rb") as source:
            data = source.read(MAX_ARCHIVE + 1)
    elif offline:
        raise ValueError("Offline Mbed TLS source archive is missing")
    else:
        request = urllib.request.Request(pin["url"], headers={"User-Agent": "Gloam-build"})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read(MAX_ARCHIVE + 1)
    if len(data) > MAX_ARCHIVE or digest(data) != pin["sha256"]:
        raise ValueError("Mbed TLS source archive hash mismatch")
    if not archive.exists():
        with tempfile.NamedTemporaryFile(dir=cache, delete=False) as output:
            temporary = Path(output.name)
            output.write(data)
        try:
            temporary.replace(archive)
        finally:
            temporary.unlink(missing_ok=True)
    return archive, pin


def extract_source(archive: Path, destination: Path, version: str) -> Path:
    """Validate the complete archive before writing any source members."""
    prefix = "mbedtls-" + version
    with tarfile.open(archive, "r:bz2") as source:
        members = source.getmembers()
        total, names = 0, set()
        if len(members) > 10000:
            raise ValueError("Too many Mbed TLS source members")
        for member in members:
            path = PurePosixPath(member.name)
            if (not member.name or path.is_absolute() or path.parts[0] != prefix
                    or ".." in path.parts or "\\" in member.name or ":" in member.name
                    or not (member.isdir() or member.isfile()) or member.name in names
                    or member.size < 0):
                raise ValueError("Unsafe Mbed TLS source member")
            names.add(member.name)
            total += member.size
            if total > MAX_SOURCE:
                raise ValueError("Mbed TLS expanded source exceeds its size limit")
        source.extractall(destination, members=members, filter="data")
    return destination / prefix


def build(root: Path, sdk: Path, toolchain: Path, jobs: int, offline: bool = False) -> dict:
    archive, pin = source_archive(root, offline)
    patch = root / "third_party/mbedtls/blocksds.patch"
    compiler = toolchain / "arm-none-eabi-gcc"
    identity = {
        "version": pin["version"], "source_sha256": pin["sha256"],
        "patch_sha256": digest(patch.read_bytes()),
        "builder_sha256": digest((root / "tools/build_mbedtls.py").read_bytes()),
        "compiler": subprocess.check_output([str(compiler), "-dumpfullversion"], text=True).strip(),
        "sdk": (sdk / "version.txt").read_text().strip(),
    }
    prefix = root / "build/mbedtls"
    stamp = prefix / "build-info.json"
    if prefix.is_symlink():
        raise ValueError("Mbed TLS build directory must not be a symlink")
    try:
        previous = json.loads(stamp.read_text())
        hashes = {name: digest((prefix / "lib" / name).read_bytes()) for name in LIBRARIES}
        headers = {p.relative_to(prefix / "include").as_posix(): digest(p.read_bytes())
                   for p in (prefix / "include").rglob("*") if p.is_file()}
        if (isinstance(previous, dict) and all(previous.get(k) == v for k, v in identity.items())
                and previous.get("libraries") == hashes and previous.get("headers") == headers):
            return previous
    except (OSError, ValueError):
        pass
    with tempfile.TemporaryDirectory(prefix=".mbedtls-", dir=root / "build") as temporary:
        stage = Path(temporary)
        source = extract_source(archive, stage, pin["version"])
        subprocess.run(["patch", "--batch", "--fuzz=0", "-p1", "-i", str(patch)],
                       cwd=source, check=True)
        flags = ["-O2", "-mcpu=arm946e-s+nofp", "-mthumb", "-ffunction-sections", "-fdata-sections",
                 "-I" + str(sdk / "libs/libnds/include"),
                 "-I" + str(sdk / "libs/dswifi/include"), "-D__unix__"]
        # Makefile variable values need quoting when SDK paths contain spaces.
        subprocess.run(["make", "lib", "-j" + str(jobs), "CC=" + shlex.quote(str(compiler)),
                        "AR=" + shlex.quote(str(toolchain / "arm-none-eabi-ar")),
                        "CFLAGS=" + shlex.join(flags)], cwd=source, check=True)
        installed = stage / "installed"
        installed.mkdir()
        shutil.copytree(source / "include", installed / "include")
        (installed / "lib").mkdir()
        for name in LIBRARIES:
            shutil.copyfile(source / "library" / name, installed / "lib" / name)
        identity["headers"] = {p.relative_to(installed / "include").as_posix(): digest(p.read_bytes())
                               for p in (installed / "include").rglob("*") if p.is_file()}
        identity["libraries"] = {name: digest((installed / "lib" / name).read_bytes()) for name in LIBRARIES}
        (installed / "build-info.json").write_text(json.dumps(identity, indent=2) + "\n")
        # Publish only after the full library set builds successfully.
        if prefix.exists():
            shutil.rmtree(prefix)
        installed.rename(prefix)
    return identity


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--jobs", type=int, choices=range(1, 65), default=4)
    parser.add_argument("--blocksds", type=Path, default=Path(os.environ.get("BLOCKSDS", "/opt/blocksds/core")))
    parser.add_argument("--toolchain", type=Path, default=Path(os.environ.get("ARM_NONE_EABI_PATH",
                        "/opt/wonderful/toolchain/gcc-arm-none-eabi/bin")))
    args = parser.parse_args()
    result = build(ROOT, args.blocksds.resolve(), args.toolchain.resolve(), args.jobs, args.offline)
    print("Mbed TLS " + result["version"] + " ready (verified source and library hashes).")


if __name__ == "__main__":
    main()
