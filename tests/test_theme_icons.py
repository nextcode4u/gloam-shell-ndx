"""Production icon packs stay bounded, reproducible and complete."""
import configparser
import io
import unittest

from PIL import Image
from tools.make_theme_icons import THEMES, assets
from tools.make_theme_components import assets as component_assets, COMPONENTS


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

    def test_modular_components_are_reproducible_and_bounded(self):
        generated = dict(component_assets())
        self.assertEqual(len(generated), 11 * len(COMPONENTS))
        for path, data in generated.items():
            self.assertEqual(path.read_bytes(), data)
            self.assertLessEqual(len(data), 65536)
            with Image.open(io.BytesIO(data)) as image:
                self.assertEqual(image.size, (32, 32))
                self.assertTrue(image.getbbox())
        folders = [Image.open(THEMES / theme / 'icons/folder.png')
                   for theme in ('phosphor','eclipse','prism','copper')]
        # Families differ in shape, not just their palette.
        masks = {image.getchannel('A').tobytes() for image in folders}
        self.assertGreaterEqual(len(masks), 3)
        for image in folders:
            image.close()


if __name__ == '__main__':
    unittest.main()
