"""Check the SD distribution layout and preservation of other applications."""

import json
import shutil
from unittest.mock import patch
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


    def test_upgrade_preserves_customizations_and_boot_copy_failure_keeps_old_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sd, rom = root / 'sd', root / 'boot.nds'
            rom.write_bytes(b'First authored build fixture')
            stage_sd(sd, rom)
            custom = {'_nds/dsi-frontend/state.ini': b'personal settings',
                      '_nds/dsi-frontend/plugins/gameyob-dsi.ini': b'personal handler',
                      '_nds/dsi-frontend/themes/copper/theme.ini': b'personal theme',
                      'roms/bios/gbc_bios.bin': b'personal firmware fixture',
                      'roms/Nintendo DS/saves/game.sav': b'personal save fixture'}
            for name, data in custom.items():
                path = sd / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            before = (sd / 'BOOT.NDS').read_bytes()
            rom.write_bytes(b'Updated authored build fixture')
            copy = shutil.copy2
            def fail_boot(source, target, *args, **kwargs):
                if Path(target).name.startswith('.gloam-stage-'):
                    raise OSError('simulated interrupted boot copy')
                return copy(source, target, *args, **kwargs)
            with patch('tools.stage_sd.shutil.copy2', side_effect=fail_boot):
                with self.assertRaises(OSError):
                    stage_sd(sd, rom)
            self.assertEqual((sd / 'BOOT.NDS').read_bytes(), before)
            self.assertFalse(list(sd.glob('.gloam-stage-*')))
            stage_sd(sd, rom)
            self.assertEqual((sd / 'BOOT.NDS').read_bytes(), rom.read_bytes())
            for name, data in custom.items():
                self.assertEqual((sd / name).read_bytes(), data)

    def test_existing_sd_links_are_rejected_before_any_files_are_staged(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sd, outside, rom = root / 'sd', root / 'outside', root / 'boot.nds'
            sd.mkdir()
            outside.mkdir()
            rom.write_bytes(b'Authored build fixture')
            (sd / '_nds').symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'symlink'):
                stage_sd(sd, rom)
            self.assertEqual(list(outside.iterdir()), [])
            self.assertEqual({p.name for p in sd.iterdir()}, {'_nds'})
