"""Check the shipped animation, source timing and release inventory together."""

import json
from pathlib import Path
import struct
import unittest
import zlib

from PIL import Image

from tools.build_release import release_notices

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = "sdroot/_nds/dsi-frontend/themes/default/"


class DefaultSplashTests(unittest.TestCase):
    def test_every_frame_and_source_timing(self):
        data = (ROOT / DEFAULT / "splash.gsp").read_bytes()
        self.assertEqual(data[:8], b"GLOMSP03")
        self.assertLess(len(data), 2 * 1024 * 1024)
        self.assertEqual(
            struct.unpack_from("<II", data, 8), (len(data), zlib.crc32(data[16:]))
        )
        self.assertEqual(struct.unpack_from("<HHB", data, 16), (192, 191, 2))
        self.assertEqual(struct.unpack_from("<II", data, 24), (0, 0))
        offset = 160
        for screen, name in enumerate(("top", "Bottom")):
            with Image.open(
                ROOT / "assets" / f"Gloam_Shell_NDX_64frame_20fps_{name}.gif"
            ) as source:
                self.assertEqual(source.size, (256, 192))
                self.assertEqual(source.n_frames, 64)
                for frame in range(64):
                    source.seek(frame)
                    self.assertEqual(source.info["duration"], 50)
            record = 32 + screen * 64
            self.assertEqual(
                struct.unpack_from("<4B4H2h", data, record),
                (screen, 1, 0, 0, 256, 192, 64, 3, 0, 0),
            )
            self.assertEqual(data[record + 49], 1)
            start, size = struct.unpack_from("<II", data, record + 20)
            self.assertEqual(start, offset)
            table = start + 512
            offsets = struct.unpack_from("<65I", data, table)
            self.assertEqual(offsets[0], 260)
            self.assertEqual(offsets[-1], size - 512)
            for first, last in zip(offsets, offsets[1:]):
                self.assertLess(first, last)
                stream = zlib.decompressobj()
                pixels = stream.decompress(
                    data[table + first : table + last], 256 * 192 + 1
                )
                self.assertEqual(len(pixels), 256 * 192)
                self.assertTrue(stream.eof)
                self.assertFalse(stream.unused_data or stream.unconsumed_tail)
            offset = (start + size + 3) & ~3
        self.assertEqual(offset, len(data))

    def test_runtime_and_source_assets_have_release_notices(self):
        self.assertFalse((ROOT / DEFAULT / "theme.ini").exists())
        catalog = json.loads((ROOT / "config/package-inputs.json").read_text())
        for name in ("splash.gsp", "CREDITS.md"):
            self.assertIn(DEFAULT + name, catalog["sd_files"])
            self.assertIn(DEFAULT + name, catalog["source_files"])
        for name in ("top", "Bottom"):
            self.assertIn(
                f"assets/Gloam_Shell_NDX_64frame_20fps_{name}.gif",
                catalog["source_files"],
            )
        notice = (DEFAULT + "CREDITS.md").removeprefix("sdroot/")
        files = {
            name: (ROOT / name).read_bytes()
            for name in ("LICENSE", "CREDITS.md", "THIRD_PARTY.md")
        }
        files[notice] = (ROOT / DEFAULT / "CREDITS.md").read_bytes()
        self.assertIn(notice, release_notices(files, []))


if __name__ == "__main__":
    unittest.main()
