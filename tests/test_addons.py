"""Keep distribution decisions explicit and enforce archive/hash checks."""

import copy
import io
import json
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from tools.build_addons import addon_files, archive_members, gba_config_files, selected_applications, download_manifest
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

    def test_gba_config_allowlist_does_not_import_firmware_or_unlisted_members(self):
        buffer = io.BytesIO()
        config = b'{"runSettings": {}}'
        name = "_gba/configs/ABCE01.json"
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr(name, config)
            archive.writestr("_gba/bios.bin", b"Synthetic forbidden firmware fixture")
            archive.writestr("GBARunner3.nds", self.binary)
        blob = buffer.getvalue()
        self.assertEqual(gba_config_files(blob, {name: sha256(config)}), {name: config})
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            gba_config_files(blob, {name: "0" * 64})
        for unsafe in ["_gba/bios.bin", "../ABCE01.json", "_gba/configs/custom.json"]:
            with self.assertRaisesRegex(ValueError, "config path"):
                gba_config_files(blob, {unsafe: sha256(config)})
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr(name, b"[]")
        with self.assertRaisesRegex(ValueError, "JSON object"):
            gba_config_files(buffer.getvalue(), {name: sha256(b"[]")})

    def test_reference_source_is_not_labelled_as_corresponding_source(self):
        self.app["package"]["source_role"] = "reference-only"
        self.write_lock()
        _, sources, _, _ = addon_files(self.root, offline=True)
        self.assertEqual(sources["dependencies/addons/demo/reference-source.tar.gz"], self.source)
        self.assertNotIn("dependencies/addons/demo/source.tar.gz", sources)

    def test_gba_configs_require_the_right_application_and_no_path_overlap(self):
        name = "_gba/configs/ABCE01.json"
        config = b"{}"
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr(name, config)
        blob = buffer.getvalue()
        (self.cache / "gba.zip").write_bytes(blob)
        self.app["package"]["game_configs"] = {
            "download": {"asset": "gba.zip", "url": "https://example.invalid/gba",
                         "sha256": sha256(blob)}, "files": {name: sha256(config)}}
        self.write_lock()
        with self.assertRaisesRegex(ValueError, "another application"):
            addon_files(self.root, offline=True)
        self.app["id"] = "gbarunner3"
        self.write_lock()
        runtime, _, _, _ = addon_files(self.root, offline=True)
        self.assertEqual(runtime[name], config)
        self.lock["applications"].append({**self.app, "id": "other"})
        self.write_lock()
        with self.assertRaisesRegex(ValueError, "overlapping"):
            addon_files(self.root, offline=True)

    def test_constituent_review_keeps_noncommercial_gates_and_source_pins(self):
        root = Path(__file__).resolve().parents[1]
        lock = json.loads((root / "config/addon-dependencies.json").read_text())
        normal = {app["id"] for app in selected_applications(lock)}
        opted_in = {app["id"] for app in selected_applications(lock, noncommercial=True)}
        conditional = {"colecods"}
        self.assertFalse(normal & conditional)
        self.assertTrue(conditional <= opted_in)
        self.assertNotIn("nesds", normal)
        self.assertNotIn("ds994a", opted_in)
        self.assertNotIn("picodrivetwl", opted_in)
        nesds = next(app for app in lock["applications"] if app["id"] == "nesds")
        pins = {source["path"]: source["commit"]
                for source in nesds["package"]["extra_sources"]}
        self.assertEqual(pins, {
            "submodules/arm9/source/RP2A03.tar.gz":
                "d4b8d3dffaf9f0eced4cf707a2ce2208eb1bdaa0",
            "submodules/arm9/source/RP2A03/ARM6502.tar.gz":
                "8b8b6c82ce29bd26cd76709c75167372b3711484",
        })
        self.assertNotIn("tuna-vids", normal)
        tuna = next(app for app in lock["applications"] if app["id"] == "tuna-vids")
        self.assertFalse(tuna["permission_confirmed"])
        self.assertEqual(tuna["package"]["project_notices"], [{
            "path": "docs/guides/TUNA_VIDS_NOTICES.md", "destination": "DISTRIBUTION.md"}])

    def test_hachibitto_is_download_only_and_requires_noncommercial_opt_in(self):
        root = Path(__file__).resolve().parents[1]
        lock = json.loads((root / "config/addon-dependencies.json").read_text())
        bundled = {a["id"] for a in selected_applications(lock, noncommercial=True)}
        self.assertNotIn("hachibitto", bundled)
        with self.assertRaisesRegex(ValueError, "not eligible"):
            selected_applications(lock, noncommercial=True, only=["hachibitto"])
        manifest = download_manifest(lock, bundled)
        app = next(a for a in manifest["applications"] if a["id"] == "hachibitto")
        self.assertTrue(app["noncommercial"])
        recipe = next(a for a in lock["applications"] if a["id"] == "hachibitto")
        self.assertEqual(app["version"], recipe["version"])
        self.assertNotIn("source", app)
        self.assertNotIn("notices", app)
        self.assertEqual(app["files"][0]["destination"],
                         "_nds/dsi-frontend/plugins/apps/Hachibitto.nds")

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

    def test_unconfirmed_packages_cannot_be_bundled_even_by_explicit_selection(self):
        for status in ["download-only", "assumed", "approved", "noncommercial"]:
            self.app.update(redistribution=status, permission_confirmed=False)
            with self.assertRaises(ValueError):
                selected_applications(self.lock, noncommercial=True, only=["demo"])

    def test_missing_binary_is_not_invented(self):
        del self.app["package"]
        with self.assertRaisesRegex(ValueError, "No pinned binary"):
            selected_applications(self.lock, only=["demo"])

    def test_download_catalog_is_metadata_only_and_excludes_bundled_apps(self):
        root = Path(__file__).resolve().parents[1]
        lock = json.loads((root / "config/addon-dependencies.json").read_text())
        bundled = {a["id"] for a in selected_applications(lock, noncommercial=True)}
        catalog = download_manifest(lock, bundled)
        ready = {a["id"] for a in catalog["applications"]}
        self.assertEqual(len(bundled), 15)
        self.assertEqual(len(ready), 14)
        self.assertFalse(ready & bundled)
        self.assertEqual(len(catalog["updates"]), 29)
        payload = next(a for a in catalog["updates"] if a["id"] == "snemulds")
        self.assertEqual(payload["requires"], ["snemulds", "snemulds-shim"])
        self.assertTrue({"gbarunner3", "nesds", "tuna-vids", "ds994a"} <= ready)
        for app in catalog["applications"]:
            self.assertNotIn("source", app)
            self.assertNotIn("notices", app)
        self.assertTrue(next(a for a in catalog["applications"] if a["id"] == "ds994a")["noncommercial"])

    def test_submodule_source_and_notices_preserve_the_pinned_bytes(self):
        blob = source_tar({"component/LICENSE": b"Synthetic component notice"})
        (self.cache / "component.tar.gz").write_bytes(blob)
        self.app["package"]["extra_sources"] = [{
            "download": {"asset": "component.tar.gz", "url": "https://example.invalid/component",
                         "sha256": sha256(blob)},
            "path": "submodules/lib/component.tar.gz",
            "notices": [{"member": "component/LICENSE", "destination": "LICENSE"}],
        }]
        self.write_lock()
        _, sources, notices, _ = addon_files(self.root, offline=True)
        self.assertEqual(sources["dependencies/addons/demo/submodules/lib/component.tar.gz"], blob)
        self.assertEqual(notices["addons/demo/submodules/lib/component/LICENSE"],
                         b"Synthetic component notice")

    def test_project_distribution_notice_is_bundled_without_overwriting_upstream(self):
        document = self.root / "docs/distribution.md"
        document.parent.mkdir()
        document.write_bytes(b"Synthetic source location and recorded distribution assumption")
        self.app["package"]["project_notices"] = [{
            "path": "docs/distribution.md", "destination": "DISTRIBUTION.md"}]
        self.write_lock()
        runtime, _, notices, _ = addon_files(self.root, offline=True)
        self.assertEqual(notices["addons/demo/DISTRIBUTION.md"], document.read_bytes())
        self.assertEqual(notices["addons/demo/LICENSE"], b"Synthetic permissive fixture notice")
        self.assertNotIn("addons/demo/DISTRIBUTION.md", runtime)
        self.app["package"]["project_notices"][0]["destination"] = "LICENSE"
        self.write_lock()
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            addon_files(self.root, offline=True)
        self.app["package"]["project_notices"][0]["destination"] = "../DISTRIBUTION.md"
        self.write_lock()
        with self.assertRaises(ValueError):
            addon_files(self.root, offline=True)

    def test_release_zip_notices_do_not_enter_the_runtime_overlay(self):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr("demo.nds", self.binary)
            archive.writestr("README.md", b"Synthetic author attribution")
            archive.writestr("__MACOSX/._README.md", b"Unneeded archive metadata")
        blob = buffer.getvalue()
        (self.cache / "release.zip").write_bytes(blob)
        download = {"asset": "release.zip", "url": "https://example.invalid/release",
                    "sha256": sha256(blob)}
        self.app["package"]["files"][0].update(download=download, format="zip", member="demo.nds")
        self.app["package"]["runtime_notices"] = [{"download": download, "format": "zip",
            "member": "README.md", "destination": "release/README.md"}]
        self.write_lock()
        runtime, _, notices, _ = addon_files(self.root, offline=True)
        self.assertEqual(runtime, {"apps/demo.nds": self.binary})
        self.assertEqual(notices["addons/demo/release/README.md"], b"Synthetic author attribution")
        self.assertFalse(any("__MACOSX" in name for name in runtime))

    def test_shim_dependency_cannot_bypass_permission_review(self):
        shim = {**self.app, "id": "snemulds-shim"}
        payload = {**self.app, "id": "snemulds"}
        self.lock["applications"] = [shim, payload]
        self.assertEqual([a["id"] for a in selected_applications(self.lock, only=[shim["id"]])],
                         ["snemulds", "snemulds-shim"])
        payload["redistribution"] = "blocked"
        with self.assertRaisesRegex(ValueError, "eligible SNEmulDS"):
            selected_applications(self.lock, only=[shim["id"]])
        payload.update(redistribution="approved", permission_confirmed=False)
        with self.assertRaisesRegex(ValueError, "eligible SNEmulDS"):
            selected_applications(self.lock, only=[shim["id"]])

    def test_reader_default_profiles_match_the_template_and_stay_scoped(self):
        self.app["id"] = "dslibris"
        profile = "_nds/dsi-frontend/plugins/dslibris-books.ini"
        text = "[plugin]\nversion=1\nextensions=epub\n"
        (self.root/"reader.ini").write_text(text)
        self.app["package"]["defaults"] = [{"path":"reader.ini", "destination":profile,
                                             "content":text, "sha256":sha256(text.encode())}]
        self.write_lock()
        runtime, _, _, _ = addon_files(self.root,offline=True)
        self.assertEqual(runtime[profile],text.encode())
        self.app["package"]["defaults"][0]["content"] = "Unexpected template"
        self.write_lock()
        with self.assertRaisesRegex(ValueError,"reviewed template"):
            addon_files(self.root,offline=True)

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
