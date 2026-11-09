"""Keep the default associations complete and within runtime bounds."""

import configparser
import json
import re
import unittest
from pathlib import Path

from tools.make_emulator_presets import CATALOG, PROJECT_ROOT, generate


class EmulatorPresetTests(unittest.TestCase):
    def test_generated_profiles_preserve_configured_fields(self):
        catalog = json.loads(CATALOG.read_text())
        profiles = catalog["profiles"]
        self.assertEqual(len(profiles), 31)
        limits = (PROJECT_ROOT / "arm9/include/types.h").read_text()
        capacity = int(re.search(r"#define ASSOCIATION_CAP (\d+)", limits).group(1))
        self.assertLessEqual(len(profiles), capacity)
        seen = set()
        for profile in profiles:
            parser = configparser.ConfigParser(interpolation=None)
            path = PROJECT_ROOT / "sdroot/_nds/dsi-frontend/plugins" / (profile["id"] + ".ini")
            parser.read(path)
            fields = parser["plugin"]
            extensions = fields["extensions"].split(",")
            self.assertEqual(extensions, profile["extensions"])
            self.assertFalse(seen.intersection(extensions))
            seen.update(extensions)
            self.assertLess(len(fields["extensions"].encode()), 128)
            for key, value in profile["settings"].items():
                output_key = "application" if key == "path" else key
                expected = "sd:/" + value if key in ("path", "dsipath", "ntrpath") else value
                self.assertEqual(fields[output_key], expected)
            for key in ("path", "dsipath", "ntrpath"):
                if key in profile["settings"]:
                    relative = profile["settings"][key]
                    self.assertNotIn("..", Path(relative).parts)
        self.assertEqual(len(seen), 78)
        self.assertIn("mx2", seen)
        self.assertIn("mxd", seen)
        self.assertIn("epub", seen)
        self.assertIn("fb2.zip", seen)
        self.assertIn("opf", seen)
        self.assertIn("pmlz", seen)
        self.assertIn("tcr", seen)
        self.assertNotIn("bin", seen)

    def test_default_profiles_and_addons_share_frontend_application_directory(self):
        presets = json.loads(CATALOG.read_text())
        addons = json.loads((PROJECT_ROOT / "config/addon-dependencies.json").read_text())
        directory = "_nds/dsi-frontend/plugins/apps"
        self.assertEqual(presets["application_directory"], directory)
        self.assertEqual(addons["application_directory"], directory)
        for profile in presets["profiles"]:
            for key in ("path", "dsipath", "ntrpath"):
                if key in profile["settings"]:
                    self.assertEqual(str(Path(profile["settings"][key]).parent), directory)
        for application in addons["applications"]:
            for item in application.get("package", {}).get("files", []):
                if item["destination"].endswith((".nds", ".srl", ".ini")):
                    self.assertEqual(str(Path(item["destination"]).parent), directory)
                else:
                    self.assertEqual(application["id"], "dslibris")
                    self.assertTrue(item["destination"].startswith("data/dslibris/font/"))

    def test_template_matches_catalog_without_old_collection_aliases(self):
        generate(check=True)
        catalog = json.loads(CATALOG.read_text())
        expected_profiles = {p["id"] + ".ini" for p in catalog["profiles"]}
        actual = {p.name for p in (PROJECT_ROOT / "sdroot/_nds/dsi-frontend/plugins").glob("*.ini")}
        self.assertEqual(actual, expected_profiles)
        for relative in catalog["rom_directories"]:
            self.assertTrue((PROJECT_ROOT / "sdroot" / relative).is_dir(), relative)
        for old in ("gb", "gba", "nds", "Homebrew", "DS(i) Homebrew", "Music", "Videos"):
            self.assertFalse((PROJECT_ROOT / "sdroot/roms" / old).exists(), old)


if __name__ == "__main__":
    unittest.main()
