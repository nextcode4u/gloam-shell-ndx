"""Verify optional downloads cannot bypass licensing or archive/hash checks."""

import copy
import io
import json
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from tools.build_addons import addon_files, archive_members, selected_applications
from tools.package_development import sha256


def source_tar(files):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
        for name, content in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(content)
            archive.addfile(info, io.BytesIO(content))
    return buffer.getvalue()


class AddonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "config").mkdir()
        self.cache = self.root / "build/addons-cache"
        self.cache.mkdir(parents=True)
        self.source = source_tar({"demo/LICENSE": b"Synthetic permissive fixture notice",
                                  "demo/main.c": b"/* Authored synthetic fixture */\n"})
        self.binary = b"Authored synthetic application bytes, not a ROM fixture"
        (self.cache / "source.tar.gz").write_bytes(self.source)
        (self.cache / "demo.nds").write_bytes(self.binary)
        self.app = {
            "id": "demo", "version": "1", "redistribution": "approved",
            "package": {
                "source": {"asset": "source.tar.gz", "url": "https://example.invalid/source",
                           "sha256": sha256(self.source)},
                "files": [{"download": {"asset": "demo.nds", "url": "https://example.invalid/app",
                                         "sha256": sha256(self.binary)},
                           "format": "raw", "destination": "apps/demo.nds",
                           "sha256": sha256(self.binary)}],
                "notices": [{"member": "demo/LICENSE", "destination": "LICENSE"}],
            },
        }
        self.lock = {"format": 1, "reviewed": "fixture", "application_directory": "apps",
                     "applications": [self.app]}

    def write_lock(self):
        (self.root / "config/addon-dependencies.json").write_text(json.dumps(self.lock))

    def test_permission_filter_and_explicit_selection(self):
        conditional = {**self.app, "id": "conditional", "redistribution": "noncommercial"}
        held = {**self.app, "id": "held", "redistribution": "blocked"}
        self.lock["applications"] += [conditional, held]
        self.assertEqual([a["id"] for a in selected_applications(self.lock)], ["demo"])
        self.assertEqual([a["id"] for a in selected_applications(self.lock, True)],
                         ["conditional", "demo"])
        for requested in (["held"], ["conditional"], ["missing"]):
            with self.assertRaises(ValueError):
                selected_applications(self.lock, only=requested)
        self.assertEqual(selected_applications(self.lock, True, ["conditional"]), [conditional])

    def test_shim_dependency_cannot_bypass_permission_review(self):
        shim = {**self.app, "id": "snemulds-shim"}
        payload = {**self.app, "id": "snemulds"}
        self.lock["applications"] = [shim, payload]
        self.assertEqual([a["id"] for a in selected_applications(self.lock, only=[shim["id"]])],
                         ["snemulds", "snemulds-shim"])
        payload["redistribution"] = "blocked"
        with self.assertRaisesRegex(ValueError, "eligible SNEmulDS"):
            selected_applications(self.lock, only=[shim["id"]])

    def test_runtime_source_and_notices_are_separate_and_match(self):
        self.write_lock()
        runtime, source, notices, report = addon_files(self.root, offline=True)
        self.assertEqual(runtime, {"apps/demo.nds": self.binary})
        self.assertEqual(source["dependencies/addons/demo/source.tar.gz"], self.source)
        self.assertEqual(notices["addons/demo/LICENSE"], b"Synthetic permissive fixture notice")
        self.assertEqual(report["runtime_files"], {"apps/demo.nds": sha256(self.binary)})
        self.assertFalse(report["noncommercial"])
        self.assertEqual(json.loads(source["dependencies/addons/manifest.json"]), report)

    def test_wrong_download_hash_member_hash_and_target_rejected(self):
        for mutation in ("download", "member", "target", "cache", "overlap"):
            with self.subTest(mutation=mutation):
                app = copy.deepcopy(self.app)
                item = app["package"]["files"][0]
                if mutation == "download":
                    item["download"]["sha256"] = "0" * 64
                elif mutation == "member":
                    item["sha256"] = "0" * 64
                elif mutation == "target":
                    item["destination"] = "_gba/bios.bin"
                elif mutation == "cache":
                    item["download"]["asset"] = "../demo.nds"
                else:
                    app["package"]["files"].append(copy.deepcopy(item))
                self.lock["applications"] = [app]
                self.write_lock()
                with self.assertRaises(ValueError):
                    addon_files(self.root, offline=True)

    def test_archive_paths_links_duplicates_and_size_rejected(self):
        with self.assertRaises(ValueError):
            archive_members(source_tar({"../private": b"bad"}), "tar.gz")
        with self.assertRaises(ValueError):
            archive_members(source_tar({"same": b"a", "SAME": b"b"}), "tar.gz")
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.FIFOTYPE):
            buffer = io.BytesIO()
            with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
                member = tarfile.TarInfo("bad")
                member.type = kind
                member.linkname = "elsewhere"
                archive.addfile(member)
            with self.assertRaises(ValueError):
                archive_members(buffer.getvalue(), "tar.gz")
        with patch("tools.build_addons.MAX_MEMBER", 4):
            with self.assertRaisesRegex(ValueError, "Oversized"):
                archive_members(source_tar({"large": b"12345"}), "tar.gz")
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr("/absolute", b"bad")
        with self.assertRaises(ValueError):
            archive_members(buffer.getvalue(), "zip")


if __name__ == "__main__":
    unittest.main()
