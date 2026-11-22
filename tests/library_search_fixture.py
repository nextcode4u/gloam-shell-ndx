"""Original metadata/text fixtures, never games or firmware."""
from pathlib import Path
import struct
import sys


def fnv(data, value=2166136261):
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def package(records, count=None):
    payload = bytearray()
    for path, flags in records:
        data = path.encode('utf-8') if isinstance(path, str) else path
        attrs = struct.pack('<H', flags)
        payload += struct.pack('<H', len(data)) + attrs + struct.pack('<I', fnv(attrs + data)) + data
    header = struct.pack('<8sIIQI', b'GLIDX001', len(records) if count is None else count,
                         len(payload), 1790000000, 0)
    return header + struct.pack('<I', fnv(header)) + payload


def fold(c):
    if (65 <= c <= 90 or (0xc0 <= c <= 0xde and c != 0xd7) or
            (0x391 <= c <= 0x3ab and c != 0x3a2) or 0x410 <= c <= 0x42f):
        return c + 32
    return 0x451 if c == 0x401 else c


def blocked_package(records):
    payload = package(records)[32:]
    blocks = bytearray()
    offset = 32 + ((len(records) + 63) // 64) * 276
    for first in range(0, len(records), 64):
        chunk = records[first:first + 64]
        bits = bytearray(256)
        size = 0
        for path, flags in chunk:
            data = path.encode('utf-8')
            size += 8 + len(data)
            chars = [fold(ord(c)) for c in path]
            for pos in range(len(chars) - 2):
                h = 2166136261
                for c in chars[pos:pos+3]:
                    h = ((h ^ c) * 16777619) & 0xffffffff
                for bit in (h & 2047, (h >> 11) & 2047):
                    bits[bit // 8] |= 1 << (bit % 8)
        entry = struct.pack('<IIII', offset, first, len(chunk), size) + bits
        blocks += entry + struct.pack('<I', fnv(entry))
        offset += size
    header = struct.pack('<8sIIQI', b'GLIDX002', len(records), len(payload), 1790000000,
                         (len(records) + 63) // 64)
    return header + struct.pack('<I', fnv(header)) + blocks + payload


def prepare(root):
    root.mkdir(parents=True, exist_ok=True)
    for name in ('roms', 'apps', 'media/books', 'extra', '_nds/dsi-frontend/cache'):
        (root / name).mkdir(parents=True, exist_ok=True)
    (root / '_nds/dsi-frontend/tabs.ini').write_text('[tabs]\nversion=1\n[tab.extra]\npath=extra\n[tab.collection]\npath=roms/Collection\n')
    rows = [(f'roms/Game{n:04}.txt', 0) for n in range(1300)]
    rows += [('apps/Player.nds', 0), ('media/books/Überblick.txt', 0), ('media/books/日本語.txt', 0),
             ('extra/Notes.md', 0), ('media/Standalone.png', 0), ('roms/Unsupported.weird', 0),
             ('roms/Collection/Deep/Scoped.nds', 0), ('roms/Collection-extra/Escape.txt', 0),
             ('roms/.Hidden/Hidden game.txt', 1), ('roms/System game.txt', 2),
             ('outside/Ignore game.txt', 0), ('roms/MovedAway.txt', 0),
             ('roms/SAVES/Nested/SaveNeedle.sav', 0),
             ('roms/Console/SaveStates/StateNeedle.bin', 0),
             ('roms/Console/BIOS/FirmwareNeedle.bin', 0),
             ('roms/Biosphere/VisibleNeedle.nds', 0),
             ('roms/saves.nds', 0), ('roms/bios.nds', 0)]
    for name, _ in rows:
        if name.startswith('outside/') or name.endswith('MovedAway.txt'):
            continue
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('Original non-runnable test data.\n')
    (root / "legacy.gli").write_bytes(package(rows))
    plugins = root / '_nds/dsi-frontend/plugins'
    plugins.mkdir()
    for n, app in enumerate(('Player','Alternate')):
        (root / 'apps' / (app + '.nds')).write_text('Authored non-runnable application placeholder')
        (plugins / f'reader{n}.ini').write_text(
            '[plugin]\nversion=1\nname=Reader '+app+'\nextensions=md\n'
            'application=sd:/apps/'+app+'.nds\nargv=1\n')
    data = blocked_package(rows)
    (root / 'valid.gli').write_bytes(data)
    (root / '_nds/dsi-frontend/cache/library.gli').write_bytes(data)
    bad = bytearray(package(rows))
    bad[-1] ^= 1
    (root / 'bad-checksum.gli').write_bytes(bad)
    (root / 'bad-count.gli').write_bytes(package(rows, len(rows) + 1))
    (root / 'bad-zero-count.gli').write_bytes(package(rows, 0))
    (root / 'empty.gli').write_bytes(package([]))
    (root / 'large.gli').write_bytes(blocked_package(
        [(f'roms/Title{n:05}.nds', 0) for n in range(49999)] + [('roms/FinalNeedle.nds', 0)]))
    bad_table = bytearray(data)
    bad_table[32 + ((len(rows)+63)//64)*276 - 1] ^= 1
    (root / 'bad-table.gli').write_bytes(bad_table)
    for n, (field, value) in enumerate([(0, 0), (4, 1), (8, 65), (12, 0),
                                          (0, 0xffffffff), (12, 0xffffffff)]):
        changed = bytearray(data)
        struct.pack_into('<I', changed, 32+field, value)
        struct.pack_into('<I', changed, 32+272, fnv(changed[32:32+272]))
        (root / f'bad-block-{n}.gli').write_bytes(changed)
    for n, (path, flags) in enumerate([(b'../outside', 0), (b'/absolute', 0), (b'roms/../outside', 0),
                                      (b'roms/\x80.txt', 0), (b'roms/a\x00b.txt', 0),
                                      (b'roms/a.txt', 4), (b'roms/a.txt', 256),
                                      (b'roms/' + b'x' * 768, 0)]):
        (root / f'bad-path-{n}.gli').write_bytes(package([(path, flags)]))
    for n in range(32):
        (root / f'truncated-{n}.gli').write_bytes(data[:n])


if __name__ == '__main__':
    prepare(Path(sys.argv[1]))
