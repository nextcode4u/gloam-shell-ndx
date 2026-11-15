"""Check the authoring converter's on-disk contract and safe input boundaries."""

from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from PIL import Image
from tools.pack_theme_assets import inside, pack, sprite


class ThemeAssetsTests(unittest.TestCase):
    def test_color_and_coverage_layout(self):
        image = Image.new("RGBA", (32, 32), (255, 0, 0, 0))
        for x, alpha in enumerate((0, 85, 170, 255)):
            image.putpixel((x, 0), (255, 0, 0, alpha))
        component = sprite(image)
        self.assertEqual(len(component), 2304)
        self.assertEqual(
            struct.unpack_from("<4H", component), (0, 0x801F, 0x801F, 0x801F)
        )
        self.assertEqual(component[2048], 0b11100100)
        icon = sprite(image, False)
        self.assertEqual(len(icon), 2048)
        self.assertEqual(struct.unpack_from("<4H", icon), (0, 0, 0x801F, 0x801F))

    def test_records_preserve_indices_without_requiring_every_sprite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "theme.ini").write_text("[icons]\ndirectory=icons\n")
            (root / "icons").mkdir()
            Image.new("RGBA", (32, 32), (0, 255, 0, 255)).save(
                root / "icons/folder.png"
            )
            data = pack(root)
            self.assertEqual(data[:8], b"GLMART02")
            raw_size, count = struct.unpack_from("<II", data, 8)
            self.assertEqual((raw_size, count), (2056, 1))
            raw = zlib.decompress(data[16:])
            self.assertEqual(struct.unpack_from("<BBHI", raw), (0, 0, 1, 2048))
            self.assertEqual(raw[8:], struct.pack("<H", 0x83E0) * 1024)
            self.assertEqual(pack(root), data)
            Image.new("RGBA", (64, 32)).save(root / "icons/folder.png")
            with self.assertRaises(ValueError):
                pack(root)

    def test_jpeg_remains_on_native_decoder_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "theme.ini").write_text(
                "[icons]\ndirectory=icons\n[backgrounds]\ntop=photo.jpg\n"
            )
            (root / "icons").mkdir()
            Image.new("RGBA", (32, 32), "red").save(root / "icons/folder.png")
            Image.new("RGB", (256, 192), "blue").save(root / "photo.jpg")
            data = pack(root)
            self.assertEqual(struct.unpack_from("<I", data, 12)[0], 1)

    def test_material_sources_require_opaque_native_geometry(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "theme.ini").write_text("[components]\nmaterial=map.png\n")
            for size, alpha in (((32, 32), 255), ((256, 192), 170)):
                Image.new("RGBA", size, (128, 128, 128, alpha)).save(root / "map.png")
                with self.assertRaises(ValueError):
                    pack(root)

    def test_output_cannot_leave_theme(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            for path in ("../outside.gfx", "/outside.gfx", "sd:/outside.gfx"):
                with self.subTest(path=path), self.assertRaises(ValueError):
                    inside(root, path)


if __name__ == "__main__":
    unittest.main()
