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
    gradient(d, "#153b3d", "#49746c")
    for y in range(25, 192, 18):
        d.line((0, y, 255, y), fill="#61857b")
        for x in range((y // 18 % 2) * 32, 256, 64):
            d.line((x, y, x, y + 18), fill="#244c4a")
    d.rectangle((103, 30, 236, 115), fill="#183338", outline="#bd8150", width=2)
    for x, y, r, c in [
        (137, 72, 27, "#bd8150"),
        (184, 52, 19, "#e5b473"),
        (200, 93, 25, "#a85e45"),
    ]:
        gear(d, x, y, r, c, "#183338")
    d.line((119, 108, 227, 108), fill="#eecc99", width=2)
    for x in (107, 231):
        for y in (34, 111):
            d.rectangle((x - 1, y - 1, x + 1, y + 1), fill="#f4d7a4")
    for x in range(15, 250, 23):
        d.line((x, 154, x + 10, 164), fill="#9d6842", width=2)

    # Red pressure wheel and ivory gauge against the oxidized teal wall.
    d.line([(16, 118), (16, 41), (66, 41), (66, 117)], fill="#d4ad76", width=4)
    d.line([(18, 118), (18, 43), (66, 43)], fill="#f8dc9e")
    d.ellipse((33, 39, 57, 63), fill="#e9d9b3", outline="#243f42", width=2)
    d.line((45, 51, 50, 45), fill="#b7493c", width=2)
    d.ellipse((6, 88, 27, 109), outline="#e37d61", width=3)
    d.line((8, 91, 25, 106), fill="#e37d61", width=2)
    d.rectangle((113, 119, 241, 131), fill="#a76846")
    for x in range(117, 238, 12):
        d.line((x, 120, x + 3, 129), fill="#e1b575")


def draw_slate(d):
    gradient(d, "#152b43", "#73516b")
    stars(d, 18, "#547184", 25)
    rng = random.Random(10)
    for x in range(0, 256, 20):
        y = rng.randrange(48, 86)
        d.rectangle((x, y, x + 17, 142), fill="#15232f", outline="#426174")
        for yy in range(y + 7, 122, 11):
            for xx in (x + 4, x + 11):
                if rng.randrange(3):
                    d.rectangle((xx, yy, xx + 2, yy + 4), fill="#f9d396")
    d.rectangle((166, 42, 200, 105), fill="#324654", outline="#8dbce8")
    d.rectangle((172, 48, 194, 73), fill="#e68b78")
    d.line((174, 53, 191, 53), fill="#243a4e", width=2)
    d.line((174, 59, 189, 59), fill="#243a4e", width=2)
    d.rectangle((179, 83, 188, 105), fill="#0c1620")
    for x in range(7, 256, 17):
        d.line((x, 26, x - 9, 114), fill="#537e8d")
    for y in range(145, 184, 6):
        d.line((70, y, 230 - (y % 4) * 12, y), fill="#54788b")

    # Salmon neon, a teal tram shelter and yellow reflected shop lights.
    d.rectangle((208, 53, 236, 77), fill="#e47e88", outline="#ffc4aa")
    for y in (59, 65, 71):
        d.line((213, y, 230, y), fill="#612d4d", width=2)
    d.line([(103, 128), (103, 100), (147, 100), (153, 106)], fill="#80c7c6", width=2)
    for x in (38, 84, 179, 224):
        for y in range(148, 183, 6):
            d.line((x, y, x + 9, y), fill="#b28777")


def draw_folio(d):
    gradient(d, "#f7e9c7", "#e6d4ad")
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
            fill="#598568",
            outline="#455c3b",
        )
        d.line((x, y, x + flip * 23, y - 11), fill="#c3d0a0")
    d.ellipse((169, 24, 189, 43), fill="#c14840", outline="#7c4a3b")
    d.ellipse((175, 30, 182, 37), fill="#f2bd53")
    d.ellipse((202, 83, 224, 100), outline="#7b7457")
    d.line((213, 86, 213, 96), fill="#7b7457")
    for x in range(95, 238, 8):
        d.line((x, 113, x, 117 if x % 16 else 120), fill="#847858")

    # Field press: blue tape, vermilion berries and a pressed yellow bloom.
    d.polygon([(195, 34), (230, 30), (232, 39), (198, 44)], fill="#95b6ba")
    d.line((218, 81, 223, 55), fill="#597953", width=2)
    for x, y in ((214, 53), (224, 48), (229, 57), (221, 61)):
        d.ellipse((x - 4, y - 4, x + 4, y + 4), fill="#d2a039")
    d.ellipse((219, 52, 225, 58), fill="#8f5738")
    for x, y in ((146, 64), (141, 70), (149, 73)):
        d.ellipse((x, y, x + 4, y + 4), fill="#b64e40")


def draw_nocturne(d):
    gradient(d, "#12334b", "#638589")
    stars(d, 9, "#a8dbd3")
    d.ellipse((170, 28, 222, 80), fill="#ffe7b1")
    d.ellipse((183, 23, 231, 70), fill="#21445b")
    for x, y, w in [(75, 96, 42), (108, 80, 61), (172, 101, 76), (8, 121, 85)]:
        d.rectangle((x, y + 12, x + w, 177), fill="#60495a")
        d.polygon(
            [(x - 5, y + 12), (x + w // 2, y - 8), (x + w + 5, y + 12)],
            fill="#203341",
        )
        for xx in range(x + 9, x + w - 5, 16):
            d.rectangle((xx, y + 22, xx + 5, y + 31), fill="#ffb578")
    # A small silhouetted roof guardian, drawn as part of this original scene.
    d.ellipse((133, 54, 149, 80), fill="#101321")
    d.ellipse((132, 48, 148, 63), fill="#101321")
    d.polygon([(132, 54), (133, 42), (140, 50), (146, 43), (149, 55)], fill="#101321")
    d.point((137, 54), fill="#efe4a5")
    d.point((144, 54), fill="#efe4a5")
    d.arc((143, 64, 161, 82), 270, 90, fill="#101321", width=3)

    # Warm tiles and a string of colored lanterns break the blue skyline.
    for y in (116, 122, 128):
        for x in range(82, 250, 17):
            d.line((x, y, x + 10, y), fill="#896269")
    d.arc((88, 68, 239, 116), 0, 180, fill="#375666")
    for x, y, c in (
        (99, 102, "#f4cf82"),
        (129, 111, "#dc8b80"),
        (165, 114, "#86cabb"),
        (202, 108, "#d4b4db"),
        (228, 98, "#efb378"),
    ):
        d.rectangle((x - 2, y, x + 2, y + 5), fill=c)


def draw_tide(d):
    gradient(d, "#e7ba91", "#275d78")
    for y in range(80, 192, 7):
        for x in range((y % 3) * 10, 256, 38):
            d.arc((x, y, x + 30, y + 6), 0, 165, fill="#a9d8d9")
    d.polygon([(114, 112), (152, 90), (182, 95), (224, 115)], fill="#365e54")
    d.polygon(
        [(155, 98), (161, 46), (177, 46), (184, 98)],
        fill="#f5e9cb",
        outline="#647f78",
    )
    d.rectangle((160, 61, 179, 69), fill="#c34836")
    d.rectangle((156, 82, 181, 90), fill="#c34836")
    d.rectangle((159, 39, 180, 48), fill="#42646c")
    d.rectangle((163, 41, 175, 46), fill="#ffde94")
    d.polygon([(156, 39), (169, 29), (183, 39)], fill="#425866")
    d.polygon([(180, 42), (244, 28), (244, 56)], fill="#fff0bc")
    d.line([(98, 45), (103, 42), (108, 45), (113, 42), (118, 45)], fill="#4b7477")
    d.line([(199, 70), (203, 68), (208, 70)], fill="#4b7477")

    # Distant coral sail and sunlit rocks; cool sea, warm sky and red paint.
    d.ellipse((205, 58, 224, 77), fill="#f6dca4")
    d.line((104, 72, 104, 93), fill="#304d60")
    d.polygon([(102, 73), (102, 86), (88, 86)], fill="#bd6256")
    d.line((88, 92, 111, 92), fill="#eedbc1", width=2)
    for x, y in ((14, 134), (74, 158), (212, 143)):
        d.ellipse((x, y, x + 28, y + 11), fill="#436a70")
        d.arc((x, y, x + 28, y + 11), 180, 310, fill="#b4baa2", width=2)


def draw_phosphor(d):
    gradient(d, "#142935", "#37535a")
    for x in range(0, 256, 12):
        d.line((x, 24, x, 191), fill="#335560")
    for y in range(24, 192, 12):
        d.line((0, y, 255, y), fill="#335560")
    for radius in (39, 28, 16):
        d.ellipse(
            (173 - radius, 71 - radius, 173 + radius, 71 + radius),
            outline="#7b9d9d",
        )
    d.line((130, 71, 216, 71), fill="#7b9d9d")
    d.line((173, 28, 173, 114), fill="#7b9d9d")
    d.pieslice((134, 32, 212, 110), 285, 315, fill="#356d6b")
    d.line((173, 71, 201, 44), fill="#f3cf85", width=2)
    for x, y in [(153, 83), (187, 64), (162, 46)]:
        d.ellipse((x - 2, y - 2, x + 2, y + 2), fill="#eeb079")
    for x in range(87, 113, 6):
        d.rectangle((x, 111 - (x % 7) * 5, x + 3, 112), fill="#70c9c9")
    for y in range(145, 180, 4):
        d.line((15, y, 100 + (y % 11) * 7, y), fill="#58827f")

    # A navy instrument enclosure, not a whole-screen green phosphor wash.
    d.rectangle((122, 24, 223, 116), outline="#d2c9a3", width=2)
    for x, c in ((128, "#e8ba72"), (137, "#8bdbd1"), (146, "#d67e77")):
        d.rectangle((x, 120, x + 4, 124), fill=c)
    for y in range(48, 104, 7):
        d.line((232, y, 246, y), fill="#899b9d")
    d.rectangle((231, 100, 248, 114), fill="#c6b793")
    d.line((234, 108, 244, 104), fill="#b65f53", width=2)


def draw_eclipse(d):
    gradient(d, "#102e3c", "#416a72")
    stars(d, 44, "#ccb1e1", 70)
    d.ellipse((153, 27, 211, 85), fill="#df9c72")
    d.ellipse((165, 23, 214, 72), fill="#8b5e61")
    d.ellipse((132, 46, 235, 68), outline="#99ede0", width=2)
    d.ellipse((137, 48, 230, 65), outline="#cda5d4")
    d.rectangle((97, 91, 129, 113), fill="#35545e", outline="#927fa7")
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

    # Copper cloud bands and ice-blue rings, pink nebula points.
    for y in (39, 44, 72, 77):
        d.arc((153, y - 5, 211, y + 7), 10, 165, fill="#edc299")
    d.arc((132, 46, 235, 68), 0, 165, fill="#a3eddd", width=2)
    for x, y in ((92, 35), (117, 55), (232, 95)):
        d.line((x - 2, y, x + 2, y), fill="#df92ac")
        d.line((x, y - 2, x, y + 2), fill="#df92ac")
    d.line((222, 104, 242, 82), fill="#edb08d", width=3)
    d.ellipse((230, 77, 246, 85), fill="#789c9f")


def draw_prism(d):
    gradient(d, "#f6d6ac", "#97b8be")
    for x in range(0, 256, 24):
        d.line((x, 24, x, 117), fill="#af7593")
    d.rectangle((81, 29, 238, 114), outline="#fff8ff", width=2)
    for x, c in [(96, "#e49f53"), (143, "#db6d86"), (190, "#4496a1")]:
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
        d.rectangle((x + 3, 49, x + 26, 68), fill="#253b53")
        d.rectangle((x + 7, 52, x + 22, 64), outline="#a4e4e8")
        d.line((x + 6, 76, x + 27, 76), fill="#fae5ff", width=2)
        d.rectangle((x + 11, 88, x + 20, 91), fill="#3b3d5a")
        d.ellipse((x + 8, 56, x + 12, 60), fill="#f7d394")
    for y in range(120, 192, 12):
        d.line((0, y, 255, y), fill="#729596")
    for x in range(-150, 450, 35):
        d.line((128, 112, x, 191), fill="#729596")

    # A three-color marquee, independent cabinet decals and checker floor.
    for x, c in ((87, "#e8af58"), (137, "#df8098"), (187, "#69bcc4")):
        d.rectangle((x, 31, x + 45, 37), fill=c)
    for y in range(126, 192, 12):
        for x in range(0, 256, 24):
            if (x // 24 + y // 12) % 2:
                d.rectangle((x, y, x + 23, y + 11), fill="#7d9caa")
    for x, c in ((111, "#fee4a1"), (158, "#a8d9e1"), (205, "#f3adbe")):
        d.ellipse((x, 72, x + 3, 75), fill=c)


def draw_ember(d):
    gradient(d, "#173643", "#b28072")
    stars(d, 8, "#e0bc83", 25)
    for x in range(8, 256, 28):
        pine(d, x, 37 + (x % 17), 65, "#416c66")
    for x in (13, 83, 225, 252):
        pine(d, x, 42, 90, "#193f3d")
    d.polygon([(118, 104), (148, 65), (177, 104)], fill="#e9ca96", outline="#f0c986")
    d.polygon([(148, 67), (151, 104), (169, 104)], fill="#9b6660")
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

    # Pine-green dusk, rose horizon, cream tent and orange fire.
    d.rectangle((128, 89, 134, 100), fill="#48695d")
    d.line((120, 103, 115, 113), fill="#e8d2a8")
    d.line((176, 103, 181, 113), fill="#e8d2a8")
    for x, y in ((104, 118), (216, 119), (75, 134)):
        d.ellipse((x, y, x + 6, y + 3), fill="#81957a")
    d.rectangle((215, 101, 222, 108), fill="#b65a4b")
    d.arc((219, 101, 227, 107), 270, 90, fill="#d6b68c")


def draw_glacier(d):
    gradient(d, "#172942", "#a1c3c7")
    stars(d, 18, "#c2e8e8", 45)
    for band, color in [(0, "#755f9d"), (7, "#b681b9"), (13, "#8bddbf")]:
        points = [
            (x, 34 + band + round(9 * math.sin(x / 38))) for x in range(60, 256, 3)
        ]
        d.line(points, fill=color, width=3)
    d.polygon([(70, 111), (115, 64), (155, 108), (183, 82), (242, 115)], fill="#a2d1d4")
    d.polygon([(115, 64), (126, 94), (110, 88), (99, 98)], fill="#d5edf0")
    d.rectangle((158, 86, 214, 109), fill="#c46c45", outline="#c3e5e8")
    d.polygon([(153, 86), (170, 76), (204, 76), (220, 86)], fill="#ebf1dc")
    for x in (165, 180, 195):
        d.rectangle((x, 91, x + 8, 98), fill="#efc481")
    d.line((225, 67, 225, 110), fill="#d1e5de")
    d.polygon([(225, 67), (238, 70), (225, 75)], fill="#ecaa5f")
    for y in range(126, 192, 16):
        d.line((0, y, 255, y - 8), fill="#8eb7c3")

    # Safety-orange equipment and violet aurora over mint ice.
    d.rectangle((166, 103, 206, 108), fill="#e5c991")
    for x in (172, 186, 200):
        d.line((x, 103, x + 3, 108), fill="#573f47", width=2)
    d.rectangle((231, 113, 243, 120), fill="#dd8855")
    d.line([(231, 122), (231, 126), (245, 126)], fill="#53798c", width=2)
    for x, y in ((113, 132), (185, 151), (37, 157)):
        d.line((x, y, x + 20, y + 2), fill="#e5eee0")


def draw_garnet(d):
    gradient(d, "#17353c", "#625450")
    for x in range(72, 256, 38):
        d.rectangle((x, 25, x + 5, 191), fill="#46616a")
        d.arc((x - 18, 22, x + 23, 84), 180, 360, fill="#9b9c8e", width=2)
    cx, cy = 177, 69
    d.ellipse(
        (cx - 40, cy - 40, cx + 40, cy + 40),
        fill="#182d3d",
        outline="#d1a18c",
        width=3,
    )
    colors = ["#ce6985", "#4f8f9b", "#e6b86f", "#647cb8"]
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

    # Different pigments in each stained-glass lobe, with a brass tracery.
    for x, y, c in (
        (177, 44, "#d9b369"),
        (200, 68, "#68b4b0"),
        (177, 92, "#d77c98"),
        (154, 68, "#8095c6"),
    ):
        d.ellipse((x - 4, y - 5, x + 4, y + 5), fill=c)
    d.arc((139, 31, 215, 107), 190, 345, fill="#e8ca8d", width=2)
    d.rectangle((92, 114, 244, 119), fill="#997b60")


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
        # Continue the landscape below the focal props, without repeating them.
        image = image.crop((0, 112, 256, 192)).resize(
            (256, 192), Image.Resampling.NEAREST
        )
        config = configparser.ConfigParser()
        config.read(THEMES / name / "theme.ini")
        matte = Image.new("RGB", image.size, config["colors"]["background"])
        image = Image.blend(image, matte, 0.16)
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
