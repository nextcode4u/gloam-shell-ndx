"""Exercise the portable installer's trust and SD-write boundaries."""

import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import subprocess
import sys
from types import SimpleNamespace
import tarfile
import unittest
from unittest.mock import patch
import zipfile

from tools.download_addons import (check_updates, check_support, destination, digest, fetch, reviewed_apps,
                                   select_apps, stage, update_manifest, interactive_menu, parse_selection, choose_target)


class DownloadAddonTests(unittest.TestCase):
    def test_full_catalog_keeps_user_facing_content_and_firmware_guidance(self):
        catalog = json.loads((Path(__file__).resolve().parents[1] / 'config/addon-dependencies.json').read_text())
        apps = {app['id']: app for app in reviewed_apps(catalog)}
        for app in catalog['applications']:
            self.assertTrue(app['content_guidance'], app['id'])
            self.assertEqual(apps[app['id']]['content_guidance'], app['content_guidance'])
            self.assertEqual(apps[app['id']]['bios'], app['bios'])


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

    def menu_args(self):
        self.app.update(version="v1", terms="Synthetic fixture terms", repository="https://example.invalid/demo")
        return SimpleNamespace(target=self.sd, cache=self.cache, offline=True, catalog=None,
                               noncommercial=False, overwrite=False, dry_run=False)

    def test_menu_selection_is_bounded_and_all_excludes_opt_in_apps(self):
        apps = [{"id": "demo"}, {"id": "noncommercial", "noncommercial": True},
                {"id": "experimental", "experimental": True}]
        self.assertEqual(parse_selection("all", apps), ["demo"])
        self.assertEqual(parse_selection("2, demo 3 2", apps), ["noncommercial", "demo", "experimental"])
        for text in ("0", "4", "unknown"):
            with self.assertRaises(ValueError):
                parse_selection(text, apps)

    def test_menu_target_retry_and_cancel_are_read_only(self):
        with patch('tools.download_addons.removable_drives', return_value=[(self.sd, "fixture drive")]), \
                patch('builtins.input', side_effect=[str(self.root / 'missing'), '1']), redirect_stdout(io.StringIO()):
            self.assertEqual(choose_target(), self.sd)
        with patch('tools.download_addons.removable_drives', return_value=[]), \
                patch('builtins.input', return_value=''), redirect_stdout(io.StringIO()):
            self.assertIsNone(choose_target())
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_menu_cancel_does_not_stage_or_fetch(self):
        args = self.menu_args()
        with patch('builtins.input', side_effect=['1', '1', 'n', '', '0']), \
                patch('tools.download_addons.fetch') as network, redirect_stdout(io.StringIO()):
            interactive_menu(self.manifest, args)
        network.assert_not_called()
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_menu_install_uses_checked_staging_and_returns_to_menu(self):
        args = self.menu_args()
        self.manifest['updates'] = [self.app]
        self.manifest['applications'] = []
        with patch('builtins.input', side_effect=['1', '1', 'y', '', '0']), redirect_stdout(io.StringIO()):
            interactive_menu(self.manifest, args)
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)
        self.assertFalse((self.sd / '_gba/bios.bin').exists())

    def test_menu_noncommercial_confirmation_is_required(self):
        args = self.menu_args()
        self.app['noncommercial'] = True
        with patch('builtins.input', side_effect=['1', '1', 'n', '', '0']), redirect_stdout(io.StringIO()):
            interactive_menu(self.manifest, args)
        self.assertEqual(list(self.sd.iterdir()), [])
        with patch('builtins.input', side_effect=['1', '1', 'y', 'y', '', '0']), redirect_stdout(io.StringIO()):
            interactive_menu(self.manifest, args)
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)

    def test_menu_updates_backup_executables_and_preserve_user_config(self):
        args = self.menu_args()
        self.run_stage()
        old = b'Previous executable fixture'
        (self.sd / self.name).write_bytes(old)
        config = self.sd / '_gba/configs/ABCE01.json'
        config.write_bytes(b'{"user":true}')
        with patch('builtins.input', side_effect=['3', '1', 'y', '', '0']), redirect_stdout(io.StringIO()):
            interactive_menu(self.manifest, args)
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)
        self.assertEqual(config.read_bytes(), b'{"user":true}')
        self.assertEqual((self.cache / 'previous-files' / digest(old) / 'demo.nds').read_bytes(), old)

    def test_menu_update_check_is_read_only_and_reports_reviewed_versions(self):
        args = self.menu_args()
        self.run_stage()
        before = {p: p.read_bytes() for p in self.sd.rglob('*') if p.is_file()}
        output = io.StringIO()
        with patch('builtins.input', side_effect=['2', '', '0']), redirect_stdout(output):
            interactive_menu(self.manifest, args)
        self.assertIn('reviewed v1', output.getvalue())
        self.assertEqual(before, {p: p.read_bytes() for p in self.sd.rglob('*') if p.is_file()})

    def test_standalone_no_argument_menu_and_closed_input_exit_without_writes(self):
        self.menu_args()
        source = Path(__file__).resolve().parents[1] / 'tools/download_addons.py'
        script = self.root / 'download-addons.py'
        script.write_bytes(source.read_bytes())
        (self.root / 'downloads.json').write_text(json.dumps(self.manifest))
        for entry in ('0\n', ''):
            result = subprocess.run([sys.executable, str(script)], input=entry, text=True,
                                    capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('Gloam Add-on Installer', result.stdout)
            self.assertNotIn('Choose a menu number.', result.stdout)
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_failed_download_continues_and_marks_required_apps_incomplete(self):
        before = copy.deepcopy(self.app)
        before['id'] = 'before'
        before.pop('game_configs')
        before['files'][0]['destination'] = '_nds/dsi-frontend/plugins/apps/before.nds'
        broken = copy.deepcopy(before)
        broken['id'] = 'broken'
        broken['files'][0]['destination'] = '_nds/dsi-frontend/plugins/apps/broken.nds'
        broken['files'][0]['download']['asset'] = 'missing.zip'
        after = copy.deepcopy(before)
        after['id'] = 'after'
        after['files'][0]['destination'] = '_nds/dsi-frontend/plugins/apps/after.nds'
        after['requires'] = ['broken']
        self.manifest['applications'] = [before, broken, after]
        output = io.StringIO()
        with redirect_stdout(output), self.assertRaisesRegex(ValueError, 'Required app failed'):
            self.run_stage()
        self.assertEqual((self.sd / before['files'][0]['destination']).read_bytes(), self.binary)
        self.assertEqual((self.sd / after['files'][0]['destination']).read_bytes(), self.binary)
        self.assertFalse((self.sd / broken['files'][0]['destination']).exists())
        self.assertIn('before: installed/updated', output.getvalue())
        self.assertIn('broken: FAILED', output.getvalue())
        self.assertIn('after: FAILED: Required app failed', output.getvalue())

    def test_only_pinned_members_are_installed_and_existing_files_are_preserved(self):
        self.assertEqual(self.run_stage(), (2, 0))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)
        self.assertFalse((self.sd / "_gba/bios.bin").exists())
        (self.sd / self.name).write_bytes(b"User's existing executable")
        self.assertEqual(self.run_stage(), (0, 2))
        self.assertEqual((self.sd / self.name).read_bytes(), b"User's existing executable")
        self.assertEqual(self.run_stage(overwrite=True), (2, 0))
        self.assertEqual((self.sd / self.name).read_bytes(), self.binary)

    def test_reader_fonts_are_narrowly_scoped_and_preserved_on_update(self):
        font = "data/dslibris/font/LiberationSerif-Regular.ttf"
        self.app["files"].append({**copy.deepcopy(self.app["files"][0]), "destination": font})
        self.run_stage()
        user_font = self.sd / font
        self.assertEqual(user_font.read_bytes(), self.binary)
        user_font.unlink()
        self.app.update(version="reviewed", repository="https://example.invalid/reader")
        output = io.StringIO()
        with redirect_stdout(output):
            check_updates(self.manifest, self.sd)
        self.assertIn("missing support files", output.getvalue())
        self.assertEqual(self.run_stage(update=True), (1, 2))
        user_font.write_bytes(b"User chosen font")
        settings = self.sd / "data/dslibris/dslibris.xml"
        settings.write_text("User settings")
        self.run_stage(update=True)
        self.assertEqual(user_font.read_bytes(), b"User chosen font")
        self.assertEqual(settings.read_text(), "User settings")
        for invalid in ("font/LiberationSerif-Regular.ttf", "data/dslibris/font/Other.ttf",
                        "data/dslibris/dslibris.xml", "data/other/font/LiberationSerif-Regular.ttf"):
            with self.assertRaises(ValueError):
                destination(self.sd, invalid)

    def test_inline_reader_defaults_install_without_fetch_and_updates_keep_customizations(self):
        name = "_nds/dsi-frontend/plugins/dslibris-books.ini"
        content = "[plugin]\nversion=1\nextensions=epub,fb2.zip\n"
        self.app["files"].append({"format": "inline", "content": content,
                                  "destination": name, "sha256": digest(content.encode())})
        self.run_stage()
        self.assertEqual((self.sd/name).read_text(), content)
        (self.sd/name).write_text("User profile")
        self.run_stage(update=True)
        self.assertEqual((self.sd/name).read_text(), "User profile")
        (self.sd/name).unlink()
        self.app["files"][-1]["sha256"] = "0"*64
        with self.assertRaisesRegex(ValueError,"hash mismatch"):
            self.run_stage()
        self.assertFalse((self.sd/name).exists())

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

    def test_experimental_downloads_require_explicit_selection_and_keep_user_config(self):
        self.app.update(experimental=True, notes="Known restore issue")
        self.app.pop("game_configs")
        self.app["files"][0]["destination"] = "roms/ScummVM/scummvm.nds"
        self.assertEqual(self.run_stage(), (0, 0))
        self.assertEqual(list(self.sd.iterdir()), [])
        self.assertEqual(self.run_stage(only=["demo"]), (1, 0))
        settings = self.sd / "roms/ScummVM/scummvm.ini"
        settings.write_text("User's existing settings")
        self.assertEqual(self.run_stage(only=["demo"], update=True), (0, 1))
        self.assertEqual(settings.read_text(), "User's existing settings")
        for name in ["roms/ScummVM/other.nds", "roms/ScummVM/scummvm.ini", "roms/Other/scummvm.nds"]:
            with self.assertRaises(ValueError):
                destination(self.sd, name)

    def test_reviewed_catalog_retains_experimental_selection_and_warning(self):
        catalog = {"format": 1, "applications": [{"id": "demo", "version": "v1",
            "repository": "https://example.invalid/demo", "license": "Authored fixture",
            "redistribution": "download-only", "experimental": True,
            "compatibility_notes": "Known restore issue", "package": {"files": self.app["files"]}}]}
        app = reviewed_apps(catalog)[0]
        self.assertTrue(app["experimental"])
        self.assertEqual(app["notes"], "Known restore issue")
        self.assertEqual(select_apps({"applications": [app]}, None, False), [])
        self.assertEqual(select_apps({"applications": [app]}, ["demo"], False), [app])

    def test_free_boot_rom_is_staged_but_existing_firmware_is_preserved_on_update(self):
        item = dict(self.app["files"][0], destination="roms/bios/gbc_bios.bin")
        self.app["files"].append(item)
        self.run_stage()
        bios = self.sd / item["destination"]
        self.assertEqual(bios.read_bytes(), self.binary)
        bios.write_bytes(b"User's existing firmware")
        self.run_stage(update=True)
        self.assertEqual(bios.read_bytes(), b"User's existing firmware")
        for name in ("roms/bios/bios.bin", "roms/bios/coleco.rom", "roms/bios/GBc_bios.bin"):
            with self.assertRaises(ValueError):
                destination(self.sd, name)

    def test_support_check_is_read_only_and_checks_complete_alternatives_and_sizes(self):
        self.run_stage()
        self.app['bios'] = 'Fixture BIOS needed.'
        self.app['support_requirements'] = [{'label': 'BASIC', 'scope': 'required',
            'any_of': [['roms/bios/basic.bin', 'roms/bios/ext.bin'], ['roms/bios/combined.bin']],
            'sizes': {'combined.bin': 8}}]
        bios = self.sd / 'roms/bios'
        bios.mkdir(parents=True)
        (bios / 'basic.bin').write_bytes(b'BASIC fixture')
        (bios / 'combined.bin').write_bytes(b'bad')
        before = {str(p.relative_to(self.sd)): p.read_bytes() for p in self.sd.rglob('*') if p.is_file()}
        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(check_support(self.manifest, self.sd), 1)
        self.assertIn('MISSING (required) BASIC', output.getvalue())
        self.assertEqual(before, {str(p.relative_to(self.sd)): p.read_bytes() for p in self.sd.rglob('*') if p.is_file()})
        (bios / 'combined.bin').unlink()
        (bios / 'COMBINED.BIN').write_bytes(b'fixture!')
        with redirect_stdout(io.StringIO()):
            self.assertEqual(check_support(self.manifest, self.sd), 0)
        self.app['support_requirements'][0]['any_of'] = [['../outside.bin']]
        with self.assertRaises(ValueError):
            check_support(self.manifest, self.sd)
        (bios / 'link.bin').symlink_to(self.root / 'outside.bin')
        self.app['support_requirements'][0]['any_of'] = [['roms/bios/link.bin']]
        with self.assertRaisesRegex(ValueError, 'symlink'):
            check_support(self.manifest, self.sd)

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
