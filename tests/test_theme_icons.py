"""Production icon packs stay bounded, reproducible and complete."""
import configparser
import io
import unittest

from PIL import Image
from tools.make_theme_icons import THEMES, assets


class ThemeIconTests(unittest.TestCase):
    def test_shipped_packs(self):
        generated = dict(assets())
        self.assertEqual(len(generated), 11 * 22)
        for manifest in THEMES.glob('*/theme.ini'):
            config = configparser.ConfigParser()
            config.read(manifest)
            self.assertEqual(config['icons']['directory'], 'icons')
        for path, data in generated.items():
            with self.subTest(path=str(path)):
                self.assertEqual(path.read_bytes(), data)
                self.assertLessEqual(len(data), 65536)
                with Image.open(io.BytesIO(data)) as image:
                    self.assertEqual(image.size, (32, 32))
                    alpha = image.getchannel('A')
                    self.assertEqual(alpha.getextrema(), (0, 255))
                    self.assertTrue(image.getbbox())


if __name__ == '__main__':
    unittest.main()
