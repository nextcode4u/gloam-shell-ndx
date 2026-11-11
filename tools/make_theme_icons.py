#!/usr/bin/env python3
"""Generate original theme icon variants from Gloam's shared symbol masks (CC0)."""
import argparse
import configparser
import io
import re
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / 'sdroot/_nds/dsi-frontend/themes'


def symbols():
    source = (ROOT / 'arm9/source/ui/item_icons.c').read_text()
    result = {}
    for name, values in re.findall(r'\[PLACEHOLDER_(\w+)\] = \{([^}]+)\}', source):
        result[name.lower()] = [int(v.strip(), 0) for v in values.split(',') if v.strip()]
    source = (ROOT / 'arm9/source/system_icons.c').read_text().split('masks[7][16] = {', 1)[1].split('};', 1)[0]
    source = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
    names = ('settings', 'camera', 'sound', 'pictochat', 'download-play', 'system-menu', 'slot1')
    for name, values in zip(names, re.findall(r'\{([^}]+)\}', source), strict=True):
        result[name] = [int(v.strip(), 0) for v in values.split(',') if v.strip()]
    assert len(result) == 22 and all(len(rows) == 16 for rows in result.values())
    return result


def assets():
    masks = symbols()
    for manifest in sorted(THEMES.glob('*/theme.ini')):
        config = configparser.ConfigParser()
        config.read(manifest)
        name = manifest.parent.name
        colors = config['colors']
        accent, surface, line = colors['accent'], colors['surface'], colors['line']
        for kind, rows in masks.items():
            image = Image.new('RGBA', (32, 32))
            draw = ImageDraw.Draw(image)
            # Quiet outlines for paper/terminal themes, framed badges elsewhere.
            if name in ('copper', 'ember', 'garnet'):
                draw.rounded_rectangle((1, 1, 30, 30), radius=4, fill=surface, outline=line)
                draw.line((6, 29, 25, 29), fill=accent)
            elif name in ('eclipse', 'nocturne'):
                draw.ellipse((1, 1, 30, 30), fill=surface, outline=line)
            elif name in ('prism', 'glacier', 'tide'):
                draw.polygon([(5, 1), (26, 1), (30, 5), (30, 26), (26, 30), (5, 30), (1, 26), (1, 5)], fill=surface, outline=line)
            mask = Image.new('RGBA', (16, 16))
            pixels = mask.load()
            tint = ImageColor.getrgb(accent) + (255,)
            for y, row in enumerate(rows):
                for x in range(16):
                    if row & (0x8000 >> x):
                        pixels[x, y] = tint
            # A full-size symbol stays recognizable at the 12/16px browser sizes.
            image.alpha_composite(mask.resize((32, 32), Image.Resampling.NEAREST))
            output = io.BytesIO()
            image.save(output, format='PNG', optimize=True)
            yield manifest.parent / 'icons' / (kind + '.png'), output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    count = 0
    for path, data in assets():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise SystemExit(f'Icon differs: {path.relative_to(ROOT)}')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        count += 1
    print(f'{"Checked" if args.check else "Wrote"} {count} theme icons')


if __name__ == '__main__':
    main()
