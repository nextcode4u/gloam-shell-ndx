"""Archive isolation, reproducibility, integrity and clean-install/upgrade checks."""

import hashlib
import json
import struct
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from tools import stage_sd
from tools.build_forwarder import FILES, TITLE_ID, unsigned_tmd
from tools.build_release import build_release, release_notices
from tools.package_development import package, sha256


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "project"
        self.root.mkdir()
        files = {
            "README.md": b"Development frontend\n",
            "CREDITS.md": b"Original fixture attribution\n",
            "LICENSE": b"Original source GPL-3.0-or-later\n",
            "tools/check-fixture.sh": b"#!/bin/sh\nexit 0\n",
            "THIRD_PARTY.md": b"Third party notices\n",
            "docs/GETTING_STARTED.md": b"Installation\n",
            "docs/CURRENT_SUPPORT.md": b"Development\n[Build tool](../tools/check-fixture.sh)\n",
            "docs/guides/SLOT1_LAUNCH.md": b"Slot-1 helper installation\n",
            "third_party/test/LICENSE": b"Full test license\n",
            "third_party/blocksds/libnds-LICENSE.FatFs": b"Filesystem notice\n",
            "third_party/blocksds/gcc-COPYING.RUNTIME": b"Runtime exception\n",
            "sdroot/_nds/dsi-frontend/plugins/example.ini": b"[plugin]\n",
            "sdroot/apps/Homebrew Apps/Read me.txt": b"Apps\n",
            "sdroot/roms/NES/README.txt": b"Use .nes and .fds files\n",
            "gloam-shell-ndx.nds": bytes(512),
        }
        for name, data in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        catalog = {
            "sd_files": [name for name in files if name.startswith("sdroot/")],
            "source_files": [name for name in files if name != "gloam-shell-ndx.nds"]
            + ["config/package-inputs.json"],
            "sd_directories": ["roms/Homebrew Games", "_nds/dsi-frontend/cache"],
        }
        self.catalog_path = self.root / "config/package-inputs.json"
        self.catalog_path.parent.mkdir()
        self.catalog_path.write_text(json.dumps(catalog))
        manifest = self.root / "dist/build-info.json"
        manifest.parent.mkdir()
        manifest.write_text(
            json.dumps({"sha256": hashlib.sha256(bytes(512)).hexdigest(), "bytes": 512})
        )

    def output(self, name="package"):
        return self.root.parent / name

    def test_reproducible_pair_and_verified_contents(self):
        first = package(self.root, self.output(), "0.1.0-dev.1")
        second = package(self.root, self.output("second"), "0.1.0-dev.1")
        for path in first.iterdir():
            self.assertEqual(path.read_bytes(), (second / path.name).read_bytes())
        with zipfile.ZipFile(first / "gloam-shell-ndx-0.1.0-dev.1-sd.zip") as archive:
            manifest = json.loads(archive.read("package-manifest.json"))
            self.assertEqual(manifest["original_source_license"], "GPL-3.0-or-later")
            self.assertEqual(archive.read("LICENSE"), b"Original source GPL-3.0-or-later\n")
            self.assertEqual(
                archive.read("docs/notices/third_party/blocksds/libnds-LICENSE.FatFs"),
                b"Filesystem notice\n",
            )
            self.assertEqual(
                archive.read("docs/notices/third_party/blocksds/gcc-COPYING.RUNTIME"),
                b"Runtime exception\n",
            )
            for name, checksum in manifest["sd_files"].items():
                self.assertEqual(hashlib.sha256(archive.read(name)).hexdigest(), checksum)
            self.assertIn("roms/Homebrew Games/", archive.namelist())
            self.assertEqual(archive.read("BOOT.NDS"), bytes(512))
            self.assertEqual(archive.read("README.md"), b"Development frontend\n")
            self.assertEqual(archive.read("THIRD_PARTY.md"), b"Third party notices\n")
            self.assertEqual(
                archive.read("docs/CURRENT_SUPPORT.md"),
                b"Development\nBuild tool (source package: `tools/check-fixture.sh`)\n",
            )
            self.assertEqual(
                archive.read("docs/guides/SLOT1_LAUNCH.md"), b"Slot-1 helper installation\n"
            )
        with zipfile.ZipFile(first / "gloam-shell-ndx-0.1.0-dev.1-source.zip") as archive:
            self.assertIn("third_party/test/LICENSE", archive.namelist())
            self.assertEqual(
                archive.read("docs/CURRENT_SUPPORT.md"),
                b"Development\n[Build tool](../tools/check-fixture.sh)\n",
            )
            self.assertEqual(
                (archive.getinfo("tools/check-fixture.sh").external_attr >> 16) & 0o777, 0o755
            )

    def forwarder(self):
        rom = bytearray(0x4000)
        rom[12:16] = b"GLDX"
        rom[0x12] = 3
        struct.pack_into("<Q", rom, 0x230, TITLE_ID)
        files = {FILES[0]: bytes(rom), FILES[1]: unsigned_tmd(rom)}
        directory = self.root / "dist/forwarder"
        directory.mkdir()
        for name, data in files.items():
            (directory / name).write_bytes(data)
        (directory / "manifest.json").write_text(
            json.dumps({"sha256": {n: hashlib.sha256(b).hexdigest() for n, b in files.items()}})
        )
        catalog = json.loads(self.catalog_path.read_text())
        catalog["include_forwarder"] = True
        self.catalog_path.write_text(json.dumps(catalog))
        return files

    def test_forwarder_is_verified_and_only_enters_sd_archive(self):
        files = self.forwarder()
        directory = self.root / "dist/forwarder"
        output = package(self.root, self.output(), "0.1.0-dev.1")
        path = "_nds/dsi-frontend/install/" + FILES[0]
        with zipfile.ZipFile(output / "gloam-shell-ndx-0.1.0-dev.1-sd.zip") as archive:
            self.assertEqual(archive.read(path), files[FILES[0]])
        with zipfile.ZipFile(output / "gloam-shell-ndx-0.1.0-dev.1-source.zip") as archive:
            self.assertNotIn(path, archive.namelist())
        (directory / FILES[0]).write_bytes(bytes(0x4000))
        with self.assertRaisesRegex(ValueError, "artifact changed"):
            package(self.root, self.output("tampered"), "0.1.0-dev.1")

    def test_private_template_addition_rejected(self):
        (self.root / "sdroot/bios.bin").write_bytes(b"private")
        with self.assertRaisesRegex(ValueError, "reviewed"):
            package(self.root, self.output(), "0.1.0-dev")
        self.assertFalse(self.output().exists())

    def test_stale_manifest_and_existing_destination_rejected(self):
        (self.root / "gloam-shell-ndx.nds").write_bytes(b"changed" * 100)
        with self.assertRaisesRegex(ValueError, "stale"):
            package(self.root, self.output(), "0.1.0-dev")
        self.output().mkdir()
        sentinel = self.output() / "keep"
        sentinel.write_bytes(b"existing")
        with self.assertRaisesRegex(ValueError, "already exists"):
            package(self.root, self.output(), "0.1.0-dev")
        self.assertEqual(sentinel.read_bytes(), b"existing")

    def test_symlink_and_traversal_rejected(self):
        path = self.root / "sdroot/apps/Homebrew Apps/Read me.txt"
        path.unlink()
        path.symlink_to(self.root / "README.md")
        with self.assertRaisesRegex(ValueError, "symlink"):
            package(self.root, self.output(), "0.1.0-dev")
        path.unlink()
        path.write_bytes(b"Apps\n")
        catalog = json.loads(self.catalog_path.read_text())
        catalog["source_files"].append("../private")
        self.catalog_path.write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "Invalid"):
            package(self.root, self.output(), "0.1.0-dev")

    def test_clean_install_and_upgrade_preserve_user_files(self):
        output = package(self.root, self.output(), "0.1.0-dev")
        card = self.output("card")
        with zipfile.ZipFile(output / "gloam-shell-ndx-0.1.0-dev-sd.zip") as archive:
            archive.extractall(card)
        original = stage_sd.SD_TEMPLATE
        self.addCleanup(setattr, stage_sd, "SD_TEMPLATE", original)
        stage_sd.SD_TEMPLATE = self.root / "sdroot"
        user = card / "_nds/dsi-frontend/plugins/example.ini"
        user.write_bytes(b"my configuration")
        save = card / "roms/my-game.sav"
        save.write_bytes(b"earned progress")
        rom = self.root / "new-frontend.nds"
        rom.write_bytes(b"next build" * 100)
        stage_sd.stage_sd(card, rom)
        self.assertEqual(user.read_bytes(), b"my configuration")
        self.assertEqual(save.read_bytes(), b"earned progress")
        self.assertEqual((card / "BOOT.NDS").read_bytes(), rom.read_bytes())

    def test_archive_directory_traversal_rejected(self):
        catalog = json.loads(self.catalog_path.read_text())
        catalog["sd_directories"].append("../private")
        self.catalog_path.write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "Invalid"):
            package(self.root, self.output(), "0.1.0-dev")

    def test_private_source_tree_rejected_even_if_added_to_catalog(self):
        private = self.root / "backups/private.txt"
        private.parent.mkdir()
        private.write_bytes(b"private")
        catalog = json.loads(self.catalog_path.read_text())
        catalog["source_files"].append("backups/private.txt")
        self.catalog_path.write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "Private/generated"):
            package(self.root, self.output(), "0.1.0-dev")

    def test_release_update_binary_notices_and_checksums(self):
        lock = self.root / "config/release-dependencies.json"
        forwarder = self.forwarder()
        checkout = self.root / "build/release-sources/test"
        checkout.mkdir(parents=True)
        (checkout / "LICENSE").write_bytes(b"Full test license\n")
        (checkout / "build.sh").write_bytes(b"#!/bin/sh\nexit 0\n")
        (checkout / "build.sh").chmod(0o755)
        def git(*args):
            return subprocess.check_output(["git", "-C", str(checkout), *args], text=True).strip()
        git("init", "-q")
        git("add", ".")
        git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
            "commit", "-qm", "Synthetic backend source")
        cache = self.root / "build/release-cache"
        cache.mkdir()
        backend = cache / "backend.zip"
        with zipfile.ZipFile(backend, "w") as archive:
            archive.writestr("loader.nds", b"synthetic backend")
        lock.write_text(json.dumps({"format": 1, "dependencies": [{
            "id": "test", "tag": "fixture", "commit": git("rev-parse", "HEAD"),
            "asset": "backend.zip", "sha256": hashlib.sha256(backend.read_bytes()).hexdigest(),
            "files": [{"member": "loader.nds", "destination": "_nds/loader.nds",
                       "sha256": hashlib.sha256(b"synthetic backend").hexdigest()}],
        }]}))
        # An existing output from an earlier build must not remain uploadable.
        (self.root / "dist/dependencies-source.zip").write_bytes(b"old release")
        build_release(self.root, offline=True)
        dist = self.root / "dist"
        binary = (dist / "BOOT.NDS").read_bytes()
        self.assertEqual(binary, (self.root / "gloam-shell-ndx.nds").read_bytes())
        with zipfile.ZipFile(dist / "sd.zip") as archive:
            self.assertEqual(binary, archive.read("BOOT.NDS"))
            for name, data in forwarder.items():
                self.assertEqual(archive.read("_nds/dsi-frontend/install/" + name), data)
            self.assertEqual(archive.read("_nds/loader.nds"), b"synthetic backend")
            self.assertEqual({n for n in archive.namelist() if "/" not in n}, {"BOOT.NDS"})
            self.assertFalse(any(n.startswith(("docs/", "dependencies/"))
                                 or n.lower().endswith(".md") for n in archive.namelist()))
            self.assertNotIn("apps/Homebrew Apps/Read me.txt", archive.namelist())
            self.assertEqual(archive.read("roms/NES/README.txt"), b"Use .nes and .fds files\n")
            self.assertEqual(archive.read("_nds/dsi-frontend/plugins/example.ini"), b"[plugin]\n")
        with zipfile.ZipFile(dist / "licenses.zip") as archive:
            self.assertEqual(archive.read("LICENSE"), b"Original source GPL-3.0-or-later\n")
            self.assertIn(b"source.zip", archive.read("DEPENDENCY_SOURCES.txt"))
            self.assertNotIn(b"dependencies-source.zip", archive.read("DEPENDENCY_SOURCES.txt"))
            self.assertIn("CREDITS.md", archive.namelist())
            self.assertIn("THIRD_PARTY.md", archive.namelist())
            self.assertEqual(archive.read("docs/notices/third_party/test/LICENSE"), b"Full test license\n")
        with zipfile.ZipFile(dist / "source.zip") as archive:
            self.assertEqual(archive.read("dependencies/test/LICENSE"), b"Full test license\n")
            script = archive.getinfo("dependencies/test/build.sh")
            self.assertEqual(script.external_attr >> 16, 0o100755)
            source_manifest = json.loads(archive.read("dependencies/source-manifest.json"))
            self.assertEqual(source_manifest["dependencies"][0]["commit"], git("rev-parse", "HEAD"))
            self.assertEqual(archive.read("README.md"), b"Development frontend\n")
            manifest = json.loads(archive.read("package-manifest.json"))
            self.assertEqual(manifest["source_archive"]["file"], "source.zip")
            self.assertEqual(manifest["update_artifact"]["sha256"], hashlib.sha256(binary).hexdigest())
            self.assertEqual(manifest["license_archive"]["sha256"],
                             hashlib.sha256((dist / "licenses.zip").read_bytes()).hexdigest())
            self.assertEqual(archive.read("docs/GETTING_STARTED.md"), b"Installation\n")
            for name, checksum in manifest["source_files"].items():
                self.assertEqual(hashlib.sha256(archive.read(name)).hexdigest(), checksum)
        self.assertFalse((dist / "dependencies-source.zip").exists())
        sums = (dist / "SHA256SUMS").read_text().splitlines()
        expected = {"BOOT.NDS", "sd.zip", "licenses.zip", "source.zip"}
        self.assertEqual({line.split()[1] for line in sums}, expected)
        for line in sums:
            checksum, name = line.split()
            self.assertEqual(hashlib.sha256((dist / name).read_bytes()).hexdigest(), checksum)

    def test_optional_addon_overlay_keeps_matching_companions(self):
        self.forwarder()
        (self.root / "config/release-dependencies.json").write_text(
            json.dumps({"format": 1, "dependencies": []})
        )
        runtime = {"apps/demo.nds": b"Synthetic application"}
        source = {"dependencies/addons/demo/source.tar.gz": b"Synthetic source archive"}
        notices = {"addons/demo/LICENSE": b"Synthetic retained notice",
                   "addons/UNCONFIRMED_DISTRIBUTION.md": b"Synthetic unconfirmed assumption"}
        report = {"format": 1, "applications": [{"id": "demo", "redistribution": "assumed",
                                                "permission_confirmed": False}],
                  "noncommercial": False, "assumed": ["demo"]}
        with patch("tools.build_addons.addon_files", return_value=(runtime, source, notices, report)):
            build_release(self.root, offline=True, addons=True)
        dist = self.root / "dist"
        with zipfile.ZipFile(dist / "sd.zip") as base, zipfile.ZipFile(dist / "addons.zip") as overlay:
            self.assertNotIn("apps/demo.nds", base.namelist())
            self.assertEqual(overlay.read("apps/demo.nds"), b"Synthetic application")
            self.assertEqual(overlay.read("roms/NES/README.txt"), b"Use .nes and .fds files\n")
            self.assertNotIn("LICENSE", overlay.namelist())
        with zipfile.ZipFile(dist / "licenses.zip") as archive:
            self.assertEqual(archive.read("addons/demo/LICENSE"), b"Synthetic retained notice")
            self.assertEqual(archive.read("addons/UNCONFIRMED_DISTRIBUTION.md"),
                             b"Synthetic unconfirmed assumption")
        with zipfile.ZipFile(dist / "source.zip") as archive:
            self.assertEqual(archive.read("dependencies/addons/demo/source.tar.gz"),
                             b"Synthetic source archive")
            manifest = json.loads(archive.read("package-manifest.json"))
            self.assertEqual(manifest["addons"]["sha256"], sha256((dist / "addons.zip").read_bytes()))
            self.assertEqual(manifest["addons"]["applications"], ["demo"])
            self.assertEqual(manifest["addons"]["assumed"], ["demo"])
        self.assertIn("  addons.zip", (dist / "SHA256SUMS").read_text())
        # A later base-only build must not retain an add-on with stale companions.
        build_release(self.root, offline=True)
        self.assertFalse((dist / "addons.zip").exists())
        self.assertNotIn("addons.zip", (dist / "SHA256SUMS").read_text())

    def test_release_requires_bundled_forwarder(self):
        (self.root / "config/release-dependencies.json").write_text(
            json.dumps({"format": 1, "dependencies": []})
        )
        with self.assertRaisesRegex(ValueError, "missing the forwarder"):
            build_release(self.root, offline=True)

    def test_release_rejects_missing_and_empty_notices(self):
        notices = {"LICENSE": b"GPL", "CREDITS.md": b"Authors", "THIRD_PARTY.md": b"Inventory",
                   "docs/notices/third_party/example/COPYING": b"Full license"}
        self.assertEqual(release_notices(notices, [{"id": "example"}]), notices)
        with self.assertRaisesRegex(ValueError, "Missing dependency license"):
            release_notices(notices, [{"id": "missing"}])
        for name in ("LICENSE", "CREDITS.md", "THIRD_PARTY.md"):
            incomplete = dict(notices)
            del incomplete[name]
            with self.assertRaisesRegex(ValueError, "missing its license"):
                release_notices(incomplete, [])
        notices["docs/notices/third_party/example/COPYING"] = b""
        with self.assertRaisesRegex(ValueError, "Empty release notice"):
            release_notices(notices, [{"id": "example"}])
