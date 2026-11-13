#!/usr/bin/env python3
"""Generate original, theme-specific pixel icon artwork (CC0)."""
import argparse
import configparser
import io
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / 'sdroot/_nds/dsi-frontend/themes'


ICON_NAMES = (
    'file', 'folder', 'game', 'application', 'audio', 'video', 'book', 'text',
    'image', 'save', 'config', 'archive', 'shortcut', 'system', 'missing',
    'settings', 'camera', 'sound', 'pictochat', 'download-play', 'system-menu', 'slot1',
)


def icon_image(kind, style, colors):
    """Original 32px artwork; no enlarged launcher mask or badge overlay."""
    image = Image.new('RGBA', (32, 32))
    d = ImageDraw.Draw(image)
    accent, text, surface, line = (colors[k] for k in ('accent', 'text', 'surface', 'line'))
    outline = style == 'terminal'
    fill = surface if outline else accent
    ink = accent if outline else surface
    radius = 5 if style == 'lunar' else 1
    def box(bounds, color=fill, edge=accent):
        d.rounded_rectangle(bounds, radius=radius, fill=color, outline=edge, width=2 if outline else 1)
    def stroke(points, color=ink, width=2):
        d.line(points, fill=color, width=width)
    if kind == 'folder':
        if style == 'paper':
            d.polygon([(2,7),(12,7),(16,11),(29,11),(29,27),(2,27)], fill=line)
            d.polygon([(2,13),(29,13),(25,28),(5,28)], fill=accent)
            d.line([(3,14),(27,14)],fill=text)
            d.polygon([(21,13),(28,13),(25,20)],fill=surface)
        elif style == 'lunar':
            d.rounded_rectangle((2,10,29,28),radius=5,fill=accent)
            d.rounded_rectangle((4,5,15,16),radius=4,fill=accent)
            d.ellipse((19,14,26,21),fill=surface)
            d.ellipse((22,12,28,19),fill=accent)
            d.point((10,19),fill=text)
        else:
            d.polygon([(2,7),(13,7),(17,11),(29,11),(29,27),(2,27)],fill=fill,outline=accent,width=2 if outline else 1)
            stroke([(3,14),(28,14)],text if style == 'retro' else ink,1)
            if outline:
                stroke([(7,19),(7,23),(23,23)],accent,1)
            elif style == 'retro':
                stroke([(3,26),(28,26),(28,15)],line,2)
    elif kind in ('game','slot1'):
        d.polygon([(5,3),(25,3),(28,6),(28,29),(4,29),(4,4)],fill=fill,outline=accent,width=2 if outline else 1)
        d.rectangle((8,6,23,16),fill=ink)
        d.rectangle((11,8,21,13),outline=text)
        for x in (9,14,19,24): d.rectangle((x,24,x+1,28),fill=ink)
        stroke([(9,20),(13,20)],ink,1)
    elif kind == 'application':
        for x,y in ((3,4),(18,4),(3,19),(18,19)):
            box((x,y,x+10,y+10))
            if not outline: d.rectangle((x+2,y+2,x+4,y+4),fill=text)
    elif kind in ('audio','sound'):
        d.ellipse((3,3,28,28),fill=fill,outline=accent,width=2 if outline else 1)
        d.ellipse((7,7,24,24),outline=ink,width=2)
        d.ellipse((13,13,18,18),fill=ink)
        stroke([(20,5),(25,9)],text,1)
    elif kind in ('video','download-play'):
        box((2,6,29,25))
        d.rectangle((6,9,25,22),fill=ink)
        d.polygon([(13,11),(13,20),(21,15)],fill=accent)
        if kind == 'download-play': stroke([(10,28),(21,28)],accent,2)
    elif kind == 'book':
        d.polygon([(2,5),(13,3),(16,6),(19,3),(29,5),(29,27),(19,25),(16,28),(13,25),(2,27)],fill=fill,outline=accent,width=2 if outline else 1)
        stroke([(16,7),(16,25)])
        for y in (10,15,20):
            stroke([(5,y),(12,y-1)],ink,1);stroke([(20,y-1),(26,y)],ink,1)
    elif kind in ('system','settings','config'):
        if kind == 'config':
            for x,cy in ((6,10),(16,22),(26,14)):
                stroke([(x,3),(x,29)],accent,2)
                d.rectangle((x-3,cy-3,x+3,cy+3),fill=fill,outline=text)
        else:
            d.polygon([(11,2),(21,2),(22,7),(28,8),(30,18),(26,23),(22,24),(21,30),(11,30),(10,25),(4,24),(2,14),(6,9),(10,8)],fill=fill,outline=accent,width=2 if outline else 1)
            d.ellipse((9,9,23,23),fill=ink)
            d.ellipse((13,13,19,19),fill=accent)
    elif kind == 'camera':
        box((2,9,29,26));d.rectangle((8,5,18,10),fill=fill,outline=accent,width=2 if outline else 1)
        d.ellipse((10,11,24,25),fill=ink);d.ellipse((14,15,20,21),fill=text)
        d.rectangle((5,12,7,14),fill=ink)
    elif kind == 'pictochat':
        d.rounded_rectangle((2,4,29,23),radius=5,fill=fill,outline=accent,width=2 if outline else 1)
        d.polygon([(7,22),(7,29),(15,22)],fill=fill)
        for x in (8,15,22): d.rectangle((x,12,x+1,14),fill=ink)
    elif kind == 'system-menu':
        d.polygon([(2,15),(16,3),(30,15),(26,15),(26,28),(6,28),(6,15)],fill=fill,outline=accent,width=2 if outline else 1)
        d.rectangle((13,18,19,28),fill=ink)
    elif kind == 'image':
        box((2,4,29,28));d.rectangle((5,7,26,25),fill=ink)
        d.ellipse((18,9,22,13),fill=text)
        d.polygon([(5,24),(12,14),(18,21),(22,17),(26,24)],fill=accent)
    elif kind == 'save':
        d.polygon([(3,3),(25,3),(29,7),(29,29),(3,29)],fill=fill,outline=accent,width=2 if outline else 1)
        d.rectangle((8,3,23,12),fill=ink);d.rectangle((19,5,21,10),fill=text)
        d.rectangle((8,17,24,28),fill=ink)
        stroke([(11,21),(21,21)],accent,1);stroke([(11,25),(21,25)],accent,1)
    else:
        # Folded documents share a silhouette, with readable type-specific marks.
        d.polygon([(6,2),(21,2),(28,9),(28,29),(6,29)],fill=fill,outline=accent,width=2 if outline else 1)
        d.polygon([(21,2),(21,9),(28,9)],fill=line)
        if kind == 'archive':
            for y in range(4,27,4): d.rectangle((12,y,14,y+1),fill=ink)
            d.rectangle((11,23,15,27),outline=ink)
        elif kind == 'shortcut':
            stroke([(9,24),(9,19),(20,19)],ink,3)
            d.polygon([(17,14),(24,19),(17,24)],fill=ink)
        elif kind == 'missing':
            stroke([(12,14),(22,24)],ink,2);stroke([(22,14),(12,24)],ink,2)
        else:
            for y,w in ((14,12),(19,12),(24,8)):
                stroke([(10,y),(10+w,y)],ink,1 if outline else 2)
    return image


def assets():
    for manifest in sorted(THEMES.glob('*/theme.ini')):
        config = configparser.ConfigParser()
        config.read(manifest)
        name = manifest.parent.name
        style = ('terminal' if name in ('phosphor','slate') else
                 'lunar' if name in ('eclipse','nocturne') else
                 'paper' if name in ('prism','folio','tide','glacier') else 'retro')
        for kind in ICON_NAMES:
            output = io.BytesIO()
            icon_image(kind, style, config['colors']).save(output, format='PNG', optimize=True)
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
