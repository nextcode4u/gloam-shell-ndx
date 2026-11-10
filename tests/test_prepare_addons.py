"""Keep release refreshes compatible, reviewable and permission preserving."""

import copy
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from tools.prepare_addons_release import (ReviewRequired, checksum, newer_release,
                                          inspect_catalog, prepare, updated_recipe, refresh_audit_table)


def archive(files):
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode="w:gz") as tar:
        for path, data in files.items():
            member = tarfile.TarInfo(path)
            member.size = len(data)
            tar.addfile(member, io.BytesIO(data))
    return output.getvalue()


class PrepareAddonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.cache = self.root / 'build/addons-cache'
        self.cache.mkdir(parents=True)
        self.old = archive({'demo-old/LICENSE': b'Synthetic unchanged license',
                            'demo-old/main.c': b'int main(int argc, char **argv) { open(argv[1]); }'})
        self.new = archive({'demo-new/LICENSE': b'Synthetic unchanged license',
                            'demo-new/main.c': b'int main(int argc, char **argv) { open(argv[1]); }'})
        self.binary = b'Synthetic new release binary'
        (self.cache / 'old.tar.gz').write_bytes(self.old)
        self.app = {'id': 'demo', 'repository': 'https://github.com/example/demo',
                    'version': 'v1', 'commit': 'old', 'release': 'https://github.com/example/demo/releases/tag/v1',
                    'redistribution': 'download-only', 'permission_confirmed': False,
                    'argv': 'direct', 'argv_evidence': 'https://github.com/example/demo/blob/old/main.c',
                    'bios_evidence': 'https://github.com/example/demo/blob/old/README.md',
                    'license_evidence': 'https://github.com/example/demo/tree/old',
                    'package': {'source': {'asset': 'old.tar.gz',
                        'url': 'https://codeload.github.com/example/demo/tar.gz/old', 'sha256': checksum(self.old)},
                        'files': [{'download': {'asset': 'old.nds',
                            'url': 'https://github.com/example/demo/releases/download/v1/demo.nds', 'sha256': '0' * 64},
                            'format': 'raw', 'destination': '_nds/dsi-frontend/plugins/apps/demo.nds', 'sha256': '0' * 64}],
                        'notices': [{'member': 'demo-old/LICENSE', 'destination': 'LICENSE'}]}}
        self.release = {'tag_name': 'v2', 'html_url': 'https://github.com/example/demo/releases/tag/v2',
                        'draft': False, 'prerelease': False, 'published_at': '2026-11-08T00:00:00Z',
                        'assets': [{'name': 'demo.nds', 'browser_download_url': 'https://github.com/example/demo/releases/download/v2/demo.nds',
                                    'digest': 'sha256:' + checksum(self.binary)}]}

    def update(self):
        with patch('tools.prepare_addons_release.github', return_value={'sha': 'new'}), \
                patch('tools.prepare_addons_release.get_blob', side_effect=[self.new, self.binary]):
            return updated_recipe(self.app, self.release, self.cache, self.root / 'report')

    def test_specialized_source_zip_requires_review_before_refresh(self):
        self.app["package"]["source_role"] = "reference-only"
        self.app["package"]["source"]["url"] = "https://example.invalid/source.zip"
        with self.assertRaisesRegex(ReviewRequired, "Specialized source"):
            self.update()

    def test_audit_refresh_preserves_guidance_following_the_table(self):
        folder = self.root / 'docs/guides'
        folder.mkdir(parents=True)
        path = folder / 'ADDONS.md'
        following = '\n\nShared extension guidance must remain here.\n\n## Conditions and exceptions\n\nKeep these notes.\n'
        path.write_text('Intro\n\n| App / pinned version | Argv | Delivery / terms |\n'
                        '| --- | --- | --- |\n| old | old | old |' + following)
        app = {**self.app, 'license': 'Synthetic license'}
        refresh_audit_table(self.root, {'applications': [app]})
        result = path.read_text()
        self.assertTrue(result.endswith(following))
        self.assertIn('[demo v1]', result)
        self.assertNotIn('| old |', result)
        refresh_audit_table(self.root, {'applications': [app]})
        self.assertEqual(path.read_text(), result)

    def test_newer_release_uses_dates_and_preserves_prerelease_channel(self):
        old = {**self.release, 'tag_name': 'v1', 'published_at': '2026-01-01T00:00:00Z'}
        preview = {**self.release, 'tag_name': 'v3-preview', 'prerelease': True}
        self.assertEqual(newer_release([preview, self.release, old], 'v1')['tag_name'], 'v2')
        self.assertIsNone(newer_release([self.release, old], 'v2'))
        with self.assertRaises(ReviewRequired):
            newer_release([self.release], 'unknown')
        backport = {**self.release, 'tag_name': 'v0.9', 'published_at': '2026-12-01T00:00:00Z'}
        with self.assertRaisesRegex(ReviewRequired, 'older version branch'):
            newer_release([self.release, backport], 'v2')

    def test_refresh_updates_hashes_sources_and_never_promotes_permission(self):
        result = self.update()
        self.assertEqual(result['version'], 'v2')
        self.assertEqual(result['commit'], 'new')
        self.assertEqual(result['redistribution'], 'download-only')
        self.assertFalse(result['permission_confirmed'])
        self.assertEqual(result['package']['files'][0]['sha256'], checksum(self.binary))
        self.assertEqual(result['package']['notices'][0]['member'], 'demo-new/LICENSE')
        self.assertEqual(result['argv_evidence'], 'https://github.com/example/demo/blob/new/main.c')

    def test_changed_notices_renamed_assets_and_missing_argv_need_review(self):
        original = self.new
        self.new = archive({'demo-new/LICENSE': b'Changed conditions', 'demo-new/main.c': b'argv[1]'})
        with self.assertRaisesRegex(ReviewRequired, 'notice changed'):
            self.update()
        self.assertTrue((self.root / 'report/demo-notices.diff').exists())
        self.new = archive({'demo-new/LICENSE': b'Synthetic unchanged license', 'demo-new/main.c': b'int main(void) {}'})
        with self.assertRaisesRegex(ReviewRequired, 'argv code'):
            self.update()
        self.new = original
        self.release['assets'][0]['name'] = 'renamed.nds'
        with self.assertRaisesRegex(ReviewRequired, 'renamed'):
            self.update()

    def test_paired_or_specialized_recipes_are_not_guessed(self):
        self.app['id'] = 'snemulds-shim'
        with self.assertRaisesRegex(ReviewRequired, 'Specialized'):
            self.update()

    def test_upstream_digest_mismatch_stops_refresh(self):
        self.release['assets'][0]['digest'] = 'sha256:' + '0' * 64
        with self.assertRaisesRegex(ValueError, 'digest mismatch'):
            self.update()

    def test_same_tag_asset_changes_are_detected_without_an_upstream_digest(self):
        release = {**self.release, 'tag_name': 'v1', 'assets': [{
            'name': 'demo.nds', 'digest': None,
            'browser_download_url': self.app['package']['files'][0]['download']['url']}]}
        lock = {'format': 1, 'applications': [self.app]}
        with patch('tools.prepare_addons_release.github', return_value=[release]), \
                patch('tools.prepare_addons_release.get_blob', return_value=self.binary):
            proposed, report = inspect_catalog(self.root, lock)
        self.assertEqual(proposed, lock)
        self.assertEqual(report[0]['status'], 'review-required')
        self.assertIn('bytes changed', report[0]['detail'])

    def test_pipeline_checks_before_build_and_does_not_apply_blocked_updates(self):
        (self.root / 'config').mkdir()
        self.app['redistribution'] = 'approved'
        self.app.pop('permission_confirmed')
        lock = {'format': 1, 'applications': [self.app]}
        path = self.root / 'config/addon-dependencies.json'
        path.write_text(json.dumps(lock))
        with patch('tools.prepare_addons_release.inspect_catalog', return_value=(copy.deepcopy(lock), [
                {'id': 'demo', 'status': 'review-required'}])), patch('subprocess.run') as run:
            with self.assertRaises(ReviewRequired):
                prepare(self.root, skip_build=True)
            run.assert_not_called()
            self.assertEqual(json.loads(path.read_text()), lock)
        with patch('tools.prepare_addons_release.inspect_catalog', return_value=(lock, [
                {'id': 'demo', 'status': 'current'}])), patch('subprocess.run') as run:
            prepare(self.root, skip_build=True)
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(len(commands), 4)
            self.assertIn('unittest', commands[1])
            self.assertIn('tools/check_tree.py', commands[2])
            self.assertEqual(commands[3][1:], ['tools/build_addons.py', '--pinned', '--skip-build'])


if __name__ == '__main__':
    unittest.main()
