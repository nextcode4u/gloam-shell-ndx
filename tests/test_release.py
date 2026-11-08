"""Release integrity checks use synthetic archives, never game or BIOS fixtures."""

import hashlib
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.build_release import download, export_source, release_files, source_checkout


def digest(data):
    return hashlib.sha256(data).hexdigest()


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def archive(self, files):
        path = self.root / "backend.zip"
        with zipfile.ZipFile(path, "w") as archive:
            for name, data in files.items():
                archive.writestr(name, data)
        return path

    def dependency(self):
        return {"files": [{"member": "loader.nds", "destination": "_nds/loader.nds",
                           "sha256": digest(b"authored synthetic bytes")} ]}

    def test_only_reviewed_members_installed_and_bytes_verified(self):
        path = self.archive({"loader.nds": b"authored synthetic bytes", "unrelated.cia": b"omit"})
        self.assertEqual(release_files(path, self.dependency()),
                         {"_nds/loader.nds": b"authored synthetic bytes"})
        dependency = self.dependency()
        dependency["files"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            release_files(path, dependency)

    def test_archive_traversal_and_duplicate_names_rejected(self):
        for bad in ["../private", "/absolute", "C:\\private"]:
            path = self.archive({"loader.nds": b"authored synthetic bytes", bad: b"bad"})
            with self.assertRaises(ValueError):
                release_files(path, self.dependency())
        path = self.archive({"loader.nds": b"authored synthetic bytes", "LOADER.NDS": b"duplicate"})
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            release_files(path, self.dependency())

    def test_offline_cache_requires_correct_hash(self):
        dependency = {"asset": "backend.zip", "sha256": digest(b"expected")}
        with self.assertRaisesRegex(ValueError, "Offline cache missing"):
            download(dependency, self.root, True)
        (self.root / "backend.zip").write_bytes(b"wrong")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            download(dependency, self.root, True)
        (self.root / "backend.zip").write_bytes(b"expected")
        self.assertEqual(download(dependency, self.root, True).read_bytes(), b"expected")

    def test_exact_clean_source_and_build_script_modes(self):
        directory = self.root / "source"
        directory.mkdir()
        def git(*args):
            return subprocess.check_output(["git", "-C", str(directory), *args], text=True).strip()
        git("init", "-q")
        (directory / "LICENSE").write_text("Synthetic license fixture")
        (directory / "build.sh").write_text("#!/bin/sh\nexit 0\n")
        (directory / "build.sh").chmod(0o755)
        git("add", ".")
        git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
            "commit", "-qm", "Synthetic source fixture")
        commit = git("rev-parse", "HEAD")
        dependency = {"id": "source", "commit": commit}
        self.assertEqual(source_checkout(dependency, self.root, True), directory)
        modes = {}
        files, revisions = export_source(directory, "backend", modes)
        self.assertEqual(set(files), {"backend/LICENSE", "backend/build.sh"})
        self.assertEqual(revisions, {"backend": commit})
        self.assertEqual(modes["backend/build.sh"], 0o100755)
        dependency["commit"] = "0" * 40
        with self.assertRaisesRegex(ValueError, "revision mismatch"):
            source_checkout(dependency, self.root, True)
        dependency["commit"] = commit
        (directory / "LICENSE").write_text("changed")
        with self.assertRaisesRegex(ValueError, "checkout changed"):
            source_checkout(dependency, self.root, True)
