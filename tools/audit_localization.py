#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit external catalogs for native bounds, coverage, glyphs and fixed UI widths."""
import argparse
import configparser
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
BASIC = ('home', 'games', 'apps', 'media', 'system', 'files', 'settings',
         'reading_folder', 'browser_hint', 'tab_hint', 'installed_titles', 'empty_folder')


def catalog(path):
    data = Path(path).read_bytes()
    if not 0 < len(data) <= 32768:
        raise ValueError('Catalog must contain 1–32768 bytes')
    if b'\0' in data:
        raise ValueError('Catalog contains embedded NUL bytes')
    text = data.decode('utf-8-sig')
    values = {}
    extra = used = 0
    for raw in text.split("\n"):
        if len(raw.encode('utf-8')) >= 511:
            raise ValueError('Catalog line exceeds the native read buffer')
        line = raw[:-1] if raw.endswith('\r') else raw
        if not line or line.startswith((';', '#')):
            continue
        key, separator, value = line.partition('=')
        if not separator or key in values:
            raise ValueError('Missing separator or duplicate key: ' + key)
        if key == 'version':
            if value != '1':
                raise ValueError('Expected catalog version 1')
            values[key] = value
            continue
        if key not in BASIC and (len(key) >= 48 or not re.fullmatch(r'(setting|section|dialog|value)\.[a-z0-9_.]+', key)):
            raise ValueError('Unsupported key: ' + key)
        size = len(value.encode('utf-8'))
        cap = 64 if key in BASIC[:6] else 256 if '.help' in key else 96
        if not 0 < size < cap or any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise ValueError('Invalid text length/control character: ' + key)
        if key not in BASIC:
            extra += 1
            used += size + 1
        values[key] = value
    if values.pop('version', None) != '1' or extra > 384 or used > 24576:
        raise ValueError('Catalog version or native storage limit exceeded')
    return values


def known_keys():
    keys = set(catalog(ROOT / 'config/languages/template.ini'))
    for path in (ROOT / 'arm9/source').rglob('*.c'):
        keys.update(re.findall(r'"((?:setting|section|dialog|value)\.[a-z0-9_.]+)"', path.read_text()))
    source = (ROOT / 'arm9/source/settings.c').read_text()
    for identifier in re.findall(r'\[OPT_[A-Z_]+\]\s*=\s*"([a-z0-9_]+)"', source):
        keys.update(('setting.' + identifier + '.label', 'setting.' + identifier + '.help'))
    keys.update('section.simple_' + str(i) for i in range(4))
    return keys | set(BASIC)


def script_advances(directory=None):
    widths = {cp: 7 for cp in range(32, 127)} | {cp: 7 for cp in range(160, 256)}
    directory = directory or ROOT / 'sdroot/_nds/dsi-frontend/fonts'
    for filename, first, last in [('extended.gsf', 0x100, 0x4ff), ('japanese.gsf', 0x2000, 0xffff)]:
        path = directory / filename
        if not path.exists():
            continue
        data = path.read_bytes()
        if len(data) < 20:
            raise ValueError('Truncated font: ' + str(path))
        magic, count, checksum, height, depth = struct.unpack_from('<8sIIBB', data)
        if magic != b'GSFONT01' or not 0 < count <= 16384 or height != 12 or depth not in (1, 2) or data[18:20] != b'\0\0' or len(data) != 20 + count * 29:
            raise ValueError('Invalid font bounds: ' + str(path))
        actual = 2166136261
        for byte in data[20:]:
            actual = ((actual ^ byte) * 16777619) & 0xffffffff
        if actual != checksum:
            raise ValueError('Invalid font checksum: ' + str(path))
        previous = 0
        for offset in range(20, len(data), 29):
            cp, advance, *rows = struct.unpack_from('<IB12H', data, offset)
            if not first <= cp <= last or not previous < cp or 0xd800 <= cp <= 0xdfff or not 0 < advance * depth <= 16 or any(row >> (advance * depth) for row in rows):
                raise ValueError('Invalid font record: ' + str(path))
            widths[cp] = advance
            previous = cp
    return widths


def body_advances(path, shared):
    widths = dict(shared)
    data = path.read_bytes()
    if len(data) < 16:
        raise ValueError('Truncated theme font')
    magic = data[:8]
    if magic == b'DSFONT03':
        height = data[9]
        count = 191 if data[13] == 96 else 95
        stride = 2 + height * 4
        if not 12 <= height <= 16 or len(data) != 16 + count * stride:
            raise ValueError('Invalid proportional theme font')
        for glyph in range(count):
            advance = data[16 + glyph * stride]
            if not 1 <= advance <= 16:
                raise ValueError('Invalid theme advance')
            cp = glyph + 32 if glyph < 95 else glyph - 95 + 160
            widths[cp] = advance
        return widths, height
    if magic not in (b'DSFONT01', b'DSFONT02'):
        raise ValueError('Unknown theme font')
    return widths, 12


def audit(path, keys, shared, themes):
    values = catalog(path)
    unsupported = sorted({ord(c) for value in values.values() for c in value if ord(c) not in shared})
    overflows = []
    font_warnings = []
    # These controls have bounded widths. Overflow is advisory: some regions loop.
    regions = {'settings': 58, 'browser_hint': 236, 'tab_hint': 154}
    for theme in sorted(themes.glob('*/theme.ini')):
        config = configparser.ConfigParser(interpolation=None)
        config.read(theme, encoding='utf-8')
        widths, height = shared, 12
        if config.has_option('fonts', 'ui'):
            relative = config['fonts']['ui']
            if relative != 'builtin':
                font = (theme.parent / relative).resolve()
                if not font.is_relative_to(theme.parent.resolve()):
                    raise ValueError('Theme font escapes its package')
                widths, height = body_advances(font, shared)
        if height > 12:
            font_warnings.append({'theme': theme.parent.name, 'message': 'Body face exceeds 12 pixels; inspect fixed footer/dialog regions'})
        for key, available in regions.items():
            if key in values:
                pixels = sum(widths.get(ord(c), 7) for c in values[key])
                if pixels > available:
                    overflows.append({'theme': theme.parent.name, 'key': key, 'pixels': pixels, 'available': available})
    missing = sorted(keys - values.keys())
    return {'catalog': str(path), 'translated_keys': len(values), 'known_keys': len(keys),
            'coverage_percent': round(100 * len(values.keys() & keys) / len(keys), 1),
            'missing_keys': missing, 'unknown_keys': sorted(values.keys() - keys),
            'unsupported_glyphs': [{'codepoint': f'U+{cp:04X}', 'character': chr(cp)} for cp in unsupported],
            'potential_overflow': overflows, 'font_warnings': font_warnings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('catalogs', nargs='*', type=Path)
    parser.add_argument('--themes', type=Path, default=ROOT / 'sdroot/_nds/dsi-frontend/themes')
    parser.add_argument('--fonts', type=Path, default=ROOT / 'sdroot/_nds/dsi-frontend/fonts')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/localization-audit.json')
    parser.add_argument('--strict', action='store_true', help='Also fail for missing keys, unknown keys or unsupported glyphs')
    args = parser.parse_args()
    paths = args.catalogs or sorted((ROOT / 'sdroot/_nds/dsi-frontend/languages').glob('*.ini'))
    if not paths or not args.themes.is_dir() or not args.fonts.is_dir():
        parser.error('Choose existing catalogs, theme and font directories')
    keys, shared = known_keys(), script_advances(args.fonts)
    report = {'note': 'Width warnings need native review; looping labels can exceed their viewport.', 'catalogs': [], 'errors': []}
    for path in paths:
        try:
            row = audit(path, keys, shared, args.themes)
            report['catalogs'].append(row)
            print(f"{path.name}: {row['coverage_percent']}% coverage; {len(row['unsupported_glyphs'])} missing glyphs; {len(row['potential_overflow'])} width warnings")
        except (OSError, UnicodeError, ValueError) as error:
            report['errors'].append({'catalog': str(path), 'error': str(error)})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return int(bool(report['errors']) or (args.strict and any(row['missing_keys'] or row['unknown_keys'] or row['unsupported_glyphs'] for row in report['catalogs'])))


if __name__ == '__main__':
    raise SystemExit(main())
