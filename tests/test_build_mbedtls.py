"""Pinned TLS source and incremental builds use synthetic, offline fixtures."""

import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from tools import build_mbedtls as tls


class MbedTLSBuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="gloam tls ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "third_party/mbedtls").mkdir(parents=True)
        (self.root / "tools").mkdir()
        (self.root / "tools/build_mbedtls.py").write_text("fixture builder")
        (self.root / "third_party/mbedtls/blocksds.patch").write_text("fixture patch")
        self.archive = self.root / "build/release-cache/mbedtls-3.6.8.tar.bz2"
        self.archive.parent.mkdir(parents=True)
        self.write_archive({"mbedtls-3.6.8/include/mbedtls/version.h": b"fixture header"})
        self.pin = {"version": "3.6.8", "archive": self.archive.name,
                    "url": "https://example.invalid/source", "sha256": tls.digest(self.archive.read_bytes())}
        (self.root / "third_party/mbedtls/UPSTREAM.json").write_text(json.dumps(self.pin))

    def write_archive(self, entries, link=None):
        with tarfile.open(self.archive, "w:bz2") as archive:
            for name, data in entries.items():
                member = tarfile.TarInfo(name)
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
            if link:
                member = tarfile.TarInfo("mbedtls-3.6.8/link")
                member.type, member.linkname = tarfile.SYMTYPE, link
                archive.addfile(member)

    def test_offline_cache_verified_and_missing_or_corrupt_cache_rejected(self):
        self.assertEqual(tls.source_archive(self.root, True)[1], self.pin)
        self.archive.write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            tls.source_archive(self.root, True)
        self.archive.unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            tls.source_archive(self.root, True)

    def test_oversized_cache_rejected_before_reading(self):
        with self.archive.open("wb") as output:
            output.truncate(tls.MAX_ARCHIVE + 1)
        with patch.object(Path, "read_bytes", side_effect=AssertionError("must not read")):
            with self.assertRaisesRegex(ValueError, "size limit"):
                tls.source_archive(self.root, True)

    def test_archive_fully_validated_before_any_extraction(self):
        target = self.root / "source"
        target.mkdir()
        for name in ("../escape", "/absolute", "mbedtls-3.6.8/../../escape", "other/file"):
            self.write_archive({"mbedtls-3.6.8/good": b"safe", name: b"unsafe"})
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                tls.extract_source(self.archive, target, "3.6.8")
            self.assertEqual(list(target.iterdir()), [])
        self.write_archive({"mbedtls-3.6.8/good": b"safe"}, link="../../escape")
        with self.assertRaisesRegex(ValueError, "Unsafe"):
            tls.extract_source(self.archive, target, "3.6.8")

    def test_build_reuses_verified_outputs_and_repairs_library_or_header_corruption(self):
        sdk = self.root / "sdk with spaces"
        sdk.mkdir()
        (sdk / "version.txt").write_text("v1.25.0")
        toolchain = self.root / "compiler with spaces"
        commands = []

        def run(command, *, cwd, check):
            commands.append(command)
            if command[0] == "make":
                (cwd / "library").mkdir()
                for name in tls.LIBRARIES:
                    (cwd / "library" / name).write_bytes(b"synthetic " + name.encode())

        with patch.object(tls.subprocess, "check_output", return_value="16.2.0\n"), \
                patch.object(tls.subprocess, "run", side_effect=run):
            first = tls.build(self.root, sdk, toolchain, 2, True)
            self.assertEqual(tls.build(self.root, sdk, toolchain, 2, True), first)
            self.assertEqual(len(commands), 2)
            make = commands[1]
            self.assertIn("CC='" + str(toolchain / "arm-none-eabi-gcc") + "'", make)
            (self.root / "build/mbedtls/lib/libmbedtls.a").write_bytes(b"corrupt")
            tls.build(self.root, sdk, toolchain, 2, True)
            self.assertEqual(len(commands), 4)
            header = self.root / "build/mbedtls/include/mbedtls/version.h"
            header.write_bytes(b"corrupt header")
            tls.build(self.root, sdk, toolchain, 2, True)
            self.assertEqual(header.read_bytes(), b"fixture header")
            self.assertEqual(len(commands), 6)


if __name__ == "__main__":
    unittest.main()
