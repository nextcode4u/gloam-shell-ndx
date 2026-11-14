#!/usr/bin/env python3
"""Build preconverted Gloam splashes from PNG, GIF or numbered PNG folders.

Run without arguments for the contributor interface. See STARTUP_SPLASHES.md.
"""

from __future__ import annotations

import argparse
import array
import math
from pathlib import Path
import re
import struct
import sys
import wave
import zlib

from PIL import Image, ImageColor, ImageOps, ImageSequence

MAGIC = b"GLOMSP01"
LIMIT = 2 * 1024 * 1024
EFFECTS = ("none", "fade", "slide-up", "slide-left", "reveal")
FPS = (5, 6, 10, 12, 15, 20)


def ticks(seconds: float) -> int:
    if not math.isfinite(seconds) or not 0 <= seconds <= 10:
        raise ValueError("Times must be between 0 and 10 seconds.")
    return round(seconds * 60)


def source_frames(path: Path, fps: int) -> list[Image.Image]:
    """Composite GIF disposal, sampling in order without retaining every source frame."""

    def prepared(image):
        if image.width > 4096 or image.height > 4096:
            raise ValueError("Resize source artwork to at most 4096×4096 first.")
        return ImageOps.contain(
            image.convert("RGBA"), (512, 384), Image.Resampling.LANCZOS
        )

    if path.is_dir():
        paths = sorted(
            path.glob("*.png"),
            key=lambda p: [
                int(s) if s.isdigit() else s.lower() for s in re.split(r"(\d+)", p.name)
            ],
        )
        if not paths or len(paths) > 120:
            raise ValueError("A frame folder needs 1–120 numbered PNG files.")
        frames = []
        for filename in paths:
            with Image.open(filename) as image:
                frames.append(prepared(image))
        return frames
    if path.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("Source images must be at most 64 MiB.")
    with Image.open(path) as image:
        if getattr(image, "n_frames", 1) == 1:
            return [prepared(image)]
        sampled, elapsed = [], 0
        for index, frame in enumerate(ImageSequence.Iterator(image)):
            elapsed += max(10, int(frame.info.get("duration", 100)))
            if index >= 600 or elapsed > 10000:
                raise ValueError(
                    "Keep source animations within 10 seconds and 600 source frames."
                )
            converted = None
            while len(sampled) * 1000 < elapsed * fps:
                if len(sampled) == 120:
                    raise ValueError(
                        "More than 120 prepared frames. Lower the frame rate or shorten the animation."
                    )
                if converted is None:
                    converted = prepared(frame)
                sampled.append(converted)
    return sampled


def fit_frame(
    image: Image.Image, size: tuple[int, int], fit: str, background
) -> Image.Image:
    if fit == "crop":
        return ImageOps.fit(image, size, Image.Resampling.LANCZOS)
    fitted = ImageOps.contain(image, size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, background)
    canvas.alpha_composite(
        fitted, ((size[0] - fitted.width) // 2, (size[1] - fitted.height) // 2)
    )
    return canvas


def rgb555(r: int, g: int, b: int) -> int:
    return 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def encode_frames(frames: list[Image.Image], direct: bool) -> bytes:
    if direct:
        words = array.array(
            "H",
            (
                rgb555(r, g, b) if a >= 128 else 0
                for frame in frames
                for r, g, b, a in zip(*[iter(frame.tobytes())] * 4)
            ),
        )
        if sys.byteorder != "little":
            words.byteswap()
        return words.tobytes()
    # One palette per layer, shared by every frame; no per-frame colour changes.
    atlas = Image.new(
        "RGB", (64 * min(16, len(frames)), 64 * math.ceil(len(frames) / 16))
    )
    for i, frame in enumerate(frames):
        atlas.paste(
            frame.convert("RGB").resize((64, 64)), ((i % 16) * 64, (i // 16) * 64)
        )
    palette_image = atlas.quantize(colors=255, dither=Image.Dither.NONE)
    palette = palette_image.getpalette()
    palette += [0] * (768 - len(palette))
    palette[255 * 3 : 256 * 3] = palette[254 * 3 : 255 * 3]
    palette_image.putpalette(palette)
    words = [0] + [rgb555(*palette[i * 3 : i * 3 + 3]) for i in range(255)]
    result = bytearray(struct.pack("<256H", *words))
    for frame in frames:
        indexed = frame.convert("RGB").quantize(
            palette=palette_image, dither=Image.Dither.NONE
        )
        result.extend(
            min(index, 254) + 1 if alpha >= 128 else 0
            for index, alpha in zip(indexed.tobytes(), frame.getchannel("A").tobytes())
        )
    return bytes(result)


def sound_pcm(path: Path | None, duration: int) -> bytes:
    if not path:
        return b""
    with wave.open(str(path), "rb") as wav:
        channels, width, rate, count = (
            wav.getnchannels(),
            wav.getsampwidth(),
            wav.getframerate(),
            wav.getnframes(),
        )
        if (
            channels not in (1, 2)
            or width not in (1, 2)
            or not 8000 <= rate <= 48000
            or count > rate * 10
        ):
            raise ValueError(
                "Use a mono/stereo, 8/16-bit PCM WAV at 8–48 kHz, no longer than 10 seconds."
            )
        raw = wav.readframes(count)
    if len(raw) != count * width * channels:
        raise ValueError("The WAV is truncated.")
    values = (
        [(b - 128) * 256 for b in raw]
        if width == 1
        else list(struct.unpack(f"<{count * channels}h", raw))
    )
    mono = [
        sum(values[i : i + channels]) // channels
        for i in range(0, len(values), channels)
    ]
    length = min(count * 16000 // rate, duration * 16000 // 60)
    samples = []
    for i in range(length):
        pos = i * rate / 16000
        left = min(int(pos), count - 1)
        right = min(left + 1, count - 1)
        samples.append(round(mono[left] + (mono[right] - mono[left]) * (pos - left)))
    if len(samples) & 1:
        samples.append(0)
    return struct.pack(f"<{len(samples)}h", *samples)


def build(
    *,
    top=None,
    bottom=None,
    top_logo=None,
    bottom_logo=None,
    fps=20,
    duration=None,
    poster=None,
    effect="fade",
    transition=0.5,
    fit="fit",
    background="#080e18",
    logo_width=128,
    direct=False,
    sound=None,
) -> bytes:
    if fps not in FPS or effect not in EFFECTS or fit not in ("fit", "crop"):
        raise ValueError("Unsupported frame rate, effect or fitting mode.")
    if not 1 <= logo_width <= 256:
        raise ValueError("Logo width must be 1–256 pixels.")
    transition_ticks = ticks(transition)
    if effect != "none" and not transition_ticks:
        raise ValueError("An animated effect needs a nonzero transition time.")
    color = (*ImageColor.getrgb(background), 255)
    layers = []
    for screen, source, logo in ((0, top, top_logo), (1, bottom, bottom_logo)):
        if source:
            frames = [
                fit_frame(f, (256, 192), fit, color)
                for f in source_frames(Path(source), fps)
            ]
            layers.append((screen, frames, 0, 0, "none" if logo else effect))
        if logo:
            frames = source_frames(Path(logo), fps)
            size = ImageOps.contain(
                frames[0], (logo_width, min(192, logo_width)), Image.Resampling.LANCZOS
            ).size
            if any(f.size != frames[0].size for f in frames):
                raise ValueError("All logo frames must have the same dimensions.")
            frames = [f.resize(size, Image.Resampling.LANCZOS) for f in frames]
            layers.append(
                (screen, frames, (256 - size[0]) // 2, (192 - size[1]) // 2, effect)
            )
    if not layers:
        raise ValueError("Choose artwork for at least one screen.")
    natural = max(
        len(frames) * 60 // fps if len(frames) > 1 else 180 for _, frames, *_ in layers
    )
    length = ticks(duration) if duration is not None else natural
    if not 1 <= length <= 600 or transition_ticks > length:
        raise ValueError(
            "Duration must be greater than zero, at most 10 seconds, and cover the transition."
        )
    poster_tick = ticks(poster) if poster is not None else length - 1
    if poster_tick >= length:
        raise ValueError("The still-frame time must be before the end of the splash.")
    data = bytearray(32 + 32 * len(layers))
    for i, (screen, frames, x, y, animation) in enumerate(layers):
        content = encode_frames(frames, direct)
        record = struct.pack(
            "<4B4H2h2H3I",
            screen,
            0 if direct else 1,
            EFFECTS.index(animation),
            0,
            frames[0].width,
            frames[0].height,
            len(frames),
            60 // fps,
            x,
            y,
            0,
            transition_ticks if animation != "none" else 0,
            len(data),
            len(content),
            0,
        )
        data[32 + i * 32 : 64 + i * 32] = record
        data.extend(content)
        data.extend(b"\0" * (-len(data) % 4))
        if len(data) > LIMIT:
            raise ValueError(
                "Splash exceeds 2 MiB. Lower the frame rate/duration, use indexed colour, or animate a smaller logo."
            )
    pcm = sound_pcm(Path(sound) if sound else None, length)
    sound_offset = len(data) if pcm else 0
    data.extend(pcm)
    if len(data) > LIMIT:
        raise ValueError(
            "Splash plus sound exceeds 2 MiB. Shorten the animation or sound."
        )
    data[:32] = struct.pack(
        "<8sIIHHB3xII",
        MAGIC,
        len(data),
        0,
        length,
        poster_tick,
        len(layers),
        sound_offset,
        len(pcm),
    )
    struct.pack_into("<I", data, 12, zlib.crc32(data[16:]))
    return bytes(data)


def write_package(path: Path, data: bytes, *, replace=False):
    if path.exists() and not replace:
        raise ValueError(
            f"{path} already exists. Choose another filename or explicitly replace it."
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("xb") as stream:
        stream.write(data)
    temporary.replace(path)


def main():
    if len(sys.argv) == 1:
        from splash_builder import main as gui

        gui()
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Output .gsp package")
    for name in ("top", "bottom", "top-logo", "bottom-logo"):
        parser.add_argument(
            "--" + name, type=Path, help="PNG/GIF or numbered PNG folder"
        )
    parser.add_argument("--fps", type=int, choices=FPS, default=20)
    parser.add_argument(
        "--duration",
        type=float,
        help="Seconds; defaults to animation length or 3 for static artwork",
    )
    parser.add_argument(
        "--poster",
        type=float,
        help="Reduced-motion still time in seconds; defaults to the end",
    )
    parser.add_argument("--effect", choices=EFFECTS, default="fade")
    parser.add_argument("--transition", type=float, default=0.5)
    parser.add_argument("--fit", choices=("fit", "crop"), default="fit")
    parser.add_argument("--background", default="#080e18")
    parser.add_argument("--logo-width", type=int, default=128)
    parser.add_argument(
        "--direct", action="store_true", help="RGB555 instead of shared indexed colour"
    )
    parser.add_argument(
        "--sound", type=Path, help="Optional WAV, converted to mono PCM16 at 16 kHz"
    )
    parser.add_argument("--replace", action="store_true")
    options = vars(parser.parse_args())
    output, replace = options.pop("output"), options.pop("replace")
    try:
        data = build(**options)
        write_package(output, data, replace=replace)
    except (ValueError, OSError, wave.Error) as exc:
        parser.exit(1, f"Splash not exported: {exc}\n")
    print(
        f"Saved {output}: {len(data):,} bytes, {len(data) / LIMIT:.1%} of the 2 MiB limit"
    )


if __name__ == "__main__":
    main()
