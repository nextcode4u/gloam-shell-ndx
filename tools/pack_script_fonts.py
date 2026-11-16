#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare the external Latin Extended-A/Greek/Cyrillic face from reviewed rows."""
import argparse
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'sdroot/_nds/dsi-frontend/fonts/extended.gsf'


def generate():
    source = (ROOT / 'arm9/source/ui/font_extended.h').read_text()
    flags = [int(v) for v in re.search(r'supported\[528\] = \{(.*?)\}', source, re.S)[1].split(',') if v.strip()]
    rows = re.findall(r'\{((?:0x[0-9a-f]+,?\s*)+)\}, /\* U\+([0-9A-F]+) \*/', source)
    assert len(flags) == len(rows) == 528
    glyphs = []
    for supported, (values, cp) in zip(flags, rows):
        if supported:
            bits = [int(v.strip(), 16) for v in values.split(',') if v.strip()]
            glyphs.append(struct.pack('<IB12H', int(cp, 16), 7, *bits))
    data = b''.join(glyphs)
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    return struct.pack('<8sIIBB2x', b'GSFONT01', len(glyphs), checksum, 12, 2) + data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = generate()
    if args.check:
        if OUTPUT.read_bytes() != data:
            raise SystemExit('Regenerate fonts with tools/pack_script_fonts.py')
    else:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_bytes(data)
    print('Extended script font pack verified.')


if __name__ == '__main__':
    main()
