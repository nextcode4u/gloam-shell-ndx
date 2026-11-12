"""Exercise standalone menus in Windows PowerShell 5.1 and the CMD launcher."""
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
POWERSHELL = shutil.which('powershell.exe') or '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
CMD = shutil.which('cmd.exe') or '/mnt/c/Windows/System32/cmd.exe'
AVAILABLE = Path(POWERSHELL).is_file() and Path(CMD).is_file()


def windows_path(path):
    if os.name == 'nt':
        return str(path)
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


@unittest.skipUnless(AVAILABLE, 'Requires native Windows PowerShell and CMD (Windows or WSL interop)')
class WindowsInstallerMenuTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='installer menu ', dir=ROOT / 'build')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.installer = self.root / 'installer'
        self.installer.mkdir()
        for output, source in [('download-addons.py', 'tools/download_addons.py'),
                               ('download-addons.ps1', 'tools/download_addons.ps1'),
                               ('Start Add-on Installer.cmd', 'tools/start_addon_installer.cmd')]:
            shutil.copyfile(ROOT / source, self.installer / output)
        self.cache = self.root / 'cache'
        self.cache.mkdir()
        self.payload = b'Original menu fixture, not a game or executable ROM.'
        self.digest = hashlib.sha256(self.payload).hexdigest()
        (self.cache / 'fixture.nds').write_bytes(self.payload)
        self.app = {'id': 'demo', 'version': 'v1', 'repository': 'https://example.invalid/demo',
                    'terms': 'Authored fixture terms', 'noncommercial': False, 'files': [{
                        'download': {'asset': 'fixture.nds', 'url': 'https://example.invalid/fixture.nds',
                                     'sha256': self.digest}, 'format': 'raw',
                        'destination': '_nds/dsi-frontend/plugins/apps/demo.nds', 'sha256': self.digest}]}
        self.manifest = {'format': 1, 'reviewed': '2026-11-09', 'applications': [self.app],
                         'updates': [self.app], 'manual': []}
        self.write_manifest()
        self.target = self.root / 'sd'
        self.target.mkdir()
        self.binary = self.target / self.app['files'][0]['destination']
        self.ps = [POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                   windows_path(self.installer / 'download-addons.ps1')]
        self.options = ['-Interactive', '-Offline', '-Target', windows_path(self.target),
                        '-Cache', windows_path(self.cache)]

    def write_manifest(self):
        (self.installer / 'downloads.json').write_text(json.dumps(self.manifest))

    def run_menu(self, command, entries):
        result = subprocess.run(command, input=entries, text=True, capture_output=True, timeout=45)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('Gloam Add-on Installer', result.stdout, result.stderr)
        self.assertNotIn('Operation stopped:', result.stdout)
        return result.stdout

    def test_locked_executable_survives_failed_update_and_retry_repairs_backup(self):
        old = b'Working executable before update'
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(old)
        backup = self.cache / 'previous-files' / hashlib.sha256(old).hexdigest() / self.binary.name
        backup.parent.mkdir(parents=True)
        backup.write_bytes(b'Interrupted backup')
        def quote(path):
            return "'" + windows_path(path).replace("'", "''") + "'"
        wrapper = self.root / 'locked.ps1'
        wrapper.write_text(
            "$handle = [IO.File]::Open(" + quote(self.binary) +
            ", [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)\n" +
            "try { & " + quote(self.installer / 'download-addons.ps1') +
            " -Offline -Update -Only demo -Target " + quote(self.target) +
            " -Cache " + quote(self.cache) + " } finally { $handle.Dispose() }\nexit $LASTEXITCODE\n")
        result = subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass',
                                 '-File', windows_path(wrapper)], capture_output=True, timeout=45)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.binary.read_bytes(), old)
        self.assertEqual(backup.read_bytes(), old)
        self.assertEqual(list(self.binary.parent.iterdir()), [self.binary])
        result = subprocess.run(self.ps + ['-Offline', '-Update', '-Only', 'demo',
            '-Target', windows_path(self.target), '-Cache', windows_path(self.cache)],
            capture_output=True, timeout=45)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.binary.read_bytes(), self.payload)

    def test_batch_continues_after_download_or_hash_failure_in_both_installers(self):
        apps = []
        for name in ('before', 'broken', 'after', 'dependent'):
            app = copy.deepcopy(self.app)
            app['id'] = name
            app['files'][0]['destination'] = f'_nds/dsi-frontend/plugins/apps/{name}.nds'
            apps.append(app)
        apps[3]['requires'] = ['broken']
        support = copy.deepcopy(apps[0]['files'][0])
        support['destination'] = '_nds/dsi-frontend/plugins/apps/broken-support.nds'
        apps[1]['files'].append(support)
        self.manifest['applications'] = apps
        self.manifest['updates'] = apps
        for update in (False, True):
            for powershell in (False, True):
                with self.subTest(update=update, powershell=powershell):
                    shutil.rmtree(self.target)
                    self.target.mkdir()
                    broken = apps[1]['files'][0]
                    broken['download']['asset'] = 'fixture.nds' if update else 'missing.nds'
                    broken['sha256'] = '0' * 64 if update else self.digest
                    self.write_manifest()
                    paths = [self.target / app['files'][0]['destination'] for app in apps]
                    for path in paths if update else paths[:1]:
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(b'Existing user executable')
                    if powershell:
                        command = self.ps + ['-Target', windows_path(self.target), '-Cache', windows_path(self.cache), '-Offline']
                        if update:
                            command.append('-Update')
                    else:
                        command = [sys.executable, str(self.installer / 'download-addons.py'),
                                   '--target', str(self.target), '--cache', str(self.cache), '--offline']
                        if update:
                            command.append('--update')
                    result = subprocess.run(command, capture_output=True, text=True, timeout=45)
                    log = result.stdout + result.stderr
                    self.assertEqual(result.returncode, 2, log)
                    self.assertIn('Completed with failures', log)
                    self.assertIn('broken: FAILED', log)
                    self.assertIn('after: installed/updated', log)
                    self.assertIn('dependent: FAILED: Required app failed: broken', log)
                    self.assertFalse((self.target / support['destination']).exists())
                    self.assertEqual(paths[3].read_bytes(), self.payload)
                    self.assertEqual(paths[0].read_bytes(), self.payload if update else b'Existing user executable')
                    self.assertEqual(paths[2].read_bytes(), self.payload)
                    if update:
                        self.assertEqual(paths[1].read_bytes(), b'Existing user executable')
                    else:
                        self.assertFalse(paths[1].exists())

    def test_support_checks_report_alternatives_without_changing_sd_files(self):
        self.app['bios'] = 'Fixture BIOS required.'
        self.app['support_requirements'] = [{'label': 'Fixture BIOS', 'scope': 'required',
            'any_of': [['roms/bios/first.bin', 'roms/bios/second.bin'], ['roms/bios/combined.bin']],
            'sizes': {'combined.bin': 8}}]
        self.write_manifest()
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(self.payload)
        commands = [self.ps + ['-Target', windows_path(self.target), '-Offline', '-CheckSupport'],
                    [sys.executable, str(self.installer / 'download-addons.py'), '--target', str(self.target), '--offline', '--check-support']]
        for command in commands:
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('MISSING (required) Fixture BIOS', result.stdout)
        bios = self.target / 'roms/bios/combined.bin'
        bios.parent.mkdir(parents=True)
        bios.write_bytes(b'fixture!')
        before = {str(p.relative_to(self.target)): p.read_bytes() for p in self.target.rglob('*') if p.is_file()}
        for command in commands:
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('FOUND Fixture BIOS', result.stdout)
        self.assertEqual(before, {str(p.relative_to(self.target)): p.read_bytes() for p in self.target.rglob('*') if p.is_file()})
        output = self.run_menu(self.ps + self.options, '6\n\n0\n')
        self.assertIn('FOUND Fixture BIOS', output)

    def test_free_boot_support_path_and_preserving_existing_firmware(self):
        bios_item = dict(self.app['files'][0], destination='roms/bios/gbc_bios.bin')
        self.app['files'].append(bios_item)
        self.write_manifest()
        command = self.ps + ['-Target', windows_path(self.target), '-Cache', windows_path(self.cache), '-Offline']
        result = subprocess.run(command, text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        bios = self.target / bios_item['destination']
        self.assertEqual(bios.read_bytes(), self.payload)
        bios.write_bytes(b'User firmware retained during update')
        result = subprocess.run(command + ['-Update'], text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(bios.read_bytes(), b'User firmware retained during update')

    def test_no_argument_menus_and_double_click_launcher(self):
        self.run_menu(self.ps, '0\n')
        self.run_menu([sys.executable, str(self.installer / 'download-addons.py')], '0\n')
        self.run_menu([CMD, '/d', '/c', windows_path(self.installer / 'Start Add-on Installer.cmd')], '0\n')
        self.assertEqual(list(self.target.iterdir()), [])

    def test_install_and_read_only_update_check_return_to_menu(self):
        self.manifest['applications'] = []  # Bundled apps appear only in the update catalog.
        self.write_manifest()
        output = self.run_menu(self.ps + self.options, '1\n1\ny\n\n0\n')
        self.assertGreaterEqual(output.count('Gloam Add-on Installer'), 2)
        self.assertEqual(self.binary.read_bytes(), self.payload)
        before = self.binary.stat().st_mtime_ns
        output = self.run_menu(self.ps + self.options, '2\n\n0\n')
        self.assertGreaterEqual(output.count('Gloam Add-on Installer'), 2)
        self.assertIn('up to date', output)
        self.assertEqual(self.binary.stat().st_mtime_ns, before)

    def test_update_keeps_user_config_and_backs_up_old_binary(self):
        self.binary.parent.mkdir(parents=True)
        old = b'Previous executable fixture'
        self.binary.write_bytes(old)
        settings = self.target / 'user.ini'
        settings.write_bytes(b'Preserved user settings')
        output = self.run_menu(self.ps + self.options, '3\n1\ny\n\n0\n')
        self.assertGreaterEqual(output.count('Gloam Add-on Installer'), 2)
        self.assertEqual(self.binary.read_bytes(), self.payload)
        self.assertEqual(settings.read_bytes(), b'Preserved user settings')
        backup = self.cache / 'previous-files' / hashlib.sha256(old).hexdigest() / 'demo.nds'
        self.assertEqual(backup.read_bytes(), old)

    def test_noncommercial_confirmation_can_be_declined_or_accepted(self):
        self.app['noncommercial'] = True
        self.write_manifest()
        self.run_menu(self.ps + self.options, '1\n1\nn\n\n0\n')
        self.assertFalse(self.binary.exists())
        self.run_menu(self.ps + self.options, '1\n1\ny\ny\n\n0\n')
        self.assertEqual(self.binary.read_bytes(), self.payload)

    def test_python_menu_selects_target_and_installs_without_command_line_target(self):
        command = [sys.executable, str(self.installer / 'download-addons.py'),
                   '--interactive', '--offline', '--cache', str(self.cache)]
        entries = '4\n' + str(self.target) + '\n1\n1\ny\n\n0\n'
        output = self.run_menu(command, entries)
        self.assertGreaterEqual(output.count('Gloam Add-on Installer'), 3)
        self.assertEqual(self.binary.read_bytes(), self.payload)

    def test_target_retry_and_empty_selection_do_not_write(self):
        options = ['-Interactive', '-Offline', '-Cache', windows_path(self.cache)]
        entries = '4\n' + windows_path(self.root / 'missing') + '\n' + windows_path(self.target) + '\n1\n0\n\n0\n'
        output = self.run_menu(self.ps + options, entries)
        self.assertIn('That folder is unavailable', output)
        self.assertEqual(list(self.target.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
