"""Production icon packs stay bounded, reproducible and complete."""
import configparser
import io
import unittest

from PIL import Image
from tools.make_theme_icons import THEMES, assets
from tools.make_theme_components import assets as component_assets, COMPONENTS
from tools.make_theme_scenes import assets as scene_assets
from tools.make_theme_navigation import TAB_NAMES


class ThemeIconTests(unittest.TestCase):
    def test_shipped_packs(self):
        generated = dict(assets())
        self.assertEqual(len(generated), 11 * (22 + len(TAB_NAMES)))
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

    def test_navigation_silhouettes_and_small_gears(self):
        for kind in TAB_NAMES:
            with Image.open(THEMES / 'copper/icons' / (kind + '.png')) as copper, \
                 Image.open(THEMES / 'prism/icons' / (kind + '.png')) as prism:
                self.assertNotEqual(copper.getchannel('A').tobytes(),
                                    prism.getchannel('A').tobytes(), kind)
        for manifest in THEMES.glob('*/theme.ini'):
            for kind in ('system', 'settings', 'tab-system'):
                with Image.open(manifest.parent / 'icons' / (kind + '.png')) as image:
                    for size in (16, 20, 24, 32):
                        alpha = image.resize((size, size), Image.Resampling.NEAREST).getchannel('A')
                        self.assertEqual(alpha.getpixel((size // 2, size // 2)), 0)
                        self.assertGreater(sum(value > 0 for value in alpha.get_flattened_data()), size * 3)

    def test_scene_backgrounds_are_distinct_and_reproducible(self):
        generated = dict(scene_assets())
        self.assertEqual(len(generated), 22)
        self.assertEqual(len(set(generated.values())), 22)
        for path, data in generated.items():
            self.assertEqual(path.read_bytes(), data)
            with Image.open(io.BytesIO(data)) as image:
                self.assertEqual(image.size, (256, 192))

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
