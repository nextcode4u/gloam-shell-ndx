"""Keep the public documentation build separate from private working files."""

import json
import tempfile
import tomllib
import unittest
from pathlib import Path

from tools.build_docs import ROOT, rewrite_links, stage


class DocumentationTests(unittest.TestCase):
    def test_repository_links_and_guide_links(self):
        text = '[source](../../config/package-inputs.json#source) [guide](../GETTING_STARTED.md#controls)'
        result = rewrite_links(text, ROOT / 'docs/guides/ADDONS.md', ROOT)
        self.assertIn('https://github.com/nextcode4u/gloam-shell-ndx/blob/main/config/package-inputs.json#source', result)
        self.assertIn('[guide](../GETTING_STARTED.md#controls)', result)

    def test_literal_examples_and_external_links_unchanged(self):
        text = '```md\n[source](../../missing.md)\n```\n[web](https://example.org/) [anchor](#install)'
        self.assertEqual(rewrite_links(text, ROOT / 'docs/index.md', ROOT), text)

    def test_html_and_reference_links(self):
        text = '<a href="../LICENSE">License</a>\n[credits]: ../CREDITS.md'
        result = rewrite_links(text, ROOT / 'docs/index.md', ROOT)
        self.assertIn('/blob/main/LICENSE', result)
        self.assertIn('/blob/main/CREDITS.md', result)

    def test_staging_excludes_unreviewed_files_and_removes_stale_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name, content in [('docs/index.md', '# Home'), ('docs/private.md', 'private'),
                                  ('forwarder/icon-source.png', 'logo'), ('build/docs-source/stale.md', 'stale')]:
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(content)
            (root / 'config').mkdir()
            (root / 'config/package-inputs.json').write_text(json.dumps({'source_files': ['docs/index.md']}))
            destination = stage(root)
            self.assertEqual({p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()},
                             {'index.md', 'images/gloam.png'})

    def test_symlink_input_rejected_before_staged_copy_replaced(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'docs').mkdir()
            (root / 'docs/index.md').symlink_to(ROOT / 'README.md')
            (root / 'config').mkdir()
            (root / 'config/package-inputs.json').write_text(json.dumps({'source_files': ['docs/index.md']}))
            with self.assertRaises(ValueError):
                stage(root)

    def test_navigation_points_to_reviewed_pages(self):
        config = tomllib.loads((ROOT / 'zensical.toml').read_text())
        def targets(items):
            for item in items:
                for value in item.values():
                    if isinstance(value, list):
                        yield from targets(value)
                    elif not value.startswith('https:'):
                        yield 'docs/' + value
        nav = list(targets(config['project']['nav']))
        reviewed = json.loads((ROOT / 'config/package-inputs.json').read_text())['source_files']
        pages = {name for name in reviewed if name.startswith('docs/') and name.endswith('.md')}
        self.assertTrue(set(nav).issubset(pages))
        self.assertEqual(len(nav), len(set(nav)))
        for name in config["project"].get("extra_css", []) + config["project"].get("extra_javascript", []):
            self.assertIn("docs/" + name, reviewed)
            self.assertTrue((ROOT / "docs" / name).is_file())


if __name__ == '__main__':
    unittest.main()
