"""Check the SD distribution layout and preservation of other applications."""

import json
import tempfile
import unittest
from pathlib import Path

from tools.stage_sd import stage_sd


class SdStagingTests(unittest.TestCase):
    def test_standard_layout_preserves_existing_homebrew_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            rom = root / "frontend.nds"
            rom.write_bytes(b"test build artifact")
            sd = root / "sd"
            (sd / "_nds").mkdir(parents=True)
            bootstrap_config = sd / "_nds/nds-bootstrap.ini"
            bootstrap_config.write_text("existing application configuration")
            (sd / "_gba").mkdir()
            bios = sd / "_gba/bios.bin"
            bios.write_bytes(b"test sentinel, not a BIOS")

            stage_sd(sd, rom)

            self.assertEqual((sd / "BOOT.NDS").read_bytes(), rom.read_bytes())
            for directory in (
                "_nds/dsi-frontend",
                "_nds/dsi-frontend/plugins",
                "_nds/dsi-frontend/plugins/icons",
                "_nds/dsi-frontend/plugins/apps",
                "_gba",
                "roms/Nintendo DS",
                "roms/Homebrew Games",
                "apps/Homebrew Apps",
                "roms/Game Boy Advance",
                "media/books",
                "media/music",
                "media/videos",
                "data",
            ):
                self.assertTrue((sd / directory).is_dir(), directory)
            self.assertEqual(bootstrap_config.read_text(), "existing application configuration")
            self.assertEqual(bios.read_bytes(), b"test sentinel, not a BIOS")
            self.assertFalse(list(sd.rglob(".gitkeep")))
            self.assertFalse((sd / "music").exists())
            self.assertEqual({p.name for p in (sd / "apps").iterdir()}, {"Homebrew Apps"})
            self.assertEqual(
                {folder.name for folder in (sd / "roms").iterdir()},
                {
                    Path(folder).parts[1]
                    for folder in json.loads(
                        (
                            Path(__file__).resolve().parents[1] / "config/emulator-presets.json"
                        ).read_text()
                    )["rom_directories"]
                } | {"README.txt"},
            )

            # User tab definitions survive ordinary restaging.
            tab_config = sd / "_nds/dsi-frontend/tabs.ini"
            self.assertTrue(tab_config.is_file())
            custom_tabs = "[tabs]\nversion=1\n[tab.mine]\nlabel=My games\npath=roms/NES\n"
            tab_config.write_text(custom_tabs)

            # Restaging preserves an existing library in the older location.
            (sd / "music").mkdir()
            track = sd / "music/existing.mp3"
            track.write_bytes(b"existing library sentinel")
            stage_sd(sd, rom)
            self.assertEqual(track.read_bytes(), b"existing library sentinel")
            self.assertEqual(tab_config.read_text(), custom_tabs)

