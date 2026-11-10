#!/usr/bin/env python3
"""Verify a complete release set before uploading or distributing its files."""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import re
import stat
import zipfile

BASE_FILES = {'BOOT.NDS', 'sd.zip', 'source.zip', 'licenses.zip'}


def checksum(stream):
    value = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        value.update(block)
    return value.hexdigest()


def checked_archive(path):
    archive = zipfile.ZipFile(path)
    try:
        names = set()
        for item in archive.infolist():
            name = item.filename
            if (name in names or '\\' in name or name.startswith('/')
                    or '..' in PurePosixPath(name).parts
                    or stat.S_ISLNK(item.external_attr >> 16)):
                raise ValueError('Unsafe or duplicate archive member: ' + name)
            names.add(name)
        failed = archive.testzip()
        if failed:
            raise ValueError('Archive CRC mismatch: ' + failed)
    except Exception:
        archive.close()
        raise
    return archive


def verify(directory, addons=False):
    directory = Path(directory)
    expected = BASE_FILES | ({'addons.zip'} if addons else set())
    hashes = {}
    for line in (directory / 'SHA256SUMS').read_text().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9_.-]+)', line)
        if not match or match[2] not in expected or match[2] in hashes:
            raise ValueError('Invalid or unexpected checksum entry')
        hashes[match[2]] = match[1]
    if hashes.keys() != expected:
        raise ValueError('Incomplete release: binaries, source and licenses must accompany each other')
    for name, digest in hashes.items():
        with (directory / name).open('rb') as source:
            if checksum(source) != digest:
                raise ValueError('Release checksum mismatch: ' + name)
    if (directory / 'BOOT.NDS').stat().st_size < 512:
        raise ValueError('Truncated frontend binary')
    with checked_archive(directory / 'sd.zip') as sd:
        with sd.open('BOOT.NDS') as source:
            if checksum(source) != hashes['BOOT.NDS']:
                raise ValueError('Standalone and SD frontend binaries differ')
        required = {'_nds/dsi-frontend/install/gloam-forwarder.dsi',
                    '_nds/dsi-frontend/install/gloam-forwarder.tmd',
                    '_nds/nds-bootstrap-release.nds',
                    '_nds/dsi-frontend/loaders/slot1launch.nds'}
        if not required <= set(sd.namelist()):
            raise ValueError('SD package is missing required launch/install files')
        if ('_nds/dsi-frontend/state.ini' in sd.namelist()
                or any(name.startswith(('docs/', 'licenses/')) for name in sd.namelist())):
            raise ValueError('SD package contains personal state or repository documentation')
    with checked_archive(directory / 'source.zip') as source:
        if not {'LICENSE', 'package-manifest.json', 'config/package-inputs.json'} <= set(source.namelist()):
            raise ValueError('Source package is incomplete')
        for dependency in ['nds-bootstrap', 'slot1launcher']:
            if not any(n.startswith('dependencies/' + dependency + '/') for n in source.namelist()):
                raise ValueError('Missing dependency source: ' + dependency)
    with checked_archive(directory / 'licenses.zip') as notices:
        if not {'LICENSE', 'CREDITS.md', 'THIRD_PARTY.md'} <= set(notices.namelist()):
            raise ValueError('Missing release license/attribution')
    if addons:
        with checked_archive(directory / 'addons.zip') as archive:
            required = {'addon-installer/Start Add-on Installer.cmd',
                        'addon-installer/download-addons.py', 'addon-installer/download-addons.ps1',
                        'addon-installer/downloads.json'}
            if not required <= set(archive.namelist()):
                raise ValueError('Add-on installer is incomplete')
    print('Verified complete release set: ' + ', '.join(sorted(expected)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path('dist'))
    parser.add_argument('--addons', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.directory, args.addons)
    except (OSError, KeyError, ValueError, zipfile.BadZipFile) as error:
        raise SystemExit(str(error))
