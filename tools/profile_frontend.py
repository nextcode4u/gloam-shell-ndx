#!/usr/bin/env python3
"""Profile the host renderer against authored libraries, never an SD card."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--items', type=int, default=1000)
    parser.add_argument('--mixed', action='store_true', help='Include nested folders, images and mixed suffixes')
    parser.add_argument('--cycles', type=int, default=0, help='Theme/navigation stress cycles per run (0-1000)')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/frontend-profile.json')
    args = parser.parse_args()
    if not 1 <= args.items <= 10000:
        parser.error('--items must be 1-10000')
    if not 0 <= args.cycles <= 1000:
        parser.error('--cycles must be 0-1000')
    preview = ROOT / 'build/host/preview'
    if not preview.exists():
        parser.error('Run tools/host-check.sh first.')
    results = {'environment': 'Host CPU/filesystem; not DSi timings or total peak RAM', 'items': args.items, 'themes': {}}
    with tempfile.TemporaryDirectory(prefix='frontend-profile-', dir=ROOT / 'build') as temporary:
        card = Path(temporary)
        shutil.copytree(ROOT / 'sdroot', card, dirs_exist_ok=True)
        for index in range(args.items):
            suffix = ('.txt', '.md', '.gba', '.mp3')[index % 4] if args.mixed else '.txt'
            item = card / 'roms' / f'Fixture {index:05d}{suffix}'
            item.write_bytes(b'Authored profiling fixture; not a playable ROM')
            if args.mixed and index % 8 == 0:
                shutil.copyfile(ROOT / 'tests/fixtures/cover-example.png', Path(str(item)+'.png'))
            if args.mixed and index % 100 == 0:
                nested = card / 'roms' / f'Collection {index:05d}' / 'Nested'
                nested.mkdir(parents=True)
                (nested / 'Readme.txt').write_text('Authored nested fixture')
        for theme in ('phosphor', 'eclipse', 'prism'):
            (card / '_nds/dsi-frontend/state.ini').write_text(
                f'version=1\ntheme_id={theme}\ncategory=1\nbrowser_style=3\nicon_size=1\n')
            output = subprocess.check_output([str(preview), str(card), str(card/'unused'), '1', 'profile', str(args.cycles)], text=True)
            results['themes'][theme] = json.loads(output)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
