"""Exercise the portable installer's trust and SD-write boundaries."""

import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.download_addons import destination, digest, stage


class DownloadAddonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sd = self.root / "sd"
        self.sd.mkdir()
        self.cache = self.root / "cache"
        self.cache.mkdir()
        self.binary = b"Synthetic executable fixture"
        self.config = b'{"runSettings": {}}'
        self.name = "_nds/dsi-frontend/plugins/apps/demo.nds"
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr("demo.nds", self.binary)
            archive.writestr("_gba/configs/ABCE01.json", self.config)
            archive.writestr("_gba/bios.bin", b"Synthetic excluded firmware fixture")
        self.blob = buffer.getvalue()
        (self.cache / "demo.zip").write_bytes(self.blob)
        download = {"asset": "demo.zip", "url": "https://example.invalid/demo.zip",
                    "sha256": digest(self.blob)}
        self.app = {"id": "demo", "noncommercial": False, "files": [{
            "download": download, "format": "zip", "member": "demo.nds",
            "destination": self.name, "sha256": digest(self.binary)}],
            "game_configs": {"download": download,
                             "files": {"_gba/configs/ABCE01.json": digest(self.config)}}}
        self.manifest = {"format": 1, "applications": [self.app], "manual": []}

    def run_stage(self, **options):
        return stage(self.manifest, self.sd, self.cache, offline=True, **options)

    def test_only_pinned_members_are_installed_and_existing_files_are_preserved(self):
        self.assertEqual(self.run_stage(), (2, 0))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)
        self.assertFalse((self.sd / "_gba/bios.bin").exists())
        (self.sd / self.name).write_bytes(b"User's existing executable")
        self.assertEqual(self.run_stage(), (0, 2))
        self.assertEqual((self.sd / self.name).read_bytes(), b"User's existing executable")
        self.assertEqual(self.run_stage(overwrite=True), (2, 0))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)

    def test_dry_run_makes_no_downloads_or_sd_writes(self):
        (self.cache / "demo.zip").unlink()
        self.assertEqual(self.run_stage(dry_run=True), (0, 0))
        self.assertEqual(list(self.sd.iterdir()), [])
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_corrupt_download_or_member_cannot_be_staged(self):
        (self.cache / "demo.zip").write_bytes(b"Corrupt cached archive")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.run_stage()
        self.assertEqual(list(self.sd.iterdir()), [])
        (self.cache / "demo.zip").write_bytes(self.blob)
        self.app["files"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "Member hash mismatch"):
            self.run_stage()
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_noncommercial_selection_and_unknown_ids_are_explicit(self):
        self.app["noncommercial"] = True
        self.assertEqual(self.run_stage(), (0, 0))
        with self.assertRaisesRegex(ValueError, "noncommercial"):
            self.run_stage(only=["demo"])
        with self.assertRaisesRegex(ValueError, "No pinned download"):
            self.run_stage(only=["missing"])
        self.assertEqual(self.run_stage(noncommercial=True), (2, 0))

    def test_path_escape_firmware_and_symlink_destinations_are_rejected(self):
        for name in ["../outside.nds", "/outside.nds", "_gba/bios.bin", "C:/outside.nds",
                     "_nds/dsi-frontend/plugins/apps/../../settings.ini"]:
            with self.assertRaises(ValueError):
                destination(self.sd, name)
        outside = self.root / "outside"
        outside.mkdir()
        (self.sd / "_nds").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.run_stage()
        self.assertEqual(list(outside.iterdir()), [])

    def test_all_destinations_are_validated_before_any_install(self):
        second = copy.deepcopy(self.app)
        second["id"] = "other"
        self.manifest["applications"].append(second)
        with self.assertRaisesRegex(ValueError, "overlapping"):
            self.run_stage()
        self.assertEqual(list(self.sd.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
