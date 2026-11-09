"""Exercise the portable installer's trust and SD-write boundaries."""

import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import tarfile
import unittest
from unittest.mock import patch
import zipfile

from tools.download_addons import (check_updates, destination, digest, fetch, reviewed_apps,
                                   select_apps, stage, update_manifest)


class DownloadAddonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sd = self.root / "sd"
        self.sd.mkdir()
        self.cache = self.root / "cache"
        self.cache.mkdir()
        self.binary = b"Synthetic executable fixture"
        self.config = b'{"runSettings": {}}'
        self.name = "_nds/dsi-frontend/plugins/apps/demo.nds"
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr("demo.nds", self.binary)
            archive.writestr("_gba/configs/ABCE01.json", self.config)
            archive.writestr("_gba/bios.bin", b"Synthetic excluded firmware fixture")
        self.blob = buffer.getvalue()
        (self.cache / "demo.zip").write_bytes(self.blob)
        download = {"asset": "demo.zip", "url": "https://example.invalid/demo.zip",
                    "sha256": digest(self.blob)}
        self.app = {"id": "demo", "noncommercial": False, "files": [{
            "download": download, "format": "zip", "member": "demo.nds",
            "destination": self.name, "sha256": digest(self.binary)}],
            "game_configs": {"download": download,
                             "files": {"_gba/configs/ABCE01.json": digest(self.config)}}}
        self.manifest = {"format": 1, "applications": [self.app], "manual": []}

    def run_stage(self, **options):
        return stage(self.manifest, self.sd, self.cache, offline=True, **options)

    def test_only_pinned_members_are_installed_and_existing_files_are_preserved(self):
        self.assertEqual(self.run_stage(), (2, 0))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)
        self.assertFalse((self.sd / "_gba/bios.bin").exists())
        (self.sd / self.name).write_bytes(b"User's existing executable")
        self.assertEqual(self.run_stage(), (0, 2))
        self.assertEqual((self.sd / self.name).read_bytes(), b"User's existing executable")
        self.assertEqual(self.run_stage(overwrite=True), (2, 0))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)

    def test_dry_run_makes_no_downloads_or_sd_writes(self):
        (self.cache / "demo.zip").unlink()
        self.assertEqual(self.run_stage(dry_run=True), (0, 0))
        self.assertEqual(list(self.sd.iterdir()), [])
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_corrupt_download_or_member_cannot_be_staged(self):
        (self.cache / "demo.zip").write_bytes(b"Corrupt cached archive")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.run_stage()
        self.assertEqual(list(self.sd.iterdir()), [])
        (self.cache / "demo.zip").write_bytes(self.blob)
        self.app["files"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "Member hash mismatch"):
            self.run_stage()
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_noncommercial_selection_and_unknown_ids_are_explicit(self):
        self.app["noncommercial"] = True
        self.assertEqual(self.run_stage(), (0, 0))
        with self.assertRaisesRegex(ValueError, "noncommercial"):
            self.run_stage(only=["demo"])
        with self.assertRaisesRegex(ValueError, "No pinned download"):
            self.run_stage(only=["missing"])
        self.assertEqual(self.run_stage(noncommercial=True), (2, 0))

    def test_path_escape_firmware_and_symlink_destinations_are_rejected(self):
        for name in ["../outside.nds", "/outside.nds", "_gba/bios.bin", "C:/outside.nds",
                     "_nds/dsi-frontend/plugins/apps/../../settings.ini"]:
            with self.assertRaises(ValueError):
                destination(self.sd, name)
        outside = self.root / "outside"
        outside.mkdir()
        (self.sd / "_nds").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.run_stage()
        self.assertEqual(list(outside.iterdir()), [])

    def test_all_destinations_are_validated_before_any_install(self):
        second = copy.deepcopy(self.app)
        second["id"] = "other"
        self.manifest["applications"].append(second)
        with self.assertRaisesRegex(ValueError, "overlapping"):
            self.run_stage()
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_update_replaces_executable_with_backup_and_preserves_custom_config(self):
        self.run_stage()
        old = b"Previously installed application fixture"
        (self.sd / self.name).write_bytes(old)
        config = self.sd / "_gba/configs/ABCE01.json"
        config.write_bytes(b'{"user":true}')
        other = copy.deepcopy(self.app)
        other["id"] = "not-installed"
        other["files"][0]["destination"] = self.name.replace('demo.nds', 'other.nds')
        other.pop('game_configs')
        self.manifest["applications"].append(other)
        self.assertEqual(self.run_stage(update=True), (1, 1))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)
        self.assertEqual(config.read_bytes(), b'{"user":true}')
        self.assertEqual((self.cache / 'previous-files' / digest(old) / 'demo.nds').read_bytes(), old)
        self.assertFalse((self.sd / other['files'][0]['destination']).exists())
        self.assertEqual(self.run_stage(update=True), (0, 2))

    def test_update_dry_run_has_no_backup_or_writes(self):
        self.run_stage()
        (self.sd / self.name).write_bytes(b"Old executable")
        self.run_stage(update=True, dry_run=True)
        self.assertEqual((self.sd / self.name).read_bytes(), b"Old executable")
        self.assertFalse((self.cache / 'previous-files').exists())

    def test_check_updates_is_read_only_and_reports_other_upstream_tags(self):
        self.run_stage()
        self.app.update(version='1', repository='https://github.com/example/demo', release_tag='v1')
        output = io.StringIO()
        with redirect_stdout(output), patch('tools.download_addons.read_remote_json',
                                           return_value={'tag_name': 'v2'}):
            check_updates(self.manifest, self.sd, upstream=True)
        self.assertIn('up to date', output.getvalue())
        self.assertIn('Other upstream release: v2 (not selected', output.getvalue())
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)

    def test_reviewed_catalog_refresh_fallback_and_older_catalog(self):
        self.manifest['updates'] = [self.app]
        self.manifest['reviewed'] = '2026-11-08'
        older = {**self.manifest, 'reviewed': '2025-01-01', 'updates': []}
        with patch('tools.download_addons.read_remote_json', return_value=older):
            self.assertEqual(update_manifest(self.manifest)['applications'], [self.app])
        with patch('tools.download_addons.read_remote_json', side_effect=OSError('unavailable')):
            self.assertEqual(update_manifest(self.manifest)['applications'], [self.app])
        with patch('tools.download_addons.read_remote_json') as network:
            update_manifest(self.manifest, offline=True)
            network.assert_not_called()
        newer = copy.deepcopy(self.manifest)
        newer['updates'][0]['version'] = '2'
        path = self.root / 'catalog.json'
        path.write_text(json.dumps(newer))
        self.assertEqual(update_manifest(self.manifest, path)['applications'][0]['version'], '2')

    def test_update_cache_accepts_new_hash_for_the_same_asset_name(self):
        new = b"New application bytes"
        item = {'asset': 'demo.zip', 'url': 'https://example.invalid/release', 'sha256': digest(new)}
        with patch('urllib.request.urlopen', return_value=io.BytesIO(new)):
            self.assertEqual(fetch(item, self.cache, offline=False), new)
        self.assertEqual((self.cache / 'demo.zip').read_bytes(), self.blob)
        self.assertEqual(fetch(item, self.cache, offline=True), new)

    def test_paired_payloads_are_selected_together_or_rejected(self):
        payload = copy.deepcopy(self.app)
        payload['id'] = 'payload'
        self.app['requires'] = ['payload']
        self.manifest['applications'].append(payload)
        self.assertEqual([a['id'] for a in select_apps(self.manifest, ['demo'], False)], ['demo', 'payload'])
        self.manifest['applications'].pop()
        with self.assertRaisesRegex(ValueError, 'Missing paired'):
            select_apps(self.manifest, ['demo'], False)

    def test_tar_payload_is_read_without_extracting_other_members(self):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode='w:gz') as archive:
            for name, data in [('source/demo.nds', self.binary), ('../outside', b'Not installed')]:
                member = tarfile.TarInfo(name)
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
        blob = buffer.getvalue()
        (self.cache / 'source.tar.gz').write_bytes(blob)
        self.app.pop('game_configs')
        self.app['files'][0].update(format='tar.gz', member='source/demo.nds', download={
            'asset': 'source.tar.gz', 'url': 'https://example.invalid/source', 'sha256': digest(blob)})
        self.assertEqual(self.run_stage(), (1, 0))
        self.assertFalse((self.root / 'outside').exists())


if __name__ == "__main__":
    unittest.main()
