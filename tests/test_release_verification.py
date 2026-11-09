"""Keep complete release companions together and reject damaged upload sets."""
import hashlib
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.verify_release import verify


class ReleaseVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.binary = bytes(512)
        (self.root / 'BOOT.NDS').write_bytes(self.binary)
        self.sd = {'BOOT.NDS': self.binary,
                   '_nds/dsi-frontend/install/gloam-forwarder.dsi': b'Authored fixture',
                   '_nds/dsi-frontend/install/gloam-forwarder.tmd': b'Authored fixture',
                   '_nds/nds-bootstrap-release.nds': b'Authored fixture',
                   '_nds/dsi-frontend/loaders/slot1launch.nds': b'Authored fixture'}
        self.archive('sd.zip', self.sd)
        self.archive('source.zip', {'LICENSE': b'Fixture license', 'package-manifest.json': b'{}',
                                   'config/package-inputs.json': b'{}',
                                   'dependencies/nds-bootstrap/source.tar.gz': b'Fixture source',
                                   'dependencies/slot1launcher/source.tar.gz': b'Fixture source'})
        self.archive('licenses.zip', {'LICENSE': b'Fixture license', 'CREDITS.md': b'Fixture credits',
                                     'THIRD_PARTY.md': b'Fixture notices'})
        self.sums()

    def archive(self, name, files):
        with zipfile.ZipFile(self.root / name, 'w') as archive:
            for path, value in files.items():
                archive.writestr(path, value)

    def sums(self):
        files = sorted(p for p in self.root.iterdir() if p.name != 'SHA256SUMS')
        (self.root / 'SHA256SUMS').write_text(''.join(
            hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in files))

    def test_complete_set_and_optional_addons(self):
        verify(self.root)
        self.archive('addons.zip', {'addon-installer/download-addons.py': b'Fixture script',
                                   'addon-installer/download-addons.ps1': b'Fixture script',
                                   'addon-installer/downloads.json': b'{}'})
        self.sums()
        verify(self.root, addons=True)
        with self.assertRaisesRegex(ValueError, 'unexpected checksum'):
            verify(self.root)

    def test_corruption_and_missing_companions_fail(self):
        (self.root / 'BOOT.NDS').write_bytes(b'Damaged')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            verify(self.root)
        (self.root / 'BOOT.NDS').write_bytes(self.binary)
        path = self.root / 'SHA256SUMS'
        path.write_text('\n'.join(line for line in path.read_text().splitlines()
                                  if not line.endswith('licenses.zip')) + '\n')
        with self.assertRaisesRegex(ValueError, 'Incomplete release'):
            verify(self.root)

    def test_standalone_and_sd_binaries_must_match(self):
        self.archive('sd.zip', {**self.sd, 'BOOT.NDS': b'Another binary'})
        self.sums()
        with self.assertRaisesRegex(ValueError, 'binaries differ'):
            verify(self.root)

    def test_unsafe_members_and_personal_state_fail_even_with_correct_hashes(self):
        for path, message in [('../escape', 'Unsafe'), ('_nds/dsi-frontend/state.ini', 'personal state')]:
            self.archive('sd.zip', {**self.sd, path: b'Fixture'})
            self.sums()
            with self.assertRaisesRegex(ValueError, message):
                verify(self.root)


if __name__ == '__main__':
    unittest.main()
