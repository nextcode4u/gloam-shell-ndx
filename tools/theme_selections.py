"""Original selection artwork with an individual material and silhouette per theme."""

from PIL import Image

if __package__:
    from .make_theme_navigation import SmoothDraw
else:
    from make_theme_navigation import SmoothDraw

FILLS = {
    "copper": "#3a5e55",
    "slate": "#4a5268",
    "folio": "#fff0cb",
    "nocturne": "#4b4a72",
    "tide": "#fff3d4",
    "phosphor": "#415544",
    "eclipse": "#654d6c",
    "prism": "#c5e864",
    "ember": "#fae7be",
    "glacier": "#fbfff4",
    "garnet": "#663e53",
}


def selection_surface(theme):
    """Keep ornament in the outer four pixels, away from live labels and icons."""
    image = Image.new("RGBA", (128, 128))
    d = SmoothDraw(image)
    fill = FILLS[theme]
    if theme == "copper":
        # A clipped brass plate with inset celadon enamel and four fastening pins.
        d.polygon(
            [(4, 0), (27, 0), (31, 4), (31, 27), (27, 31), (4, 31), (0, 27), (0, 4)],
            fill="#d5ad6c",
        )
        d.polygon(
            [(5, 2), (26, 2), (29, 5), (29, 26), (26, 29), (5, 29), (2, 26), (2, 5)],
            fill=fill,
        )
        for x, y in ((3, 3), (28, 3), (3, 28), (28, 28)):
            d.ellipse((x - 1, y - 1, x + 1, y + 1), fill="#fff0bd")
    elif theme == "slate":
        # A raised frosted slab with a coral folded corner, not a leading marker.
        d.rounded_rectangle((0, 1, 31, 31), radius=7, fill="#191d2c")
        d.rounded_rectangle((0, 0, 31, 29), radius=7, fill="#b8cbd5")
        d.rounded_rectangle((1, 1, 30, 28), radius=6, fill=fill)
        d.polygon([(25, 0), (29, 1), (31, 5), (25, 5)], fill="#f4ad98")
    elif theme == "folio":
        # A loose, torn sheet secured by warm translucent-looking paper tape.
        d.polygon(
            [
                (1, 1),
                (7, 2),
                (12, 0),
                (20, 1),
                (27, 0),
                (31, 3),
                (30, 12),
                (31, 21),
                (29, 31),
                (22, 30),
                (14, 31),
                (7, 30),
                (1, 31),
                (0, 23),
                (1, 15),
                (0, 7),
            ],
            fill="#b48c57",
        )
        d.polygon(
            [
                (2, 3),
                (10, 2),
                (20, 3),
                (28, 2),
                (29, 11),
                (28, 20),
                (29, 28),
                (20, 29),
                (11, 28),
                (3, 29),
                (2, 20),
                (3, 12),
            ],
            fill=fill,
        )
        for x in (1, 25):
            d.polygon([(x, 0), (x + 5, 1), (x + 4, 4), (x, 3)], fill="#dcc394")
    elif theme == "nocturne":
        # A padded cloth applique with embroidered corner stitches.
        d.rounded_rectangle((0, 0, 31, 31), radius=6, fill="#a7b7cd")
        d.rounded_rectangle((1, 1, 30, 30), radius=5, fill=fill)
        for x, y in ((3, 3), (28, 3), (3, 28), (28, 28)):
            d.line((x - 1, y - 1, x + 1, y + 1), fill="#f2d495", width=1)
            d.line((x + 1, y - 1, x - 1, y + 1), fill="#f2d495", width=1)
    elif theme == "tide":
        # A sailcloth-backed cream boarding ticket with notched ends.
        d.rounded_rectangle((0, 0, 31, 31), radius=4, fill="#3d7385")
        d.rounded_rectangle((1, 2, 30, 29), radius=3, fill=fill)
        for x in (-2, 29):
            d.ellipse((x, 13, x + 4, 18), fill=(0, 0, 0, 0))
        d.polygon([(26, 0), (31, 0), (31, 5)], fill="#c3664e")
    elif theme == "phosphor":
        # A field equipment tag: chamfered opposing corners and orange corner caps.
        d.polygon(
            [(0, 0), (25, 0), (31, 6), (31, 31), (6, 31), (0, 25)], fill="#b7c799"
        )
        d.polygon([(1, 1), (24, 1), (30, 7), (30, 30), (7, 30), (1, 24)], fill=fill)
        d.polygon([(25, 0), (31, 6), (28, 6), (25, 3)], fill="#edaa68")
        d.polygon([(0, 25), (6, 31), (6, 28), (3, 25)], fill="#edaa68")
    elif theme == "eclipse":
        # A pearlescent offset capsule with rose and aqua ends.
        d.rounded_rectangle((0, 0, 31, 31), radius=7, fill="#ddaed0")
        d.rectangle((0, 0, 12, 31), fill="#a0d1d1")
        d.rounded_rectangle((1, 2, 30, 29), radius=6, fill=fill)
        d.rectangle((1, 2, 12, 29), fill=fill)
        d.ellipse((27, 3, 29, 5), fill="#fff2d9")
    elif theme == "prism":
        # A thick puffy lime sticker with a pink peel, carrying a whole-row fill.
        d.rounded_rectangle((0, 1, 31, 31), radius=7, fill="#ed719c")
        d.rounded_rectangle((0, 0, 31, 29), radius=7, fill="#fff8ef")
        d.rounded_rectangle((2, 2, 29, 27), radius=5, fill=fill)
        d.polygon([(26, 25), (31, 23), (29, 29), (25, 30)], fill="#f7a0b8")
    elif theme == "ember":
        # A cream leather luggage label, with russet seams and corner lacing.
        d.rounded_rectangle((0, 0, 31, 31), radius=3, fill="#9a5b38")
        d.rounded_rectangle((2, 2, 29, 29), radius=2, fill=fill)
        for x, y in ((3, 3), (28, 3), (3, 28), (28, 28)):
            d.line((x - 1, y, x + 1, y), fill="#9a5b38", width=1)
            d.line((x, y - 1, x, y + 1), fill="#9a5b38", width=1)
    elif theme == "glacier":
        # Folded white expedition paper: navy top fold and an orange lower seal.
        d.polygon(
            [(0, 0), (25, 0), (31, 6), (31, 31), (6, 31), (0, 25)], fill="#6aa9b8"
        )
        d.polygon([(1, 1), (24, 1), (30, 7), (30, 30), (7, 30), (1, 24)], fill=fill)
        d.polygon([(25, 0), (25, 6), (31, 6)], fill="#31556b")
        d.polygon([(0, 25), (6, 31), (0, 31)], fill="#e6a065")
    elif theme == "garnet":
        # A satin inset held by a gilded art-nouveau setting and jade stones.
        d.rounded_rectangle((0, 0, 31, 31), radius=5, fill="#d6b17b")
        d.rounded_rectangle((2, 2, 29, 29), radius=4, fill=fill)
        for x, y in ((3, 3), (28, 3), (3, 28), (28, 28)):
            d.polygon([(x, y - 2), (x + 2, y), (x, y + 2), (x - 2, y)], fill="#89b6ac")
    else:
        raise ValueError(f"No selection artwork for {theme}")
    return image
