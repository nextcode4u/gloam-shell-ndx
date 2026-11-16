"""External language/font inputs stay reproducible and ready for SD staging."""
import importlib.util
from pathlib import Path
import struct
import unittest
ROOT = Path(__file__).resolve().parents[1]


def generator(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class LanguageAssetsTests(unittest.TestCase):
    def test_font_packs_match_reviewed_sources(self):
        for name, file in [('make_japanese_font', 'japanese'), ('pack_script_fonts', 'extended')]:
            expected = generator(name).generate()
            actual = (ROOT / 'sdroot/_nds/dsi-frontend/fonts' / (file + '.gsf')).read_bytes()
            self.assertEqual(actual, expected)
            self.assertEqual(len(actual), 20 + struct.unpack_from('<I', actual, 8)[0] * 29)

    def test_shipped_catalogs_match_contributor_sources(self):
        for source, identifier in [('japanese', 'ja'), ('spanish', 'es')]:
            expected = (ROOT / 'config/languages' / (source + '.ini')).read_bytes()
            actual = (ROOT / 'sdroot/_nds/dsi-frontend/languages' / (identifier + '.ini')).read_bytes()
            self.assertEqual(actual, expected)
            self.assertLessEqual(len(actual), 32768)
            keys = []
            for line in actual.decode('utf-8').splitlines():
                if line and not line.startswith((';', '#')):
                    keys.append(line.split('=', 1)[0])
            self.assertEqual(len(keys), len(set(keys)))
            self.assertIn('version', keys)
            self.assertIn('home', keys)


if __name__ == '__main__':
    unittest.main()
