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
        for original, output in [('gloam_setup_artwork.ps1','gloam-setup-artwork.ps1'),
                                 ('gloam_setup_artwork_ui.ps1','gloam-setup-artwork-ui.ps1'),('gloam_setup_core.ps1', 'gloam-setup-core.ps1'),
                                 ('gloam_setup_organizer.ps1', 'gloam-setup-organizer.ps1'),
                                 ('gloam_setup_organizer_ui.ps1', 'gloam-setup-organizer-ui.ps1'),
                                 ('gloam_setup_settings.ps1', 'gloam-setup-settings.ps1'),
                                 ('gloam_setup_settings_ui.ps1', 'gloam-setup-settings-ui.ps1'),
                                 ('gloam_setup_configuration.ps1', 'gloam-setup-configuration.ps1'),
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
        self.preamble += '. ' + literal(win(self.installer / 'gloam-setup-settings.ps1')) + '; . ' + literal(win(self.installer / 'gloam-setup-configuration.ps1')) + '; '

        self.preamble += '. ' + literal(win(self.installer / 'gloam-setup-organizer.ps1')) + '; '
        self.preamble += '. ' + literal(win(self.installer / 'gloam-setup-artwork.ps1')) + '; '

    def run_ps(self, code, success=True):
        import base64
        encoded = base64.b64encode((self.preamble + code).encode('utf-16-le')).decode()
        result = subprocess.run([PS, '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass',
                                 '-EncodedCommand', encoded], capture_output=True, timeout=60)
        output = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
        self.assertEqual(result.returncode == 0, success, output)
        return output

    def test_addon_details_check_bios_without_requiring_installation(self):
        output = self.run_ps(r"""
$app = [pscustomobject]@{ id='example'; content_guidance='roms/Example: .rom'; bios='User supplies BIOS'; support_requirements=@(
    [pscustomobject]@{label='Example BIOS'; scope='required'; any_of=@(@('roms/bios/example.bin')); sizes=[pscustomobject]@{'example.bin'=4}}
) }
$before = Get-GloamAddonDetails $app $root
if ($before -notmatch 'Missing or wrong size' -or $before -notmatch '\.rom') { throw $before }
$folder = Join-Path $root 'roms/bios'
[IO.Directory]::CreateDirectory($folder) | Out-Null
[IO.File]::WriteAllBytes((Join-Path $folder 'example.bin'), [byte[]](1,2))
if ((Get-GloamAddonDetails $app $root) -notmatch 'Missing or wrong size') { throw 'Accepted wrong BIOS size' }
[IO.File]::WriteAllBytes((Join-Path $folder 'example.bin'), [byte[]](1,2,3,4))
if ((Get-GloamAddonDetails $app $root) -notmatch 'required\]: Found') { throw 'Did not detect BIOS' }
if ((Get-GloamAddonDetails $app '') -notmatch 'Not checked') { throw 'No-target result misleading' }
""")

    def test_organizer_discovers_systems_from_selected_sd(self):
        for folder in ('roms/NES', 'roms/Atari/2600', 'roms/Atari/Lynx',
                       'roms/bios', 'roms/saves', 'roms/ScummVM/titles',
                       'roms/NES/A', 'collections/SNES'):
            (self.sd / folder).mkdir(parents=True, exist_ok=True)
        config = self.sd / '_nds/dsi-frontend/tabs.ini'
        config.parent.mkdir(parents=True)
        config.write_text('[roots]\ngames=collections\n')
        self.run_ps(r"""
$folders = @(Get-GloamOrganizerFolders $root)
$expected = @('roms/Atari/2600','roms/Atari/Lynx','roms/NES','collections/SNES')
if (Compare-Object $expected $folders) { throw ($folders -join ', ') }
""")

    def test_addon_columns_refresh_install_and_firmware_status(self):
        self.run_ps(r"""
$app = [pscustomobject]@{ id='example'; files=@([pscustomobject]@{destination='apps/one.nds'}, [pscustomobject]@{destination='apps/two.nds'}); bios_mode='required'; support_requirements=@(
    [pscustomobject]@{ label='BIOS'; scope='required'; any_of=@(@('roms/bios/test.bin')); sizes=[pscustomobject]@{'test.bin'=4} }
) }
$status = Get-GloamAddonStatus $app $root
if ($status.Installation -ne 'Not installed' -or $status.BiosStatus -ne 'Required - missing') { throw 'Empty card status' }
[IO.Directory]::CreateDirectory((Join-Path $root 'apps')) | Out-Null
[IO.File]::WriteAllBytes((Join-Path $root 'apps/one.nds'), [byte[]](1))
if ((Get-GloamAddonStatus $app $root).Installation -ne 'Partial install') { throw 'Partial install status' }
[IO.File]::WriteAllBytes((Join-Path $root 'apps/two.nds'), [byte[]](1))
[IO.Directory]::CreateDirectory((Join-Path $root 'roms/bios')) | Out-Null
[IO.File]::WriteAllBytes((Join-Path $root 'roms/bios/test.bin'), [byte[]](1,2,3,4))
$status = Get-GloamAddonStatus $app $root
if ($status.Installation -ne 'Installed' -or $status.BiosStatus -ne 'Present') { throw 'Installed status' }
$app.bios_mode='optional'; $app.support_requirements[0].scope='optional'
if ((Get-GloamAddonStatus $app $root).BiosStatus -ne 'Optional - present') { throw 'Optional present status' }
Remove-Item -LiteralPath (Join-Path $root 'roms/bios/test.bin')
if ((Get-GloamAddonStatus $app $root).BiosStatus -ne 'Optional - missing') { throw 'Optional missing status' }
$app.support_requirements=@(); $app.bios_mode='embedded'
if ((Get-GloamAddonStatus $app $root).BiosStatus -ne 'Embedded in app') { throw 'Embedded status' }
$app.bios_mode='none'
if ((Get-GloamAddonStatus $app $root).BiosStatus -ne 'Not needed') { throw 'No BIOS status' }
if ((Get-GloamAddonStatus $app '').Installation -ne 'Not checked') { throw 'No card status' }
""")

    def artwork_png(self):
        import struct, zlib
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress((b'\0' + b'\x90\x40\xff\xff' * 2) * 2)) + chunk(b'IEND', b''))

    def test_artwork_import_preserves_originals_and_application(self):
        folder = self.sd / 'media/books'
        folder.mkdir(parents=True)
        (folder / 'Example.md').write_text('Authored book fixture')
        image = self.root / 'sample.png'
        image.write_bytes(self.artwork_png())
        self.run_ps("$source=" + literal(win(image)) + r"""
$content='media/books/Example.md'
$rows=@(Get-GloamArtworkLibrary $root 'media/books' $false)
if ($rows.Count -ne 1 -or $rows[0].Status -ne 'Missing') { throw 'Scan status' }
$record=Read-GloamArtworkRecord $root $content
$record.Values['application']='sd:/apps/reader.nds'
Save-GloamArtworkRecord $root $record
$png=ConvertTo-GloamArtworkPng ([IO.File]::ReadAllBytes($source))
[IO.File]::WriteAllBytes($source,$png)
$hash=Get-GloamHash $source
Set-GloamArtwork $root $content $source $hash 'Authored fixture' $false
$state=Get-GloamArtworkState $root $content
if ($state.Status -ne 'Present' -or $state.Kind -ne 'Assigned') { throw 'Missing assignment' }
$original=Get-GloamHash $state.Image
$refused=$false
try { Set-GloamArtwork $root $content $source $hash 'Authored fixture' $false } catch { $refused=$true }
if (-not $refused) { throw 'Replaced existing art without opt-in' }
Set-GloamArtwork $root $content $source $hash 'Authored fixture' $true
Clear-GloamArtwork $root $content
$record=Read-GloamArtworkRecord $root $content
if ($record.Values['application'] -ne 'sd:/apps/reader.nds' -or $record.Values['cover']) { throw 'Lost per-file application' }
if ((Get-GloamHash $state.Image) -ne $original) { throw 'Original image changed' }
if ((Get-GloamArtworkState $root $content).Status -ne 'Missing') { throw 'Assignment not cleared' }
""")
        self.assertEqual((folder / 'Example.md').read_text(), 'Authored book fixture')

    def test_artwork_urls_and_embedded_music(self):
        import struct
        folder = self.sd / 'media/music'
        folder.mkdir(parents=True)
        payload = b'\0image/png\0\x03\0' + self.artwork_png()
        frame = b'APIC' + struct.pack('>I', len(payload)) + b'\0\0' + payload
        size = bytes((len(frame) >> shift) & 127 for shift in (21, 14, 7, 0))
        (folder / 'Example.mp3').write_bytes(b'ID3\x03\0\0' + size + frame + b'authored audio fixture')
        ds = self.sd / 'roms/DS'
        ds.mkdir(parents=True)
        (ds / 'Example.nds').write_bytes(b'AuthoredTest' + b'ASME')
        self.run_ps(r"""
$url=Get-GloamArtworkUrl $root 'roms/DS/Example.nds' 'Nintendo DS (GameTDB ID)' '' 'US'
if ($url -ne 'https://art.gametdb.com/ds/coverS/US/ASME.png') { throw $url }
$url=Get-GloamArtworkUrl $root 'media/music/Example.mp3' 'Nintendo - Game Boy' 'Example & Test (USA)'
if ($url -notmatch 'Example%20_%20Test%20') { throw $url }
$state=Get-GloamArtworkState $root 'media/music/Example.mp3'
if ($state.Status -ne 'Embedded') { throw $state.Status }
$png=ConvertTo-GloamArtworkPng (Get-GloamEmbeddedArtwork $state.Image)
if (-not $png.Length) { throw 'No embedded image' }
$refused=$false
try { Get-GloamArtworkDownload 'file:///C:/private.png' } catch { $refused=$true }
if (-not $refused) { throw 'Accepted non-HTTPS URL' }
""")

    def test_artwork_rejects_mismatched_record_and_invalid_image(self):
        (self.sd / 'roms').mkdir()
        (self.sd / 'roms/demo.nds').write_bytes(b'Authored fixture')
        self.run_ps(r"""
$record=Read-GloamArtworkRecord $root 'roms/demo.nds'
$record.Values['content']='sd:/roms/different.nds'
Save-GloamArtworkRecord $root $record
$refused=$false
try { Clear-GloamArtwork $root 'roms/demo.nds' } catch { $refused=$true }
if (-not $refused) { throw 'Overwrote mismatched identity' }
$refused=$false
try { ConvertTo-GloamArtworkPng ([Text.Encoding]::UTF8.GetBytes('<html>not an image</html>')) } catch { $refused=$true }
if (-not $refused) { throw 'Accepted HTML as image' }
$rows=@(Get-GloamArtworkLibrary $root 'roms' $false)
if ($rows[0].Status -ne 'Check record') { throw 'Invalid record was hidden' }
""")

    def test_artwork_worker_continues_after_bad_preview(self):
        folder = self.sd / 'roms/NES'
        folder.mkdir(parents=True)
        for name in ('First.nes', 'Second.nes'):
            (folder / name).write_bytes(b'authored fixture')
        image = self.root / 'preview.png'
        image.write_bytes(self.artwork_png())
        rows = []
        for name in ('First.nes', 'Second.nes'):
            rows.append(dict(Content='roms/NES/' + name, Candidate=win(image),
                             CandidateHash='0'*64 if name.startswith('First') else hashlib.sha256(image.read_bytes()).hexdigest(),
                             Source='Authored fixture', Result='', Status='Missing', Kind='', Image='', Selected=True))
        request = self.root / 'request.json'
        request.write_text(json.dumps(dict(Action='ArtworkApply', Target=win(self.sd),
                                          ReleaseDirectory=win(self.root), Offline=True,
                                          Options=dict(Items=rows, Replace=False))))
        self.run_ps('& ' + literal(win(self.installer / 'gloam-setup-worker.ps1')) + ' -Request ' + literal(win(request)), success=False)
        results = json.loads((self.root / 'artwork-results.json').read_text(encoding='utf-8-sig'))
        self.assertIn('preview changed', results[0]['Result'])
        self.assertEqual(results[1]['Result'], 'Applied')
        self.assertEqual((folder / 'First.nes').read_bytes(), b'authored fixture')

    def test_gui_artwork_scan_uses_selected_card(self):
        (self.sd / 'roms').mkdir()
        (self.sd / 'roms/Example.nds').write_bytes(b'authored fixture')
        output = self.run_ps('& ' + literal(win(self.installer / 'gloam-setup.ps1'))
                             + ' -Target $root -Offline -SmokeTest -SmokeAction ArtworkScan -ScreenshotPage Artwork')
        self.assertIn('GUI worker exit: 0', output)
        self.assertFalse((self.sd / '_nds/dsi-frontend/items').exists())

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

    def organizer_file(self, relative, data=b'Authored fixture'):
        path = self.sd / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def organize(self, mode='Alphabetize', folder='roms/Nintendo DS'):
        plan = self.root / 'plan.json'
        self.run_ps('$plan = New-GloamOrganizerPlan $root ' + literal(folder) + ' ' + mode
                    + '; Write-GloamOrganizerJson ' + literal(win(plan)) + ' $plan; '
                    + 'Invoke-GloamOrganization $root ' + literal(win(plan))
                    + ' (Get-GloamHash ' + literal(win(plan)) + ')')
        return json.loads(plan.read_text(encoding='utf-8'))

    def test_organizer_preserves_companions_and_undo_restores_exact_configuration(self):
        old = 'sd:/roms/Nintendo DS/Moon.nds'
        new = 'sd:/roms/Nintendo DS/M/Moon.nds'
        def identity_hash(text):
            value = 14695981039346656037
            for byte in text.encode():
                value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
            return f'{value:016x}'
        snapshots = {}
        for name in ['Moon.nds', 'Moon.sav', 'Moon.nds.png', 'Moon.pub', 'saves/Moon.sav2', 'saves/Moon.prv3']:
            path = self.organizer_file('roms/Nintendo DS/' + name, name.encode())
            snapshots[path.relative_to(self.sd).as_posix()] = path.read_bytes()
        for name, content in {
            'state.ini': 'version=1\nfavorite=' + old + '\nfavorite=sd:/roms/Other.nds\nselected_path=' + old + '\n',
            'recent.ini': 'request=123,' + old + '\n',
            'items/' + identity_hash(old) + '.ini': 'content=' + old + '\ncover=' + old + '.png\n',
            'games/' + identity_hash(old) + '.ini': 'content=' + old + '\ncpu_boost=1\n',
            'games/' + identity_hash(old) + '.cheats.bak': 'content=' + old + '\nselected=123\n',
        }.items():
            path = self.organizer_file('_nds/dsi-frontend/' + name, content.encode())
            snapshots[path.relative_to(self.sd).as_posix()] = path.read_bytes()
        plan = self.organize()
        self.assertEqual(len(plan['Moves']), 6)
        self.assertEqual(len(plan['Edits']), 5)
        self.assertIn(new, (self.sd / '_nds/dsi-frontend/state.ini').read_text())
        self.assertTrue((self.sd / ('_nds/dsi-frontend/games/' + identity_hash(new) + '.ini')).is_file())
        for move in plan['Moves']:
            self.assertFalse((self.sd / move['Source']).exists())
            self.assertEqual((self.sd / move['Destination']).read_bytes(), snapshots[move['Source']])
        journal = next((self.sd / '_nds/dsi-frontend/organizer').iterdir()).name
        self.run_ps('Undo-GloamOrganization $root ' + literal(journal))
        for name, data in snapshots.items():
            self.assertEqual((self.sd / name).read_bytes(), data)
        self.assertFalse((self.sd / ('_nds/dsi-frontend/games/' + identity_hash(new) + '.ini')).exists())

    def test_organizer_skips_collisions_shared_saves_and_multifile_folders(self):
        for name in ['Moon.nds', 'Moon.sav', 'M/Moon.nds', 'Dual.nds', 'Dual.dsi', 'Dual.sav',
                     'Game Folder/data.nds', 'README.md', 'BIOS.nds', '.hidden.nds', '2 Games.nds']:
            self.organizer_file('roms/Nintendo DS/' + name)
        plan = self.organize()
        self.assertEqual([m['Source'] for m in plan['Moves']], ['roms/Nintendo DS/2 Games.nds'])
        self.assertEqual(len(plan['Skipped']), 3)
        for name in ['Moon.nds', 'Moon.sav', 'Dual.nds', 'Dual.dsi', 'Dual.sav', 'Game Folder/data.nds', 'README.md']:
            self.assertTrue((self.sd / 'roms/Nintendo DS' / name).exists())
        self.assertTrue((self.sd / 'roms/Nintendo DS/#/2 Games.nds').is_file())

    def test_organizer_flatten_preserves_bucket_save_folder(self):
        self.organizer_file('roms/Nintendo DS/m/Moon.nds')
        self.organizer_file('roms/Nintendo DS/m/saves/Moon.sav')
        self.organizer_file('roms/Nintendo DS/m/multifile/extra.nds')
        plan = self.organize('Flatten')
        self.assertEqual(len(plan['Moves']), 2)
        self.assertTrue((self.sd / 'roms/Nintendo DS/Moon.nds').is_file())
        self.assertTrue((self.sd / 'roms/Nintendo DS/saves/Moon.sav').is_file())
        self.assertTrue((self.sd / 'roms/Nintendo DS/m/multifile/extra.nds').is_file())

    def test_organizer_stale_preview_refuses_before_moving(self):
        rom = self.organizer_file('roms/Nintendo DS/Moon.nds')
        plan = self.root / 'plan.json'
        command = 'Write-GloamOrganizerJson ' + literal(win(plan)) + " (New-GloamOrganizerPlan $root 'roms/Nintendo DS' Alphabetize)"
        self.run_ps(command)
        rom.write_bytes(b'Changed file after preview')
        self.run_ps('Invoke-GloamOrganization $root ' + literal(win(plan))
                    + ' (Get-GloamHash ' + literal(win(plan)) + ')', success=False)
        self.assertTrue(rom.exists())
        self.assertFalse((self.sd / 'roms/Nintendo DS/M').exists())
        self.assertFalse((self.sd / '_nds').exists())

    def test_organizer_undo_preserves_new_save_changes(self):
        self.organizer_file('roms/Nintendo DS/Moon.nds')
        self.organizer_file('roms/Nintendo DS/Moon.sav')
        self.organize()
        save = self.sd / 'roms/Nintendo DS/M/Moon.sav'
        save.write_bytes(b'New save progress')
        journal = next((self.sd / '_nds/dsi-frontend/organizer').iterdir()).name
        self.run_ps('Undo-GloamOrganization $root ' + literal(journal), success=False)
        self.assertEqual(save.read_bytes(), b'New save progress')
        self.assertTrue((save.parent / 'Moon.nds').is_file())

    def test_organizer_unicode_names_and_interrupted_metadata_recovery(self):
        self.organizer_file('roms/Nintendo DS/Étoile.nds')
        state = self.organizer_file('_nds/dsi-frontend/state.ini',
                                    'version=1\nfavorite=sd:/roms/Nintendo DS/Étoile.nds\n'.encode())
        original = state.read_bytes()
        plan = self.root / 'plan.json'
        self.run_ps("$plan = New-GloamOrganizerPlan $root 'roms/Nintendo DS' Alphabetize; "
                    + 'Write-GloamOrganizerJson ' + literal(win(plan)) + ' $plan; '
                    + '$script:originalWriter = ${function:Write-GloamOrganizerJson}; '
                    + '$script:injected = $false; '
                    + 'function Write-GloamOrganizerJson($Path,$Value) { '
                    + "if ($Path.EndsWith('status.json') -and $Value.State -eq 'complete' -and -not $script:injected) { "
                    + "$script:injected = $true; throw 'Injected completion failure' }; "
                    + '& $script:originalWriter $Path $Value }; '
                    + 'Invoke-GloamOrganization $root ' + literal(win(plan))
                    + ' (Get-GloamHash ' + literal(win(plan)) + ')', success=False)
        self.assertEqual(state.read_bytes(), original)
        self.assertTrue((self.sd / 'roms/Nintendo DS/Étoile.nds').is_file())
        journal = next((self.sd / '_nds/dsi-frontend/organizer').iterdir())
        self.assertEqual(json.loads((journal / 'status.json').read_text(encoding='utf-8'))['State'], 'undone')
        self.organize()
        self.assertTrue((self.sd / 'roms/Nintendo DS/#/Étoile.nds').is_file())
        self.assertIn('sd:/roms/Nintendo DS/#/Étoile.nds', state.read_text(encoding='utf-8'))

    def test_gui_organizer_preview_uses_worker_and_leaves_sd_unchanged(self):
        rom = self.organizer_file('roms/Nintendo DS/Éditions/Moon.nds')
        jobs = self.root / 'jobs'
        gui = literal(win(self.installer / 'gloam-setup.ps1'))
        output = self.run_ps('& ' + gui + ' -Target $root -Offline -SmokeTest -SmokeAction PreviewOrganization '
                             + "-ScreenshotPage Organizer -SmokeOrganizerFolder 'roms/Nintendo DS/Éditions' -JobDirectory " + literal(win(jobs)))
        self.assertIn('GUI worker exit: 0', output)
        plan = json.loads(next(jobs.rglob('organizer-plan.json')).read_text())
        self.assertEqual(len(plan['Moves']), 1)
        self.assertTrue(rom.is_file())
        self.assertFalse((self.sd / '_nds').exists())

    def test_organizer_recovers_a_partially_moved_journal(self):
        self.organizer_file('roms/Nintendo DS/Moon.nds')
        self.organizer_file('roms/Nintendo DS/Moon.sav')
        # Model process termination after the first rename, before completion.
        journal = '0123456789abcdef0123456789abcdef'
        self.run_ps("$plan = New-GloamOrganizerPlan $root 'roms/Nintendo DS' Alphabetize; "
                    + "$folder = Get-GloamPath $root '_nds/dsi-frontend/organizer/" + journal + "'; "
                    + '[IO.Directory]::CreateDirectory($folder) | Out-Null; '
                    + "Write-GloamOrganizerJson (Join-Path $folder 'plan.json') $plan; "
                    + '$move = $plan.Moves[0]; $target = Get-GloamPath $root $move.Destination; '
                    + '[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null; '
                    + '[IO.File]::Move((Get-GloamPath $root $move.Source),$target); '
                    + 'Undo-GloamOrganization $root ' + literal(journal))
        self.assertTrue((self.sd / 'roms/Nintendo DS/Moon.nds').is_file())
        self.assertTrue((self.sd / 'roms/Nintendo DS/Moon.sav').is_file())

    def test_organizer_rejects_library_roots_and_support_folders(self):
        for folder in ['roms', 'apps', 'roms/ScummVM', 'roms/SNES/bios', 'roms/NDS/saves', '../escape']:
            self.run_ps('Get-GloamOrganizerFolder $root ' + literal(folder), success=False)
        self.organizer_file('_nds/dsi-frontend/tabs.ini', b'[roots]\ngames=sd:/roms/My Games\n')
        self.organizer_file('roms/My Games/NDS/Example.nds')
        self.run_ps("Get-GloamOrganizerFolder $root 'roms/My Games'", success=False)
        self.run_ps("Get-GloamOrganizerFolder $root 'roms/My Games/NDS'")

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

    def test_worker_installs_bundled_catalog_app_and_preserves_existing_files(self):
        payload = b'Authored A7800DS installer fixture'
        digest = hashlib.sha256(payload).hexdigest()
        cache = self.root / 'cache'
        cache.mkdir()
        (cache / 'a7800-fixture.nds').write_bytes(payload)
        app = dict(id='a7800ds', version='fixture', terms='Authored', noncommercial=False,
                   repository='https://example.invalid', files=[{
                       'format': 'raw', 'destination': '_nds/dsi-frontend/plugins/apps/A7800DS.nds',
                       'sha256': digest, 'download': {'asset': 'a7800-fixture.nds',
                       'url': 'https://example.invalid/a7800-fixture.nds', 'sha256': digest}}])
        manifest = self.installer / 'downloads.json'
        manifest.write_text(json.dumps({'format': 1, 'applications': [], 'updates': [app], 'manual': []}))
        before = manifest.read_bytes()
        job = dict(Action='InstallAddons', Target=win(self.sd), ReleaseDirectory=win(self.root),
                   Manifest=win(manifest), Cache=win(cache), Selected=['a7800ds'],
                   Noncommercial=False, Offline=True)
        request = self.root / 'job.json'
        request.write_text(json.dumps(job))
        command = '& ' + literal(win(self.installer / 'gloam-setup-worker.ps1')) + ' -Request ' + literal(win(request))
        self.run_ps(command)
        installed = self.sd / app['files'][0]['destination']
        self.assertEqual(installed.read_bytes(), payload)
        installed.write_bytes(b'Keep the existing custom executable')
        self.run_ps(command)
        self.assertEqual(installed.read_bytes(), b'Keep the existing custom executable')
        self.assertEqual(manifest.read_bytes(), before)
        app['noncommercial'] = True
        manifest.write_text(json.dumps({'format': 1, 'applications': [], 'updates': [app], 'manual': []}))
        installed.unlink()
        self.run_ps(command, success=False)
        self.assertFalse(installed.exists())

    def test_gui_install_failure_log_is_plain_text(self):
        # This recommended app is present only in the complete GUI catalog.
        # A deliberately bad inline hash must reach staging, then fail legibly.
        app = dict(id='gameyob-dsi', version='fixture', terms='Authored', noncommercial=False,
                   repository='https://example.invalid', files=[{
                       'format': 'inline', 'content': 'Authored fixture', 'sha256': '0' * 64,
                       'destination': '_nds/dsi-frontend/plugins/dslibris-books.ini'}])
        good = dict(id='nesds', version='fixture', terms='Authored', noncommercial=False,
                    repository='https://example.invalid', files=[{
                        'format': 'inline', 'content': '# Authored valid fixture',
                        'sha256': hashlib.sha256(b'# Authored valid fixture').hexdigest(),
                        'destination': '_nds/dsi-frontend/plugins/picodrivetwl.ini'}])
        (self.installer / 'downloads.json').write_text(json.dumps(
            {'format': 1, 'applications': [], 'updates': [app, good], 'manual': []}))
        jobs = self.root / 'jobs'
        gui = literal(win(self.installer / 'gloam-setup.ps1'))
        output = self.run_ps('& ' + gui + ' -Target $root -Offline -SmokeTest -SmokeAction InstallAddons -JobDirectory ' + literal(win(jobs)))
        self.assertIn('GUI worker exit: 2', output)
        log = ''.join(path.read_text(encoding='utf-8') for path in jobs.rglob('*.log'))
        self.assertIn('hash', log.lower())
        self.assertNotIn('No pinned download', log)
        self.assertNotIn('CLIXML', log)
        self.assertNotIn('<Objs', log)
        self.assertIn('nesds: installed/updated', log)
        self.assertFalse((self.sd / '_nds/dsi-frontend/plugins/dslibris-books.ini').exists())
        self.assertEqual((self.sd / '_nds/dsi-frontend/plugins/picodrivetwl.ini').read_bytes(), b'# Authored valid fixture')

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

    def test_disabled_dropdown_keeps_display_name_and_text_field_value(self):
        xaml = literal(win(self.installer / 'gloam-setup.xaml'))
        self.run_ps('Add-Type -AssemblyName PresentationFramework; '
                    + '$window = [Windows.Markup.XamlReader]::Parse([IO.File]::ReadAllText(' + xaml + ')); '
                    + "$window.FindName('PageSetup').Visibility = 'Collapsed'; "
                    + "$window.FindName('PageLibrary').Visibility = 'Visible'; "
                    + "$combo = $window.FindName('Profiles'); "
                    + "$combo.ItemsSource = @([pscustomobject]@{Name='Example player';Id='internal-id'}); "
                    + '$combo.SelectedIndex = 0; '
                    + "$field = $window.FindName('ProfileName'); $field.Text = 'Example player'; "
                    + 'function Read-RenderedText($node) { '
                    + 'if ($node -is [Windows.Controls.TextBlock]) { $node.Text }; '
                    + 'for ($i=0; $i -lt [Windows.Media.VisualTreeHelper]::GetChildrenCount($node); $i++) { '
                    + 'Read-RenderedText ([Windows.Media.VisualTreeHelper]::GetChild($node,$i)) } }; '
                    + 'try { $window.Show(); foreach ($enabled in @($false,$true)) { '
                    + '$combo.IsEnabled=$enabled; $field.IsEnabled=$enabled; $window.UpdateLayout(); '
                    + "if ('Example player' -notin @(Read-RenderedText $combo)) { throw 'Dropdown lost its display name' }; "
                    + "if ($field.Text -cne 'Example player') { throw 'Text field value changed' } "
                    + '} } finally { $window.Close() }')

    def test_gui_loads_every_page_and_runs_read_only_worker_without_blocking(self):
        gui = literal(win(self.installer / 'gloam-setup.ps1'))
        for page in ['Setup', 'Addons', 'Library', 'Organizer', 'Settings', 'Themes', 'Maintenance']:
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

    def test_settings_edit_preserves_repeated_records_and_unknown_fields(self):
        state = self.sd / '_nds/dsi-frontend/state.ini'
        state.parent.mkdir(parents=True)
        state.write_text('version=1\nsettings_mode=advanced\nfavorite=sd:/roms/one.nds\n'
                         'favorite=sd:/apps/two.nds\nlocation=1,2,3,sd:/roms\n'
                         'tab_location=books,1,0,sd:/media/books\nfuture_option=keep\n'
                         'simple_settings_lock=0\nmusic_volume=70\n')
        self.run_ps("$state = Read-GloamState $root; Save-GloamSettings $root @{simple_settings_lock='1';music_volume='35'} $state.Hash")
        data = state.read_text()
        self.assertTrue(data.startswith('version=1\n'))
        self.assertIn('settings_mode=simple', data)
        self.assertIn('simple_settings_lock=1', data)
        self.assertEqual(data.count('favorite='), 2)
        self.assertIn('tab_location=books,1,0,sd:/media/books', data)
        self.assertIn('future_option=keep', data)
        self.assertIn('music_volume=35', data)
        self.assertEqual(len(list((self.sd / '_nds/dsi-frontend/setup-backups').rglob('state.ini'))), 1)
        self.run_ps("$state = Read-GloamState $root; Save-GloamSettings $root @{simple_settings_lock='0';settings_mode='advanced'} $state.Hash")
        self.assertIn('settings_mode=advanced', state.read_text(encoding='utf-8'))
        self.assertIn('simple_settings_lock=0', state.read_text(encoding='utf-8'))

    def test_settings_defaults_validation_and_conflict_detection(self):
        self.run_ps("$s=Read-GloamState $root; if ($s.Values.settings_mode -ne 'simple' -or $s.Values.simple_settings_lock -ne '0') {throw 'Wrong defaults'}; Save-GloamSettings $root (Get-GloamSetupPreset 'Quiet') $s.Hash")
        state = self.sd / '_nds/dsi-frontend/state.ini'
        before = state.read_bytes()
        for values in ["@{music_volume='101'}", "@{settings_mode='unknown'}", "@{theme_id='../escape'}", "@{favorite='deleted'}"]:
            self.run_ps('$s=Read-GloamState $root; Save-GloamSettings $root ' + values + ' $s.Hash', success=False)
            self.assertEqual(state.read_bytes(), before)
        self.run_ps("$s=Read-GloamState $root; [IO.File]::AppendAllText((Join-Path $root '_nds/dsi-frontend/state.ini'), 'external=keep'); Save-GloamSettings $root @{settings_mode='advanced'} $s.Hash", success=False)
        self.assertTrue(state.read_text().endswith('external=keep'))

    def test_native_configuration_interoperates_with_python_and_preserves_saves(self):
        import sys
        sys.path.insert(0, str(ROOT / 'tools'))
        from native_configuration import decode_records, encode_records
        private = self.sd / '_nds/dsi-frontend'
        private.mkdir(parents=True)
        (private / 'state.ini').write_bytes(b'version=1\nsettings_mode=simple\n')
        (private / 'appearance-1.ini').write_bytes(b'')
        save = self.sd / 'roms/saves/personal.sav'
        save.parent.mkdir(parents=True)
        save.write_bytes(b'Authored save fixture')
        export = self.root / 'export.dsconfig'
        self.run_ps('Export-GloamConfiguration $root ' + literal(win(export)))
        decoded = decode_records(export.read_bytes())
        self.assertEqual(decoded['state.ini'], (private / 'state.ini').read_bytes())
        self.assertEqual(decoded['appearance-1.ini'], b'')
        self.assertEqual(len(decoded), 2)
        imported = self.root / 'import.dsconfig'
        imported.write_bytes(encode_records({'state.ini': b'version=1\nsimple_settings_lock=1\n', 'tabs.ini': b'[tabs]\nversion=1\n'}))
        self.run_ps('Import-GloamConfiguration $root ' + literal(win(imported)) + ' (Get-GloamHash ' + literal(win(imported)) + ')')
        self.assertEqual((private / 'state.ini').read_bytes(), b'version=1\nsimple_settings_lock=1\n')
        self.assertEqual(save.read_bytes(), b'Authored save fixture')
        original = (private / 'state.ini').read_bytes()
        corrupt = bytearray(imported.read_bytes())
        corrupt[-1] ^= 1
        imported.write_bytes(corrupt)
        self.run_ps('Import-GloamConfiguration $root ' + literal(win(imported)) + ' (Get-GloamHash ' + literal(win(imported)) + ')', success=False)
        self.assertEqual((private / 'state.ini').read_bytes(), original)
        self.assertEqual(save.read_bytes(), b'Authored save fixture')

    def test_theme_selection_and_music_match_launcher_constraints(self):
        import wave
        theme = self.sd / '_nds/dsi-frontend/themes/test/theme.ini'
        theme.parent.mkdir(parents=True)
        theme.write_text('[theme]\nversion=1\n[layout]\nview=grid\nicon_size=large\n')
        self.run_ps("$s=Read-GloamState $root; Save-GloamSettings $root (Get-GloamThemeSettings $root test $true) $s.Hash")
        state = self.sd / '_nds/dsi-frontend/state.ini'
        self.assertIn('browser_style=3', state.read_text(encoding='utf-8'))
        self.assertIn('icon_size=1', state.read_text(encoding='utf-8'))
        wav = self.sd / 'media/music/fixture.wav'
        wav.parent.mkdir(parents=True)
        with wave.open(str(wav), 'wb') as output:
            output.setnchannels(1); output.setsampwidth(2); output.setframerate(8000)
            output.writeframes(b'\0\0' * 32)
        self.run_ps("$s=Read-GloamState $root; Save-GloamSettings $root @{music_mode='2';music_path='sd:/media/music/fixture.wav'} $s.Hash")
        before = state.read_bytes()
        import struct
        malformed = bytearray(wav.read_bytes() + b'JUNK' + struct.pack('<I', 1) + b'x')
        struct.pack_into('<I', malformed, 4, len(malformed) - 8)
        wav.write_bytes(malformed)
        self.run_ps("$s=Read-GloamState $root; Save-GloamSettings $root @{music_mode='2'} $s.Hash", success=False)
        self.assertEqual(state.read_bytes(), before)
        wav.write_bytes(b'Not audio')
        self.run_ps("$s=Read-GloamState $root; Save-GloamSettings $root @{music_mode='2'} $s.Hash", success=False)
        self.assertEqual(state.read_bytes(), before)

    def test_gui_stages_and_saves_the_simple_locked_preset(self):
        state = self.sd / '_nds/dsi-frontend/state.ini'
        state.parent.mkdir(parents=True)
        state.write_text('version=1\nsettings_mode=advanced\nfavorite=sd:/roms/one.nds\nfavorite=sd:/roms/two.nds\n')
        gui = literal(win(self.installer / 'gloam-setup.ps1'))
        output = self.run_ps('& ' + gui + ' -Target $root -SmokeTest -SmokeAction SaveSettings')
        self.assertIn('GUI settings save completed.', output)
        self.assertIn('simple_settings_lock=1', state.read_text(encoding='utf-8'))
        self.assertIn('settings_mode=simple', state.read_text(encoding='utf-8'))
        self.assertEqual(state.read_text().count('favorite='), 2)

    def test_pending_recovery_blocks_settings_edits_and_state_backup_is_validated(self):
        state = self.sd / '_nds/dsi-frontend/state.ini'
        state.parent.mkdir(parents=True)
        state.write_bytes(b'version=1\nmusic_volume=70\n')
        backup = state.with_suffix('.bak')
        backup.write_bytes(b'version=1\nmusic_volume=25\n')
        pending = state.parent / 'configuration/pending.dsconfig'
        pending.parent.mkdir()
        pending.write_bytes(b'Authored recovery marker')
        self.run_ps("$s=Read-GloamState $root; Save-GloamSettings $root @{music_volume='40'} $s.Hash", success=False)
        self.assertEqual(state.read_bytes(), b'version=1\nmusic_volume=70\n')
        pending.unlink()
        self.run_ps('Restore-GloamStateBackup $root')
        self.assertEqual(state.read_bytes(), b'version=1\nmusic_volume=25\n')
        backup.write_bytes(b'Invalid backup')
        self.run_ps('Restore-GloamStateBackup $root', success=False)
        self.assertEqual(state.read_bytes(), b'version=1\nmusic_volume=25\n')

    def test_failed_import_rolls_back_and_terminated_import_keeps_native_journal(self):
        import sys
        sys.path.insert(0, str(ROOT / 'tools'))
        from native_configuration import decode_records, encode_records
        private = self.sd / '_nds/dsi-frontend'
        private.mkdir(parents=True)
        old = b'version=1\nmusic_volume=70\n'
        new = b'version=1\nmusic_volume=20\n'
        (private / 'state.ini').write_bytes(old)
        archive = self.root / 'incoming.dsconfig'
        archive.write_bytes(encode_records({'state.ini': new, 'tabs.ini': b'[tabs]\nversion=1\n'}))
        wrapper = "$script:writer=${function:Write-GloamFile}; function Write-GloamFile {param($Root,$Relative,$Data,$Replace=$false); if ($Relative -eq '_nds/dsi-frontend/tabs.ini') { ACTION }; & $script:writer $Root $Relative $Data $Replace }; "
        command = 'Import-GloamConfiguration $root ' + literal(win(archive)) + ' (Get-GloamHash ' + literal(win(archive)) + ')'
        self.run_ps(wrapper.replace('ACTION', "throw 'Authored write failure'") + command, success=False)
        self.assertEqual((private / 'state.ini').read_bytes(), old)
        self.assertFalse((private / 'configuration/pending.dsconfig').exists())
        self.run_ps(wrapper.replace('ACTION', 'exit 42') + command, success=False)
        self.assertEqual((private / 'state.ini').read_bytes(), new)
        self.assertEqual(decode_records((private / 'configuration/pending.dsconfig').read_bytes())['state.ini'], new)
        self.assertEqual(decode_records((private / 'configuration/rollback.dsconfig').read_bytes())['state.ini'], old)
        self.run_ps("$s=Read-GloamState $root; Save-GloamSettings $root @{music_volume='50'} $s.Hash", success=False)
        self.assertEqual((private / 'state.ini').read_bytes(), new)

    def test_read_only_setup_check_retains_unsaved_settings(self):
        gui = literal(win(self.installer / 'gloam-setup.ps1'))
        output = self.run_ps('& ' + gui + ' -Target $root -Offline -SmokeTest -SmokeAction CheckSetup -SmokeDirtySettings -JobDirectory ' + literal(win(self.root / 'jobs')))
        self.assertIn('GUI worker exit: 0', output)
        self.assertEqual(list(self.sd.iterdir()), [])


    def test_gui_release_lookup_requests_a_fresh_listing(self):
        directory = self.root / 'release-lookup'
        code = "function Invoke-RestMethod { param($Uri,$UserAgent,$TimeoutSec); if ($Uri -notmatch 'setup_refresh=[0-9]+') { throw 'Cached release listing used' }; return @([pscustomobject]@{draft=$false; tag_name='v0.1.0-preview.8'; published_at='2026-11-10T00:00:00Z'; assets=@([pscustomobject]@{name='addons.zip'})}) }; "
        self.run_ps(code + '$release=Get-GloamRelease ' + literal(win(directory)) + "; if ($release.tag_name -ne 'v0.1.0-preview.8') { throw 'Wrong release' }")
