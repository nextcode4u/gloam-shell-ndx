"""Licensed theme recordings stay attributable, bounded and safe to loop."""

from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from prepare_theme_music import circular_loop, verify


class ThemeMusicTests(unittest.TestCase):
    def test_reviewed_recordings_and_notices(self):
        manifest = json.loads((ROOT / "config/theme-music.json").read_text())
        for theme, entry in manifest["themes"].items():
            with self.subTest(theme=theme):
                folder = ROOT / manifest["theme_root"] / theme
                path = folder / "sounds/loop.wav"
                verify(path, entry)
                self.assertIn(entry["title"], (folder / "MUSIC.md").read_text())
                self.assertIn(entry["source_url"], (folder / "MUSIC.md").read_text())
                self.assertEqual(
                    hashlib.sha256(
                        (folder / "MUSIC-LICENSE.txt").read_bytes()
                    ).hexdigest(),
                    manifest["license_text_sha256"],
                )
                with wave.open(str(path)) as stream:
                    self.assertGreater(stream.getnframes() / stream.getframerate(), 15)
                    values = array("h", stream.readframes(stream.getnframes()))
                self.assertEqual(len(values) % 2, 0)
                if sys.byteorder != "little":
                    values.byteswap()
                rms = (
                    math.sqrt(sum(value * value for value in values) / len(values))
                    / 32768
                )
                self.assertGreater(rms, 0.02)
                self.assertLess(max(abs(value) for value in values), 30000)
                self.assertLess(abs(values[0] - values[-1]), 512)
        for theme, entry in manifest.get("retained", {}).items():
            verify(ROOT / manifest["theme_root"] / theme / "sounds/loop.wav", entry)

    def test_circular_join_keeps_length_and_avoids_silence_fades(self):
        source = array("f", (0.2 * math.sin(index / 20) for index in range(1200)))
        loop = circular_loop(source, 1000, 100)
        self.assertEqual(len(loop), 1000)
        self.assertLessEqual(max(abs(value) for value in loop), 0.201)
        self.assertGreater(sum(value * value for value in loop), 1)
        with self.assertRaises(ValueError):
            circular_loop(source, 1200, 100)


if __name__ == "__main__":
    unittest.main()
