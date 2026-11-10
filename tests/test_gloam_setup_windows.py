"""Native Windows setup checks with authored SD fixtures; no games or downloads."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PS = shutil.which('powershell.exe') or '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
AVAILABLE = Path(PS).is_file()


def win(path):
    if os.name == 'nt':
        return str(path)
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def literal(value):
    return "'" + str(value).replace("'", "''") + "'"


@unittest.skipUnless(AVAILABLE, 'Requires native Windows PowerShell')
class GloamSetupTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='gui setup ', dir=ROOT / 'build')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sd = self.root / 'sd'
        self.sd.mkdir()
        self.installer = self.root / 'installer'
        self.installer.mkdir()
        for original, output in [('gloam_setup_core.ps1', 'gloam-setup-core.ps1'),
                                 ('gloam_setup_worker.ps1', 'gloam-setup-worker.ps1'),
                                 ('gloam_setup.ps1', 'gloam-setup.ps1'),
                                 ('gloam_setup.xaml', 'gloam-setup.xaml'),
                                 ('download_addons.ps1', 'download-addons.ps1')]:
            shutil.copyfile(ROOT / 'tools' / original, self.installer / output)
        app = {'id': 'demo', 'version': 'v1', 'terms': 'Authored fixture', 'noncommercial': False,
               'repository': 'https://example.invalid/demo', 'files': []}
        (self.installer / 'downloads.json').write_text(json.dumps(
            {'format': 1, 'applications': [app], 'updates': [app], 'manual': []}))
        self.preamble = '. ' + literal(win(self.installer / 'gloam-setup-core.ps1')) + '; $root = ' + literal(win(self.sd)) + '; '

    def run_ps(self, code, success=True):
        import base64
        encoded = base64.b64encode((self.preamble + code).encode('utf-16-le')).decode()
        result = subprocess.run([PS, '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass',
                                 '-EncodedCommand', encoded], capture_output=True, timeout=60)
        output = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
        self.assertEqual(result.returncode == 0, success, output)
        return output

    def archive(self, name, members):
        path = self.root / name
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
            for member, content in members.items():
                archive.writestr(member, content)
        return path

    def app(self, name='reader.nds'):
        path = self.sd / '_nds/dsi-frontend/plugins/apps' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'Authored non-runnable executable fixture'.ljust(512, b'\0'))
        return 'sd:/_nds/dsi-frontend/plugins/apps/' + name

    def test_safe_install_preserves_user_files_and_backs_up_boot(self):
        state = self.sd / '_nds/dsi-frontend/state.ini'
        state.parent.mkdir(parents=True)
        state.write_bytes(b'personal settings')
        (self.sd / 'BOOT.NDS').write_bytes(b'old boot')
        archive = self.archive('sd.zip', {'BOOT.NDS': b'new boot', '_nds/': b'',
            '_nds/dsi-frontend/themes/': b'', '_nds/dsi-frontend/themes/demo/theme.ini': b'new theme',
            'roms/SNES/README.txt': b'Authored notes'})
        self.run_ps('Install-GloamArchive $root ' + literal(win(archive)))
        self.assertEqual(state.read_bytes(), b'personal settings')
        self.assertEqual((self.sd / 'BOOT.NDS').read_bytes(), b'new boot')
        backups = list((self.sd / '_nds/dsi-frontend/setup-backups').rglob('BOOT.NDS'))
        self.assertEqual([p.read_bytes() for p in backups], [b'old boot'])
        self.assertEqual((self.sd / 'roms/SNES/README.txt').read_bytes(), b'Authored notes')

    def test_runtime_update_replaces_owned_backends_but_preserves_custom_files(self):
        backend = self.sd / '_nds/nds-bootstrap-release.nds'
        backend.parent.mkdir()
        backend.write_bytes(b'old backend')
        theme = self.sd / '_nds/dsi-frontend/themes/custom/theme.ini'
        theme.parent.mkdir(parents=True)
        theme.write_bytes(b'personal theme')
        path = self.archive('update.zip', {'BOOT.NDS': b'new frontend',
            '_nds/nds-bootstrap-release.nds': b'new backend',
            '_nds/dsi-frontend/themes/custom/theme.ini': b'packaged defaults'})
        self.run_ps('Install-GloamArchive $root ' + literal(win(path)) + ' $true')
        self.assertEqual(backend.read_bytes(), b'new backend')
        self.assertEqual(theme.read_bytes(), b'personal theme')
        self.assertEqual([p.read_bytes() for p in (self.sd / '_nds/dsi-frontend/setup-backups').rglob('nds-bootstrap-release.nds')], [b'old backend'])

    def test_archive_preflight_rejects_escapes_and_parent_collisions_without_writes(self):
        for members in [{'roms/good.txt': b'good', '../escape': b'bad'},
                        {'roms/good.txt': b'good', 'apps/block': b'file', 'apps/block/child': b'bad'},
                        {'roms/good.txt': b'good', 'roms/GOOD.txt': b'collision'},
                        {'BOOT.NDS': b'new', 'licenses/not-on-sd.txt': b'bad'}]:
            with self.subTest(members=members):
                path = self.archive('bad.zip', members)
                self.run_ps('Install-GloamArchive $root ' + literal(win(path)), success=False)
                self.assertEqual(list(self.sd.iterdir()), [])

    def test_profiles_keep_metadata_and_separate_mode_paths_with_folder_scope(self):
        app = self.app()
        other = self.app('reader-ds.nds')
        profile = self.sd / '_nds/dsi-frontend/plugins/reader.ini'
        profile.write_text('[plugin]\nversion=1\nname=Old\nextensions=txt\napplication=' + app + '\nicon=sd:/data/custom.png\nfuture_option=keep\n')
        self.run_ps("Save-GloamProfile $root reader Reader 'md,txt,fb2.zip' " + literal(app)
                    + " 'media/books' $false $false " + literal(other))
        data = profile.read_text()
        self.assertIn('content_root=sd:/media/books', data)
        self.assertIn('ntrpath=' + other, data)
        self.assertIn('dsipath=' + app, data)
        self.assertIn('future_option=keep', data)
        self.assertIn('icon=sd:/data/custom.png', data)
        self.assertTrue((self.sd / 'media/books').is_dir())
        self.assertEqual(len(list((self.sd / '_nds/dsi-frontend/setup-backups').rglob('reader.ini'))), 1)
        before = profile.read_bytes()
        for ext in ['nds', 'argv', 'bad=value', 'waytoolongextension']:
            self.run_ps('Save-GloamProfile $root reader Reader ' + literal(ext) + ' ' + literal(app) + " 'media/books'", success=False)
            self.assertEqual(profile.read_bytes(), before)

    def test_folder_tabs_and_library_roots_preserve_unrelated_configuration(self):
        tabs = self.sd / '_nds/dsi-frontend/tabs.ini'
        tabs.parent.mkdir(parents=True)
        tabs.write_bytes((ROOT / 'sdroot/_nds/dsi-frontend/tabs.ini').read_bytes())
        self.run_ps("Save-GloamTab $root books Books 'media/books'; Save-GloamRoots $root 'roms/Games' apps media")
        data = tabs.read_text()
        self.assertIn('[tab.books]', data)
        self.assertIn('path=sd:/media/books', data)
        self.assertIn('games=sd:/roms/Games', data)
        before = tabs.read_bytes()
        for bad in ['../outside', 'C:/Windows', 'sd:/roms/../escape']:
            self.run_ps('Save-GloamTab $root test Test ' + literal(bad), success=False)
            self.assertEqual(tabs.read_bytes(), before)
        self.run_ps("Save-GloamTab $root games Games roms", success=False)

    def test_content_copy_preserves_existing_files_and_preflights_duplicates(self):
        first = self.root / 'book.txt'
        first.write_bytes(b'Authored document fixture')
        destination = self.sd / 'media/books/book.txt'
        destination.parent.mkdir(parents=True)
        destination.write_bytes(b'Existing personal file')
        other = self.root / 'notes.md'
        other.write_bytes(b'Original notes fixture')
        self.run_ps('Copy-GloamContent $root @(' + literal(win(first)) + ',' + literal(win(other)) + ") 'media/books'")
        self.assertEqual(destination.read_bytes(), b'Existing personal file')
        self.assertEqual((destination.parent / 'notes.md').read_bytes(), b'Original notes fixture')
        self.run_ps('Copy-GloamContent $root @(' + literal(win(first)) + ',' + literal(win(first)) + ") 'media/duplicate'", success=False)
        self.assertFalse((self.sd / 'media/duplicate').exists())

    def test_theme_import_keeps_existing_themes(self):
        theme = self.root / 'moon'
        theme.mkdir()
        (theme / 'theme.ini').write_text('[theme]\nversion=1\n')
        (theme / 'art').mkdir()
        (theme / 'art/bg.bin').write_bytes(b'Authored wallpaper fixture')
        command = 'Import-GloamTheme $root ' + literal(win(theme))
        self.run_ps(command)
        destination = self.sd / '_nds/dsi-frontend/themes/moon/art/bg.bin'
        self.assertEqual(destination.read_bytes(), b'Authored wallpaper fixture')
        self.run_ps(command, success=False)
        self.assertEqual(destination.read_bytes(), b'Authored wallpaper fixture')

    def test_worker_installs_verified_offline_release_and_rejects_changed_asset(self):
        package = self.root / 'release'
        package.mkdir()
        for name, members in [('sd.zip', {'BOOT.NDS': b'Original authored boot fixture'}),
                              ('addons.zip', {'_nds/dsi-frontend/plugins/apps/demo.nds': b'Original app fixture'}),
                              ('licenses.zip', {'LICENSE': b'Original license fixture'})]:
            shutil.move(self.archive(name, members), package / name)
        sums = ''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in package.iterdir())
        (package / 'SHA256SUMS').write_text(sums)
        job = {'Action': 'Install', 'Target': win(self.sd), 'ReleaseDirectory': win(package),
               'IncludeAddons': True, 'Offline': True}
        request = self.root / 'job.json'
        request.write_text(json.dumps(job))
        worker = literal(win(self.installer / 'gloam-setup-worker.ps1'))
        self.run_ps('& ' + worker + ' -Request ' + literal(win(request)))
        self.assertEqual((self.sd / 'BOOT.NDS').read_bytes(), b'Original authored boot fixture')
        (package / 'sd.zip').write_bytes(b'altered')
        self.run_ps('& ' + worker + ' -Request ' + literal(win(request)), success=False)
        self.assertEqual((self.sd / 'BOOT.NDS').read_bytes(), b'Original authored boot fixture')

    def test_bootstrap_extracts_only_verified_setup_files(self):
        names = ['gloam-setup.ps1','gloam-setup.xaml','gloam-setup-core.ps1','gloam-setup-worker.ps1','download-addons.ps1','downloads.json']
        members = {'addon-installer/' + name: b'Authored inert setup fixture' for name in names}
        members['roms/not-extracted.txt'] = b'Runtime file'
        package = self.archive('addons.zip', members)
        (self.root / 'SHA256SUMS').write_text(hashlib.sha256(package.read_bytes()).hexdigest() + '  addons.zip\n')
        code = '& ' + literal(win(ROOT / 'setup.ps1')) + ' -NoLaunch -ReleaseDirectory ' + literal(win(self.root))
        self.run_ps(code)
        for name in names:
            self.assertEqual((self.root / 'addon-installer' / name).read_bytes(), b'Authored inert setup fixture')
        self.assertFalse((self.root / 'roms').exists())
        package.write_bytes(b'Changed archive')
        self.run_ps(code, success=False)

    def test_gui_loads_every_page_and_runs_read_only_worker_without_blocking(self):
        gui = literal(win(self.installer / 'gloam-setup.ps1'))
        for page in ['Setup', 'Addons', 'Library', 'Themes']:
            screenshot = self.root / (page + '.png')
            output = self.run_ps('& ' + gui + ' -Target $root -SmokeTest -ScreenshotPage ' + page
                                + ' -ScreenshotPath ' + literal(win(screenshot)))
            self.assertIn('GUI loaded:', output)
            self.assertGreater(screenshot.stat().st_size, 5000)
        # GameYob is selected by Recommended; the installed demo is not. BIOS
        # checks must still inspect the demo, independently of those checkboxes.
        manifest = self.installer / 'downloads.json'
        data = json.loads(manifest.read_text())
        demo = data['updates'][0]
        installed = self.app('demo.nds')
        demo['files'] = [{'destination': installed[4:]}]
        demo['bios'] = 'Authored BIOS requirement'
        demo['support_requirements'] = [{'label': 'Demo BIOS', 'scope': 'required',
            'any_of': [['roms/bios/fixture.bin']]}]
        data['updates'].append(dict(id='gameyob-dsi', version='fixture', terms='Authored',
            noncommercial=False, files=[], repository='https://example.invalid'))
        manifest.write_text(json.dumps(data))
        before = {p.relative_to(self.sd).as_posix(): p.read_bytes() for p in self.sd.rglob('*') if p.is_file()}
        jobs = self.root / 'jobs'
        output = self.run_ps('& ' + gui + ' -Target $root -Offline -SmokeTest -SmokeAction CheckSupport -JobDirectory ' + literal(win(jobs)))
        self.assertIn('GUI worker exit: 0', output)
        request = json.loads(next(jobs.rglob('request.json')).read_text())
        self.assertEqual(request['Selected'], [])
        log = next(jobs.rglob('output.log')).read_text()
        self.assertIn('MISSING', log)
        self.assertIn('Demo BIOS', log)
        self.assertEqual(before, {p.relative_to(self.sd).as_posix(): p.read_bytes() for p in self.sd.rglob('*') if p.is_file()})
