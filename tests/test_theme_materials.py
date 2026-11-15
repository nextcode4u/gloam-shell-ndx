"""Material preparation preserves repeat pitch, cutouts and readable native contrast."""

import unittest

from PIL import Image, ImageStat

from tools.theme_materials import (
    MATERIALS,
    material_tile,
    texture_component,
    tinted_material,
)


class MaterialTests(unittest.TestCase):
    def test_focus_changes_the_fill_and_material_not_only_a_leading_marker(self):
        from tools.make_theme_components import THEMES

        for name in MATERIALS:
            self.assertNotEqual(
                material_tile(name).tobytes(), material_tile(name, True).tobytes(), name
            )
            with Image.open(THEMES / name / "components/row.png") as plain, Image.open(
                THEMES / name / "components/row-selected.png"
            ) as selected:
                a = ImageStat.Stat(plain.crop((8, 8, 24, 24)).convert("RGB")).mean
                b = ImageStat.Stat(selected.crop((8, 8, 24, 24)).convert("RGB")).mean
                self.assertGreater(max(abs(x - y) for x, y in zip(a, b)), 8, name)

    def test_each_material_is_independent_and_has_soft_repeat_joins(self):
        tiles = set()
        for name in MATERIALS:
            tile = material_tile(name)
            self.assertEqual(tile.size, (16, 16))
            tiles.add(tile.tobytes())
            self.assertEqual(
                tile.crop((0, 0, 1, 16)).tobytes(), tile.crop((15, 0, 16, 16)).tobytes()
            )
            self.assertEqual(
                tile.crop((0, 0, 16, 1)).tobytes(), tile.crop((0, 15, 16, 16)).tobytes()
            )
        self.assertEqual(len(tiles), 11)

    def test_tint_repeats_at_native_pitch_and_survives_rgb555(self):
        for name in MATERIALS:
            image = tinted_material(name, "#30384c", (48, 32))
            self.assertEqual(
                image.crop((0, 0, 16, 16)).tobytes(),
                image.crop((16, 16, 32, 32)).tobytes(),
            )
            colors = {tuple(c >> 3 for c in p) for p in image.get_flattened_data()}
            self.assertGreaterEqual(len(colors), 3, name)
            # The cloth/paper detail must remain subordinate to bright text.
            self.assertLess(max(max(p) for p in image.get_flattened_data()), 160)

    def test_material_does_not_fill_alpha_cutouts_or_make_fills_translucent(self):
        image = Image.new("RGBA", (32, 32), (50, 60, 70, 255))
        image.paste((0, 0, 0, 0), (8, 8, 24, 24))
        alpha = image.getchannel("A").tobytes()
        textured = texture_component(image, "nocturne", "panel")
        self.assertEqual(textured.getchannel("A").tobytes(), alpha)
        self.assertIsNone(textured.crop((8, 8, 24, 24)).getbbox())
