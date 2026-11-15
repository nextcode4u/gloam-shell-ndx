"""Material preparation preserves repeat pitch, cutouts and readable native contrast."""

import unittest

from PIL import Image, ImageStat

from tools.theme_materials import (
    MATERIALS,
    material_image,
    material_field,
    tinted_material,
)


class MaterialTests(unittest.TestCase):
    def test_focus_changes_the_fill_and_material_not_only_a_leading_marker(self):
        from tools.make_theme_components import THEMES

        for name in MATERIALS:
            self.assertNotEqual(
                material_image(name).tobytes(),
                material_image(name, True).tobytes(),
                name,
            )
            with Image.open(THEMES / name / "components/row.png") as plain, Image.open(
                THEMES / name / "components/row-selected.png"
            ) as selected:
                a = ImageStat.Stat(plain.crop((8, 8, 24, 24)).convert("RGB")).mean
                b = ImageStat.Stat(selected.crop((8, 8, 24, 24)).convert("RGB")).mean
                self.assertGreater(max(abs(x - y) for x, y in zip(a, b)), 8, name)

    def test_materials_use_full_native_images_instead_of_repeated_patches(self):
        images = set()
        for name in MATERIALS:
            image = material_image(name)
            self.assertEqual(image.size, (256, 192))
            images.add(image.tobytes())
            for x in (16, 32, 64):
                self.assertNotEqual(
                    image.crop((0, 0, 16, 16)).tobytes(),
                    image.crop((x, 0, x + 16, 16)).tobytes(),
                    name,
                )
        self.assertEqual(len(images), 11)

    def test_tinted_artwork_and_fields_preserve_nonrepeating_detail(self):
        for name in MATERIALS:
            image = tinted_material(name, "#30384c", (256, 192))
            self.assertNotEqual(
                image.crop((0, 0, 16, 16)).tobytes(),
                image.crop((16, 16, 32, 32)).tobytes(),
                name,
            )
            self.assertLess(max(max(p) for p in image.get_flattened_data()), 180)
            field = material_field(name)
            self.assertEqual(field.size, (256, 192))
            values = set(field.get_flattened_data())
            self.assertGreaterEqual(len(values), 3, name)
            self.assertLessEqual(max(values), 15)
            self.assertNotEqual(
                field.crop((0, 0, 16, 16)).tobytes(),
                field.crop((16, 0, 32, 16)).tobytes(),
                name,
            )
