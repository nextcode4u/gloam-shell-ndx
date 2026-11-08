#!/usr/bin/env python3
"""Generate original PCM UI cues for the Copper example theme."""

import argparse
import io
import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RATE = 16000


def tones(notes: list[float], seconds: float, level: float) -> bytes:
    frames = bytearray()
    count = int(seconds * RATE)
    for frequency in notes:
        for index in range(count):
            position = index / count
            envelope = math.sin(math.pi * position) ** 2
            sample = math.sin(2 * math.pi * frequency * index / RATE)
            frames.extend(struct.pack("<h", int(sample * envelope * level * 32767)))
    output = io.BytesIO()
    with wave.open(output, "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(RATE)
        stream.writeframes(frames)
    return output.getvalue()


def test_music_loop() -> bytes:
    """Original four-second fixture, independent of the licensed theme track."""
    melody = [523.25, 659.25, 783.99, 659.25, 587.33, 698.46, 659.25, 587.33]
    harmony = [261.63] * 4 + [349.23] * 4
    frames = bytearray()
    note_frames = RATE // 2
    for lead, backing in zip(melody, harmony):
        for index in range(note_frames):
            time = index / RATE
            # Short fades avoid clicks at every note and at the loop boundary.
            envelope = min(1.0, time / 0.012, (0.5 - time) / 0.07)
            sample = (
                0.28 * math.sin(2 * math.pi * lead * time)
                + 0.10 * math.sin(2 * math.pi * backing * time)
                + 0.035 * math.sin(4 * math.pi * lead * time)
            )
            frames.extend(struct.pack("<h", round(sample * envelope * 32767)))
    output = io.BytesIO()
    with wave.open(output, "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(RATE)
        stream.writeframes(frames)
    return output.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    assets = {
        "cursor.wav": tones([880], 0.04, 0.12),
        "confirm.wav": tones([660, 990], 0.08, 0.14),
        "back.wav": tones([440], 0.07, 0.12),
    }
    directory = ROOT / "sdroot/_nds/dsi-frontend/themes/copper/sounds"
    for name, data in assets.items():
        path = directory / name
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                raise ValueError(f"Audio sample differs: {path}")
        else:
            directory.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    print(f"{'Checked' if args.check else 'Wrote'} {len(assets)} original PCM assets")


if __name__ == "__main__":
    main()
