#!/usr/bin/env python3
"""Original native-resolution scene artwork for Gloam themes (CC0).

Illustrations are authored in pixels, with the subject on the right of the top
screen and room for a live cover on the left. No downloaded artwork is used.
"""

import argparse
import configparser
import io
import math
import random
from pathlib import Path
from PIL import Image, ImageColor, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "sdroot/_nds/dsi-frontend/themes"
SUBJECTS = {
    "copper": ("COPPERWORKS", "Clockwork workshop"),
    "slate": ("NIGHTLINE", "Rain-soaked city archive"),
    "folio": ("FIELD NOTES", "Botanical field notebook"),
    "nocturne": ("NOCTURNE", "Moonlit rooftops and a watchful cat"),
    "tide": ("TIDELIGHT", "Lighthouse above a quiet coast"),
    "phosphor": ("PHOSPHOR RADAR", "Green radar station"),
    "eclipse": ("ECLIPSE", "Observatory beneath a ringed planet"),
    "prism": ("PRISM ARCADE", "Pastel arcade at closing time"),
    "ember": ("EMBER CAMP", "Forest campsite and glowing embers"),
    "glacier": ("POLAR STATION", "Research outpost beneath an aurora"),
    "garnet": ("GARNET HALL", "Rose window in a gothic hall"),
}


def gradient(draw, upper, lower):
    a, b = ImageColor.getrgb(upper), ImageColor.getrgb(lower)
    for y in range(192):
        color = tuple(round(x + (z - x) * y / 191) for x, z in zip(a, b))
        draw.line((0, y, 255, y), fill=color)


def stars(draw, seed, color, count=45):
    rng = random.Random(seed)
    for _ in range(count):
        x, y = rng.randrange(256), rng.randrange(25, 110)
        draw.point((x, y), fill=color)


def pine(draw, x, y, size, color):
    draw.rectangle((x - 1, y, x + 1, y + size), fill=color)
    for step in range(3):
        top = y + step * size // 5
        width = size * (step + 2) // 9
        draw.polygon(
            [(x, top), (x - width, top + size // 2), (x + width, top + size // 2)],
            fill=color,
        )


def gear(draw, x, y, r, color, inside):
    for tooth in range(12):
        angle = tooth * math.pi / 6
        xx, yy = x + round(math.cos(angle) * r), y + round(math.sin(angle) * r)
        draw.rectangle((xx - 3, yy - 3, xx + 3, yy + 3), fill=color)
    draw.ellipse((x - r + 2, y - r + 2, x + r - 2, y + r - 2), fill=color)
    draw.ellipse((x - r + 7, y - r + 7, x + r - 7, y + r - 7), fill=inside)
    draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=color)
    for angle in range(0, 360, 90):
        a = math.radians(angle)
        draw.line(
            (x, y, x + round(math.cos(a) * (r - 4)), y + round(math.sin(a) * (r - 4))),
            fill=color,
            width=2,
        )


def draw_copper(d):
    gradient(d, "#231917", "#513426")
    for y in range(25, 192, 18):
        d.line((0, y, 255, y), fill="#64422c")
        for x in range((y // 18 % 2) * 32, 256, 64):
            d.line((x, y, x, y + 18), fill="#3a281f")
    d.rectangle((103, 30, 236, 115), fill="#211c1b", outline="#bd8150", width=2)
    for x, y, r, c in [
        (137, 72, 27, "#bd8150"),
        (184, 52, 19, "#e5b473"),
        (200, 93, 25, "#936d42"),
    ]:
        gear(d, x, y, r, c, "#211c1b")
    d.line((119, 108, 227, 108), fill="#eecc99", width=2)
    for x in (107, 231):
        for y in (34, 111):
            d.rectangle((x - 1, y - 1, x + 1, y + 1), fill="#f4d7a4")
    for x in range(15, 250, 23):
        d.line((x, 154, x + 10, 164), fill="#9d6842", width=2)


def draw_slate(d):
    gradient(d, "#111e2b", "#30465a")
    stars(d, 18, "#547184", 25)
    rng = random.Random(10)
    for x in range(0, 256, 20):
        y = rng.randrange(48, 86)
        d.rectangle((x, y, x + 17, 142), fill="#15232f", outline="#426174")
        for yy in range(y + 7, 122, 11):
            for xx in (x + 4, x + 11):
                if rng.randrange(3):
                    d.rectangle((xx, yy, xx + 2, yy + 4), fill="#c6a477")
    d.rectangle((166, 42, 200, 105), fill="#283b4b", outline="#8dbce8")
    d.rectangle((172, 48, 194, 73), fill="#86b6c9")
    d.line((174, 53, 191, 53), fill="#243a4e", width=2)
    d.line((174, 59, 189, 59), fill="#243a4e", width=2)
    d.rectangle((179, 83, 188, 105), fill="#0c1620")
    for x in range(7, 256, 17):
        d.line((x, 26, x - 9, 114), fill="#496379")
    for y in range(145, 184, 6):
        d.line((70, y, 230 - (y % 4) * 12, y), fill="#54788b")


def draw_folio(d):
    gradient(d, "#efe8cc", "#cbbc95")
    for y in range(32, 192, 10):
        d.line((0, y, 255, y), fill="#c8c2a7")
    d.line((82, 0, 82, 191), fill="#b27664")
    for y in range(12, 192, 22):
        d.ellipse((4, y, 11, y + 7), fill="#a29776")
        d.line((0, y + 3, 8, y + 3), fill="#675d47", width=2)
    d.line([(165, 111), (163, 84), (179, 57), (178, 31)], fill="#455c3b", width=2)
    for x, y, flip in [(165, 96, -1), (165, 82, 1), (172, 68, -1), (178, 53, 1)]:
        d.polygon(
            [
                (x, y),
                (x + flip * 24, y - 17),
                (x + flip * 26, y - 5),
                (x + flip * 12, y + 2),
            ],
            fill="#73906a",
            outline="#455c3b",
        )
        d.line((x, y, x + flip * 23, y - 11), fill="#c3d0a0")
    d.ellipse((169, 24, 189, 43), fill="#b66a57", outline="#7c4a3b")
    d.ellipse((175, 30, 182, 37), fill="#e2be77")
    d.ellipse((202, 83, 224, 100), outline="#7b7457")
    d.line((213, 86, 213, 96), fill="#7b7457")
    for x in range(95, 238, 8):
        d.line((x, 113, x, 117 if x % 16 else 120), fill="#847858")


def draw_nocturne(d):
    gradient(d, "#15162c", "#4b4569")
    stars(d, 9, "#d8cfee")
    d.ellipse((170, 28, 222, 80), fill="#c8c0e4")
    d.ellipse((183, 23, 231, 70), fill="#25243e")
    for x, y, w in [(75, 96, 42), (108, 80, 61), (172, 101, 76), (8, 121, 85)]:
        d.rectangle((x, y + 12, x + w, 177), fill="#242238")
        d.polygon(
            [(x - 5, y + 12), (x + w // 2, y - 8), (x + w + 5, y + 12)],
            fill="#121725",
        )
        for xx in range(x + 9, x + w - 5, 16):
            d.rectangle((xx, y + 22, xx + 5, y + 31), fill="#d5b17b")
    # A small silhouetted roof guardian, drawn as part of this original scene.
    d.ellipse((133, 54, 149, 80), fill="#101321")
    d.ellipse((132, 48, 148, 63), fill="#101321")
    d.polygon([(132, 54), (133, 42), (140, 50), (146, 43), (149, 55)], fill="#101321")
    d.point((137, 54), fill="#efe4a5")
    d.point((144, 54), fill="#efe4a5")
    d.arc((143, 64, 161, 82), 270, 90, fill="#101321", width=3)


def draw_tide(d):
    gradient(d, "#a4d4cf", "#376f79")
    for y in range(80, 192, 7):
        for x in range((y % 3) * 10, 256, 38):
            d.arc((x, y, x + 30, y + 6), 0, 165, fill="#b9ded0")
    d.polygon([(114, 112), (152, 90), (182, 95), (224, 115)], fill="#4c6960")
    d.polygon(
        [(155, 98), (161, 46), (177, 46), (184, 98)],
        fill="#f5e9cb",
        outline="#647f78",
    )
    d.rectangle((160, 61, 179, 69), fill="#c16c58")
    d.rectangle((156, 82, 181, 90), fill="#c16c58")
    d.rectangle((159, 39, 180, 48), fill="#42646c")
    d.rectangle((163, 41, 175, 46), fill="#ffde94")
    d.polygon([(156, 39), (169, 29), (183, 39)], fill="#425866")
    d.polygon([(180, 42), (244, 28), (244, 56)], fill="#d8e9c3")
    d.line([(98, 45), (103, 42), (108, 45), (113, 42), (118, 45)], fill="#4b7477")
    d.line([(199, 70), (203, 68), (208, 70)], fill="#4b7477")


def draw_phosphor(d):
    gradient(d, "#06170f", "#153726")
    for x in range(0, 256, 12):
        d.line((x, 24, x, 191), fill="#1b412c")
    for y in range(24, 192, 12):
        d.line((0, y, 255, y), fill="#1b412c")
    for radius in (39, 28, 16):
        d.ellipse(
            (173 - radius, 71 - radius, 173 + radius, 71 + radius),
            outline="#4b9563",
        )
    d.line((130, 71, 216, 71), fill="#4b9563")
    d.line((173, 28, 173, 114), fill="#4b9563")
    d.pieslice((134, 32, 212, 110), 285, 315, fill="#367c50")
    d.line((173, 71, 201, 44), fill="#c8ffd1", width=2)
    for x, y in [(153, 83), (187, 64), (162, 46)]:
        d.ellipse((x - 2, y - 2, x + 2, y + 2), fill="#b4ffb9")
    for x in range(87, 113, 6):
        d.rectangle((x, 111 - (x % 7) * 5, x + 3, 112), fill="#57b975")
    for y in range(145, 180, 4):
        d.line((15, y, 100 + (y % 11) * 7, y), fill="#285e3c")


def draw_eclipse(d):
    gradient(d, "#120c29", "#433256")
    stars(d, 44, "#ccb1e1", 70)
    d.ellipse((153, 27, 211, 85), fill="#d6a5cc")
    d.ellipse((165, 23, 214, 72), fill="#45264f")
    d.ellipse((132, 46, 235, 68), outline="#f2cce8", width=2)
    d.ellipse((137, 48, 230, 65), outline="#986eac")
    d.rectangle((97, 91, 129, 113), fill="#332f48", outline="#927fa7")
    d.pieslice((92, 71, 134, 110), 180, 360, fill="#705d87", outline="#b4a2cc")
    d.line((114, 91, 125, 73), fill="#d6c4e6", width=3)
    d.polygon(
        [
            (75, 120),
            (118, 105),
            (145, 115),
            (187, 99),
            (245, 119),
            (256, 192),
            (0, 192),
        ],
        fill="#211c36",
    )


def draw_prism(d):
    gradient(d, "#ebe0f4", "#9aa5d2")
    for x in range(0, 256, 24):
        d.line((x, 24, x, 117), fill="#c2b3d9")
    d.rectangle((81, 29, 238, 114), outline="#fff8ff", width=2)
    for x, c in [(96, "#8b75bb"), (143, "#c285ae"), (190, "#77a8bc")]:
        d.polygon(
            [
                (x, 42),
                (x + 30, 42),
                (x + 34, 62),
                (x + 28, 82),
                (x + 35, 110),
                (x - 3, 110),
                (x + 4, 83),
                (x - 1, 67),
            ],
            fill=c,
            outline="#faf3ff",
        )
        d.rectangle((x + 3, 49, x + 26, 68), fill="#303856")
        d.rectangle((x + 7, 52, x + 22, 64), outline="#a4e4e8")
        d.line((x + 6, 76, x + 27, 76), fill="#fae5ff", width=2)
        d.rectangle((x + 11, 88, x + 20, 91), fill="#3b3d5a")
        d.ellipse((x + 8, 56, x + 12, 60), fill="#f7d394")
    for y in range(120, 192, 12):
        d.line((0, y, 255, y), fill="#757cac")
    for x in range(-150, 450, 35):
        d.line((128, 112, x, 191), fill="#757cac")


def draw_ember(d):
    gradient(d, "#261c28", "#8c5940")
    stars(d, 8, "#e0bc83", 25)
    for x in range(8, 256, 28):
        pine(d, x, 37 + (x % 17), 65, "#32332c")
    for x in (13, 83, 225, 252):
        pine(d, x, 42, 90, "#182827")
    d.polygon([(118, 104), (148, 65), (177, 104)], fill="#d9a160", outline="#f0c986")
    d.polygon([(148, 67), (151, 104), (169, 104)], fill="#784e34")
    d.ellipse((168, 96, 210, 113), fill="#69432e")
    d.line((175, 108, 201, 103), fill="#b48857", width=3)
    d.line((176, 103, 198, 109), fill="#b48857", width=3)
    d.polygon(
        [
            (178, 103),
            (177, 90),
            (186, 95),
            (189, 75),
            (197, 91),
            (200, 100),
            (194, 106),
        ],
        fill="#f0a04e",
    )
    d.polygon([(185, 102), (188, 88), (194, 100), (191, 105)], fill="#ffe7a5")
    for x, y in [(182, 69), (193, 60), (201, 82)]:
        d.rectangle((x, y, x + 1, y + 2), fill="#f0bb73")


def draw_glacier(d):
    gradient(d, "#0e263a", "#549baf")
    stars(d, 18, "#c2e8e8", 45)
    for band, color in [(0, "#386e82"), (7, "#4e9d9e"), (13, "#80c6bc")]:
        points = [
            (x, 34 + band + round(9 * math.sin(x / 38))) for x in range(60, 256, 3)
        ]
        d.line(points, fill=color, width=3)
    d.polygon([(70, 111), (115, 64), (155, 108), (183, 82), (242, 115)], fill="#a2d1d4")
    d.polygon([(115, 64), (126, 94), (110, 88), (99, 98)], fill="#d5edf0")
    d.rectangle((158, 86, 214, 109), fill="#44687f", outline="#c3e5e8")
    d.polygon([(153, 86), (170, 76), (204, 76), (220, 86)], fill="#ebf1dc")
    for x in (165, 180, 195):
        d.rectangle((x, 91, x + 8, 98), fill="#efc481")
    d.line((225, 67, 225, 110), fill="#d1e5de")
    d.polygon([(225, 67), (238, 70), (225, 75)], fill="#e8b894")
    for y in range(126, 192, 16):
        d.line((0, y, 255, y - 8), fill="#8eb7c3")


def draw_garnet(d):
    gradient(d, "#200f24", "#57253e")
    for x in range(72, 256, 38):
        d.rectangle((x, 25, x + 5, 191), fill="#6b4253")
        d.arc((x - 18, 22, x + 23, 84), 180, 360, fill="#9e6a7a", width=2)
    cx, cy = 177, 69
    d.ellipse(
        (cx - 40, cy - 40, cx + 40, cy + 40),
        fill="#1d172a",
        outline="#d1a18c",
        width=3,
    )
    colors = ["#c8758c", "#86578a", "#d6a17c", "#764e81"]
    for petal in range(8):
        angle = petal * math.pi / 4
        x, y = cx + round(math.cos(angle) * 23), cy + round(math.sin(angle) * 23)
        d.ellipse(
            (x - 9, y - 12, x + 9, y + 12),
            fill=colors[petal % 4],
            outline="#2f2035",
            width=2,
        )
        d.line(
            (
                cx,
                cy,
                cx + round(math.cos(angle) * 38),
                cy + round(math.sin(angle) * 38),
            ),
            fill="#342035",
            width=2,
        )
    d.ellipse(
        (cx - 10, cy - 10, cx + 10, cy + 10),
        fill="#e4b692",
        outline="#503145",
        width=2,
    )
    for x in (98, 239):
        d.rectangle((x - 2, 96, x + 2, 113), fill="#bda18c")
        d.ellipse((x - 2, 90, x + 2, 96), fill="#f3cb94")


DRAWINGS = {
    "copper": draw_copper,
    "slate": draw_slate,
    "folio": draw_folio,
    "nocturne": draw_nocturne,
    "tide": draw_tide,
    "phosphor": draw_phosphor,
    "eclipse": draw_eclipse,
    "prism": draw_prism,
    "ember": draw_ember,
    "glacier": draw_glacier,
    "garnet": draw_garnet,
}


def scene(name, screen):
    image = Image.new("RGB", (256, 192))
    DRAWINGS[name](ImageDraw.Draw(image))
    if screen:
        # Continue the same world beneath opaque row surfaces, with a calmer tone.
        config = configparser.ConfigParser()
        config.read(THEMES / name / "theme.ini")
        matte = Image.new("RGB", image.size, config["colors"]["background"])
        image = Image.blend(image, matte, 0.35)
    return image


def assets():
    for name in SUBJECTS:
        for screen in range(2):
            output = io.BytesIO()
            scene(name, screen).save(output, format="PNG", optimize=True)
            yield THEMES / name / "backgrounds" / (
                "scene-top.png" if screen == 0 else "scene-bottom.png"
            ), output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for path, data in assets():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise SystemExit(f"Scene differs: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    print(
        "Checked 22 original scene backgrounds"
        if args.check
        else "Wrote 22 original scene backgrounds"
    )


if __name__ == "__main__":
    main()
