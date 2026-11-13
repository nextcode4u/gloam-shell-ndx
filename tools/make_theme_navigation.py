"""Original navigation symbols authored at dock resolution (CC0-1.0)."""

import math

from PIL import Image, ImageDraw

TAB_NAMES = (
    "tab-home",
    "tab-games",
    "tab-apps",
    "tab-media",
    "tab-system",
    "tab-files",
    "tab-folder",
)


def gear_image(colors, teeth=8):
    """A toothed ring with a transparent bore, readable at sixteen pixels."""
    image = Image.new("RGBA", (16, 16))
    draw = ImageDraw.Draw(image)
    if teeth == 8:
        draw.ellipse((3, 3, 12, 12), fill=colors["accent"])
        for bounds in (
            (6, 0, 9, 4),
            (6, 11, 9, 15),
            (0, 6, 4, 9),
            (11, 6, 15, 9),
            (2, 2, 4, 4),
            (11, 2, 13, 4),
            (2, 11, 4, 13),
            (11, 11, 13, 13),
        ):
            draw.rectangle(bounds, fill=colors["accent"])
    else:
        points = []
        pitch = math.tau / teeth
        for tooth in range(teeth):
            for fraction, radius in ((-0.5, 5), (-0.24, 7.5), (0.24, 7.5), (0.5, 5)):
                angle = (tooth + fraction) * pitch
                points.append(
                    (
                        round(7.5 + math.cos(angle) * radius),
                        round(7.5 + math.sin(angle) * radius),
                    )
                )
        draw.polygon(points, fill=colors["accent"])
    draw.ellipse((5, 5, 10, 10), fill=(0, 0, 0, 0))
    return image.resize((32, 32), Image.Resampling.NEAREST)


def navigation_image(kind, theme, colors):
    """Theme subjects alter silhouettes, rather than just swapping palettes."""
    if kind == "tab-system":
        return gear_image(colors, 6 if theme in ("prism", "folio", "tide") else 8)
    image = Image.new("RGBA", (16, 16))
    d = ImageDraw.Draw(image)
    accent, ink, light, edge = (
        colors[k] for k in ("accent", "surface", "text", "line")
    )
    terminal = theme in ("slate", "phosphor")
    fill = ink if terminal else accent

    def box(bounds):
        d.rectangle(bounds, fill=fill, outline=accent)

    def arcade():
        d.polygon(
            [(4, 1), (12, 1), (13, 8), (11, 11), (13, 15), (2, 15), (4, 10), (3, 8)],
            fill=accent,
        )
        d.rectangle((5, 3, 10, 6), fill=ink)
        d.line((5, 9, 10, 9), fill=ink)
        d.rectangle((6, 12, 9, 13), fill=ink)

    if kind == "tab-home":
        if theme == "copper":
            d.polygon(
                [(1, 7), (5, 3), (5, 6), (10, 2), (14, 6), (14, 14), (1, 14)],
                fill=accent,
            )
            d.rectangle((2, 1, 4, 6), fill=accent)
            for x in (3, 7, 11):
                d.rectangle((x, 9, x + 1, 11), fill=ink)
        elif theme == "prism":
            arcade()
        elif theme == "folio":
            d.polygon(
                [
                    (2, 3),
                    (6, 2),
                    (8, 4),
                    (10, 2),
                    (14, 3),
                    (14, 14),
                    (10, 13),
                    (8, 15),
                    (6, 13),
                    (2, 14),
                ],
                fill=accent,
            )
            d.line((8, 5, 8, 12), fill=ink)
        elif theme == "nocturne":
            d.polygon(
                [
                    (2, 6),
                    (2, 1),
                    (6, 4),
                    (10, 4),
                    (14, 1),
                    (14, 11),
                    (11, 14),
                    (5, 14),
                    (2, 11),
                ],
                fill=accent,
            )
            d.point((5, 8), fill=light)
            d.point((11, 8), fill=light)
        elif theme == "tide":
            d.polygon([(6, 5), (10, 5), (12, 14), (4, 14)], fill=accent)
            d.rectangle((5, 2, 11, 4), fill=accent)
            d.polygon([(4, 2), (8, 0), (12, 2)], fill=accent)
            d.line((5, 9, 11, 9), fill=ink, width=2)
        elif theme == "eclipse":
            d.ellipse((3, 3, 12, 12), fill=accent)
            d.arc((0, 5, 15, 10), 0, 360, fill=light)
            d.line((2, 14, 13, 14), fill=accent)
        elif theme == "ember":
            d.polygon([(1, 14), (8, 2), (15, 14)], fill=accent)
            d.polygon([(5, 14), (8, 7), (11, 14)], fill=ink)
        elif theme == "glacier":
            d.polygon([(1, 7), (8, 2), (15, 7)], fill=light)
            box((3, 7, 13, 14))
            d.rectangle((6, 9, 9, 14), fill=ink)
        elif theme == "garnet":
            d.rounded_rectangle((3, 1, 12, 15), radius=5, fill=accent)
            d.rounded_rectangle((6, 4, 9, 15), radius=2, fill=ink)
        elif theme == "phosphor":
            d.ellipse((1, 1, 14, 14), outline=accent)
            d.line((8, 3, 8, 13), fill=accent)
            d.line((3, 8, 13, 8), fill=accent)
            d.line((8, 8, 12, 3), fill=light)
        else:
            for x, height in ((1, 6), (6, 12), (11, 9)):
                box((x, 15 - height, x + 3, 14))
                d.point((x + 1, 17 - height), fill=light)
    elif kind == "tab-games":
        if theme == "prism":
            # A joystick differs from the arcade cabinet used for Home.
            d.rectangle((2, 11, 14, 14), fill=accent)
            d.line((6, 4, 6, 11), fill=accent, width=2)
            d.ellipse((3, 1, 8, 6), fill=accent)
            d.rectangle((11, 8, 13, 10), fill=light)
        elif theme in ("copper", "slate", "glacier"):
            box((3, 1, 12, 15))
            d.rectangle((5, 3, 10, 7), fill=ink if not terminal else accent)
            d.line((4, 11, 8, 11), fill=ink if not terminal else light)
            d.line((6, 9, 6, 13), fill=ink if not terminal else light)
            d.point((10, 11), fill=light)
        elif theme in ("folio", "tide"):
            box((2, 2, 13, 13))
            for x, y in ((5, 5), (10, 5), (8, 8), (5, 10), (10, 10)):
                d.rectangle((x, y, x + 1, y + 1), fill=ink)
        elif theme == "garnet":
            d.polygon(
                [
                    (3, 1),
                    (5, 1),
                    (5, 4),
                    (7, 4),
                    (7, 1),
                    (9, 1),
                    (9, 4),
                    (11, 4),
                    (11, 1),
                    (13, 1),
                    (13, 6),
                    (11, 8),
                    (11, 12),
                    (14, 15),
                    (2, 15),
                    (5, 12),
                    (5, 8),
                    (3, 6),
                ],
                fill=accent,
            )
        else:
            d.rounded_rectangle((1, 4, 14, 13), radius=3, fill=fill, outline=accent)
            d.line((3, 8, 7, 8), fill=ink if not terminal else light)
            d.line((5, 6, 5, 10), fill=ink if not terminal else light)
            d.point((10, 7), fill=light)
            d.point((12, 10), fill=light)
    elif kind == "tab-apps":
        if theme in ("copper", "ember"):
            d.line((3, 13, 11, 5), fill=accent, width=4)
            d.polygon(
                [(8, 5), (8, 2), (10, 0), (10, 4), (13, 4), (15, 2), (15, 5), (12, 8)],
                fill=accent,
            )
            d.point((3, 13), fill=ink)
        elif theme in ("folio", "garnet"):
            box((2, 2, 11, 14))
            d.line((5, 5, 9, 5), fill=ink)
            d.line((9, 12, 14, 3), fill=light, width=2)
        elif theme == "prism":
            for x, y in ((2, 8), (8, 8), (5, 2)):
                d.polygon(
                    [
                        (x, y + 2),
                        (x + 3, y),
                        (x + 6, y + 2),
                        (x + 6, y + 6),
                        (x + 3, y + 8),
                        (x, y + 6),
                    ],
                    fill=accent,
                    outline=light,
                )
        else:
            for x, y in ((1, 2), (9, 2), (1, 10), (9, 10)):
                d.rounded_rectangle(
                    (x, y, x + 5, y + 5),
                    radius=2 if theme == "nocturne" else 0,
                    fill=fill,
                    outline=accent,
                )
                d.point((x + 2, y + 2), fill=light)
    elif kind == "tab-media":
        if theme == "copper":
            d.ellipse((1, 1, 14, 14), fill=accent)
            d.ellipse((4, 4, 11, 11), outline=ink)
            d.ellipse((6, 6, 9, 9), fill=ink)
        elif theme == "prism":
            d.arc((2, 1, 13, 14), 180, 360, fill=accent, width=3)
            d.rectangle((1, 7, 4, 13), fill=accent)
            d.rectangle((11, 7, 14, 13), fill=accent)
        elif theme == "phosphor":
            d.rectangle((0, 2, 15, 13), outline=accent)
            d.line([(2, 8), (4, 8), (6, 4), (9, 11), (11, 7), (13, 7)], fill=light)
        elif theme == "folio":
            box((2, 1, 13, 14))
            d.line((5, 4, 10, 4), fill=ink)
            d.line((5, 7, 10, 7), fill=ink)
            d.line((5, 10, 9, 10), fill=ink)
        elif theme in ("slate", "ember", "glacier"):
            box((1, 5, 14, 14))
            d.line((3, 4, 10, 0), fill=accent)
            d.ellipse((3, 7, 8, 12), fill=ink if not terminal else accent)
            d.line((11, 7, 11, 12), fill=light)
        else:
            d.line((6, 12, 6, 3, 13, 1, 13, 10), fill=accent, width=2)
            d.ellipse((2, 10, 6, 14), fill=accent)
            d.ellipse((9, 8, 13, 12), fill=accent)
    elif kind == "tab-files" and theme == "copper":
        box((2, 1, 13, 14))
        for y in (3, 8):
            d.rectangle((4, y, 11, y + 4), fill=ink)
            d.line((6, y + 2, 9, y + 2), fill=light)
    elif kind == "tab-files" and theme == "prism":
        d.rectangle((1, 1, 10, 11), fill=accent)
        d.rectangle((4, 4, 13, 14), fill=light)
        d.rectangle((6, 6, 14, 15), fill=accent)
        d.line((8, 9, 12, 9), fill=ink)
    else:
        if theme in ("prism", "folio", "tide", "glacier"):
            d.polygon([(1, 3), (6, 3), (8, 5), (14, 5), (12, 14), (3, 14)], fill=edge)
            d.polygon([(0, 7), (15, 7), (12, 14), (3, 14)], fill=accent)
            d.polygon([(11, 7), (15, 7), (13, 10)], fill=light)
        elif theme in ("eclipse", "nocturne"):
            d.rounded_rectangle((1, 5, 14, 14), radius=3, fill=accent)
            d.rounded_rectangle((2, 2, 7, 8), radius=2, fill=accent)
            d.ellipse((10, 7, 13, 10), fill=ink)
        else:
            d.polygon(
                [(1, 3), (6, 3), (8, 6), (14, 6), (14, 14), (1, 14)],
                fill=fill,
                outline=accent,
            )
            d.line((2, 7, 13, 7), fill=light if not terminal else accent)
    return image.resize((32, 32), Image.Resampling.NEAREST)
