"""Shared supersampling primitives and navigation slot names (CC0-1.0)."""

from PIL import Image, ImageDraw


class SmoothDraw:
    """Draw the existing vector coordinates at 4x, then export at native size.

    Supersampling is an authoring step. The DSi only sees the finished sprite.
    """

    def __init__(self, image):
        self.draw = ImageDraw.Draw(image)

    def __getattr__(self, name):
        def scaled(coordinates, *args, **kwargs):
            def expand(value):
                if isinstance(value, (tuple, list)):
                    return tuple(expand(part) for part in value)
                return value * 4

            if name in ("line", "arc"):
                kwargs.setdefault("width", 1)
            for key in ("width", "radius"):
                if key in kwargs:
                    kwargs[key] *= 4
            return getattr(self.draw, name)(expand(coordinates), *args, **kwargs)

        return scaled


def smooth_export(image):
    """Keep antialiased edges without Lanczos ringing in empty/solid areas."""
    image = image.resize((32, 32), Image.Resampling.LANCZOS)
    image.putalpha(
        image.getchannel("A").point(lambda a: 0 if a < 16 else 255 if a > 239 else a)
    )
    return image


TAB_NAMES = (
    "tab-home",
    "tab-games",
    "tab-apps",
    "tab-media",
    "tab-system",
    "tab-files",
    "tab-folder",
)
