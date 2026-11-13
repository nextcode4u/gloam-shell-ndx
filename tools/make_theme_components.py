#!/usr/bin/env python3
"""Build original modular nine-slice UI artwork for the bundled themes (CC0)."""
import argparse
import configparser
import io
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / 'sdroot/_nds/dsi-frontend/themes'
COMPONENTS = ('header', 'footer', 'panel', 'row-selected', 'tab-selected', 'card', 'card-selected')


def component_image(name, style, colors):
    image = Image.new('RGBA', (32, 32))
    draw = ImageDraw.Draw(image)
    bg, surface, line, accent = (colors[key] for key in ('background', 'surface', 'line', 'accent'))
    selected = name in ('row-selected', 'card-selected')
    fill = accent if name == 'tab-selected' else surface
    edge = accent if selected or name == 'panel' else line
    if style == 'terminal':
        draw.rectangle((0, 0, 31, 31), fill=fill)
        if selected:
            # Open brackets rather than the stock filled strip.
            for x,side in ((0,1),(31,-1)):
                draw.line([(x+4*side,0),(x,0),(x,31),(x+4*side,31)], fill=accent)
        elif name in ('header', 'footer'):
            y = 30 if name == 'header' else 0
            draw.line((0,y,31,y), fill=accent)
        elif name != 'tab-selected':
            draw.rectangle((0,0,31,31),outline=edge)
    elif style == 'lunar':
        draw.rounded_rectangle((0,0,31,31),radius=7,fill=fill,outline=edge)
        if selected or name == 'panel':
            draw.arc((2,2,15,15),180,270,fill=accent,width=2)
            draw.arc((16,16,29,29),0,90,fill=accent,width=2)
        if name == 'header':
            draw.line((8,30,23,30),fill=accent)
    elif style == 'paper':
        draw.polygon([(0,0),(25,0),(31,6),(31,31),(0,31)],fill=fill,outline=edge)
        draw.polygon([(25,0),(25,6),(31,6)],fill=accent if selected else line)
        if selected:
            draw.line((1,30,30,30),fill=accent,width=2)
    else:
        draw.rounded_rectangle((0,0,31,31),radius=3,fill=fill,outline=edge)
        draw.line([(2,28),(2,2),(28,2)],fill=accent if selected else line)
        draw.line([(3,30),(30,30),(30,3)],fill=bg,width=2)
    return image


def assets():
    for manifest in sorted(THEMES.glob('*/theme.ini')):
        config = configparser.ConfigParser()
        config.read(manifest)
        if not config.has_section('components'):
            continue
        theme = manifest.parent.name
        style = ('terminal' if theme in ('phosphor', 'slate') else
                 'lunar' if theme in ('eclipse', 'nocturne') else
                 'paper' if theme in ('prism', 'folio', 'tide', 'glacier') else 'retro')
        for name in COMPONENTS:
            output = io.BytesIO()
            component_image(name, style, config['colors']).save(output, format='PNG', optimize=True)
            yield manifest.parent / 'components' / (name + '.png'), output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    count = 0
    for path, data in assets():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise SystemExit(f'Component differs: {path.relative_to(ROOT)}')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        count += 1
    print(f'{"Checked" if args.check else "Wrote"} {count} theme components')


if __name__ == '__main__':
    main()
