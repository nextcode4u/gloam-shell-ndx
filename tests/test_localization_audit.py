import importlib.util
from pathlib import Path
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('localization_audit', ROOT / 'tools/audit_localization.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class LocalizationAuditTests(unittest.TestCase):
    def test_native_catalog_bounds_and_duplicate_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.ini'
            for data in [b'version=1\nhome=A\nhome=B\n', b'version=2\n',
                         b'version=1\nhome=\xff\n', ('version=1\nhome=' + '本' * 22).encode(),
                         b'version=1\nunknown=value\n']:
                path.write_bytes(data)
                with self.assertRaises((ValueError, UnicodeError)):
                    audit.catalog(path)
            path.write_text('version=1\nhome=部分\n')
            self.assertEqual(audit.catalog(path), {'home': '部分'})
            path.write_bytes(b'\xef\xbb\xbfversion=1\r\nhome=Home\r\n')
            self.assertEqual(audit.catalog(path), {'home': 'Home'})
            path.write_bytes(b'version=1\nhome=Home\x00hidden')
            with self.assertRaises(ValueError):
                audit.catalog(path)

    def test_registered_keys_and_shipped_glyph_coverage(self):
        keys = audit.known_keys()
        self.assertIn('setting.interface_language.label', keys)
        self.assertIn('dialog.game_manual.help', keys)
        widths = audit.script_advances()
        self.assertEqual(widths[ord('本')], 12)
        self.assertIn(ord('Ω'), widths)
        for id in ('ja', 'es', 'fr', 'de', 'it', 'pt-BR', 'nl'):
            report = audit.audit(ROOT / 'sdroot/_nds/dsi-frontend/languages' / (id + '.ini'), keys, widths,
                                 ROOT / 'sdroot/_nds/dsi-frontend/themes')
            self.assertFalse(report['unsupported_glyphs'])
            self.assertGreater(report['coverage_percent'], 50)
            self.assertTrue(report['missing_keys'])  # Starters retain English help fallbacks.
            if id not in ('es', 'ja'):
                self.assertTrue(all(key.endswith('.help') for key in report['missing_keys']))
            self.assertFalse(report['unknown_keys'])
            self.assertFalse(report['potential_overflow'])

    def test_distributed_catalogs_match_sources_and_packaging(self):
        import json

        inventory = json.loads((ROOT / 'config/package-inputs.json').read_text())
        sources = {'ja': 'japanese', 'es': 'spanish', 'fr': 'french',
                   'de': 'german', 'it': 'italian', 'pt-BR': 'portuguese-brazilian',
                   'nl': 'dutch'}
        for locale, source in sources.items():
            relative = 'sdroot/_nds/dsi-frontend/languages/' + locale + '.ini'
            self.assertEqual((ROOT / relative).read_bytes(),
                             (ROOT / 'config/languages' / (source + '.ini')).read_bytes())
            self.assertIn(relative, inventory['sd_files'])
            self.assertIn(relative, inventory['source_files'])
            values = audit.catalog(ROOT / relative)
            self.assertIn('setting.interface_language.label', values)
            self.assertIn('dialog.theme_controls', values)
            if locale not in ('es', 'ja'):
                self.assertGreaterEqual(len(values), 325)
                self.assertIn('setting.splash_mode.help', values)
                self.assertIn('setting.background_updates.help', values)

    def test_template_covers_the_registered_keys_within_native_limits(self):
        self.assertEqual(set(audit.catalog(ROOT / 'config/languages/template.ini')),
                         audit.known_keys())

    def test_font_corruption_does_not_produce_false_coverage(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'japanese.gsf'
            data = bytearray((ROOT / 'sdroot/_nds/dsi-frontend/fonts/japanese.gsf').read_bytes())
            data[-1] ^= 1
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'checksum'):
                audit.script_advances(Path(folder))

    def test_fixed_width_warnings_are_advisory_and_font_paths_are_bounded(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            theme = root / 'themes/example'
            theme.mkdir(parents=True)
            manifest = theme / 'theme.ini'
            manifest.write_text('[theme]\nversion=1\n')
            catalog = root / 'language.ini'
            catalog.write_text('version=1\nsettings=ABCDEFGHIJKLMN\n')
            report = audit.audit(catalog, audit.known_keys(), audit.script_advances(), root / 'themes')
            self.assertEqual(report['potential_overflow'][0]['available'], 58)
            manifest.write_text('[theme]\nversion=1\n[fonts]\nui=../../outside.dsfont\n')
            with self.assertRaisesRegex(ValueError, 'escapes'):
                audit.audit(catalog, audit.known_keys(), audit.script_advances(), root / 'themes')
