"""Portable configuration must never import apps, games, saves or redirected paths."""

import importlib.util
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("configuration", ROOT / "tools/configuration.py")
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)


class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.target = self.root / "target"
        self.source.mkdir()
        self.target.mkdir()
        self.private = config.private_directory(self.source)
        (self.private / "state.ini").write_text("version=1\nicon_size=1\n")
        (self.private / "games").mkdir()
        (self.private / "games/0123456789abcdef.ini").write_text("version=1\nmode=0\n")
        (self.source / "roms").mkdir()
        (self.source / "roms/save.sav").write_bytes(b"untouched game save")
        (self.private / "themes").mkdir()
        (self.private / "themes/music.wav").write_bytes(b"not configuration")
        self.archive = self.root / "configuration.zip"
        self.assertEqual(config.export_configuration(self.source, self.archive), 2)

    def test_round_trip_and_verified_original_backup(self):
        target = config.private_directory(self.target)
        original = b"version=1\nicon_size=0\n"
        (target / "state.ini").write_bytes(original)
        backup = config.import_configuration(self.target, self.archive)
        self.assertEqual(
            (target / "state.ini").read_bytes(), (self.private / "state.ini").read_bytes()
        )
        self.assertEqual((backup / "old/state.ini").read_bytes(), original)
        self.assertEqual(json.loads((backup / "transaction.json").read_text())["state"], "complete")
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(
                set(archive.namelist()),
                {"manifest.json", "state.ini", "games/0123456789abcdef.ini"},
            )
        self.assertEqual((self.source / "roms/save.sav").read_bytes(), b"untouched game save")

    def test_reject_paths_checksum_duplicates_and_non_configuration(self):
        for name in [
            "../state.ini",
            "roms/game.sav",
            "/state.ini",
            "items/../state.ini",
            "state.ini/",
            "plugins/user/../../state.ini",
        ]:
            self.assertFalse(config.allowed_record(name))
        with zipfile.ZipFile(self.archive) as source:
            original = {name: source.read(name) for name in source.namelist()}
        for variant in ["checksum", "traversal", "duplicate", "undeclared"]:
            bad = self.root / f"{variant}.zip"
            content = original.copy()
            if variant == "checksum":
                content["state.ini"] = b"corrupted"
            if variant == "traversal":
                manifest = json.loads(content["manifest.json"])
                manifest["files"][0]["path"] = "../state.ini"
                content["manifest.json"] = json.dumps(manifest).encode()
            if variant == "undeclared":
                content["roms/game.sav"] = b"forbidden"
            with zipfile.ZipFile(bad, "w") as archive:
                for name, data in content.items():
                    archive.writestr(name, data)
                if variant == "duplicate":
                    archive.writestr("STATE.INI", content["state.ini"])
            with self.assertRaises(ValueError):
                config.import_configuration(self.target, bad)
            self.assertFalse((self.target / config.OWNER / "state.ini").exists())

    def test_failed_import_rolls_back_all_records(self):
        target = config.private_directory(self.target)
        original = b"version=1\nicon_size=0\n"
        (target / "state.ini").write_bytes(original)
        real_write = config.atomic_write
        failed = False

        def fail_second_record(path, data):
            nonlocal failed
            if not failed and path == target / "state.ini":
                failed = True
                raise OSError("simulated SD write failure")
            return real_write(path, data)

        with patch.object(config, "atomic_write", side_effect=fail_second_record):
            with self.assertRaises(OSError):
                config.import_configuration(self.target, self.archive)
        self.assertEqual((target / "state.ini").read_bytes(), original)
        self.assertFalse((target / "games/0123456789abcdef.ini").exists())

    def test_interrupted_import_recovery_and_external_edit_protection(self):
        target = config.private_directory(self.target)
        (target / "state.ini").write_text("version=1\nold=1\n")
        backup = config.import_configuration(self.target, self.archive)
        journal = json.loads((backup / "transaction.json").read_text())
        journal["state"] = "prepared"  # Simulate power loss before completion marker.
        config.write_journal(backup, journal)
        (target / "state.ini").write_text("external edit\n")
        with self.assertRaises(ValueError):
            config.recover_configuration(self.target)
        self.assertEqual((target / "state.ini").read_text(), "external edit\n")
        (target / "state.ini").write_bytes((self.private / "state.ini").read_bytes())
        self.assertEqual(config.recover_configuration(self.target), 1)
        self.assertEqual((target / "state.ini").read_text(), "version=1\nold=1\n")
        self.assertFalse((target / "games/0123456789abcdef.ini").exists())

    def test_redirected_record_is_rejected(self):
        target = config.private_directory(self.target)
        outside = self.root / "outside.ini"
        outside.write_text("leave me alone\n")
        (target / "state.ini").symlink_to(outside)
        with self.assertRaises(ValueError):
            config.import_configuration(self.target, self.archive)
        self.assertEqual(outside.read_text(), "leave me alone\n")
