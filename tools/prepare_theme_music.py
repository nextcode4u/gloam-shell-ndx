#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify or rebuild licensed, bounded theme music from pinned original tracks."""

import argparse
from array import array
import hashlib
import json
import math
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import urllib.request
import wave

ROOT = Path(__file__).resolve().parents[1]
RATE = 16000


def digest(data):
    return hashlib.sha256(data).hexdigest()


def circular_loop(samples, frames, overlap):
    if len(samples) < frames + overlap or overlap < 2 or frames <= overlap:
        raise ValueError("Source excerpt is too short")
    output = array("f", samples[overlap:frames])
    for index in range(overlap):
        weight = (1 - math.cos(math.pi * index / (overlap - 1))) / 2
        output.append(samples[frames + index] * (1 - weight) + samples[index] * weight)
    # Move the hardware loop boundary to a quiet crossing; no recurring fade-out.
    pivot = min(
        range(1, min(4096, len(output))),
        key=lambda i: abs(output[i - 1]) + abs(output[i]),
    )
    return output[pivot:] + output[:pivot]


def convert(source, entry):
    frames = (
        round(entry["seconds"] * RATE / 2) * 2
    )  # PCM16 hardware words contain two samples.
    overlap = round(entry["crossfade_seconds"] * RATE)
    command = [
        "ffmpeg",
        "-nostdin",
        "-v",
        "error",
        "-ss",
        str(entry["start_seconds"]),
        "-i",
        str(source),
        "-t",
        str((frames + overlap) / RATE),
        "-vn",
        "-af",
        "highpass=f=65,lowpass=f=7200,loudnorm=I=-23:TP=-3:LRA=7",
        "-ac",
        "1",
        "-ar",
        str(RATE),
        "-f",
        "f32le",
        "pipe:1",
    ]
    samples = array("f")
    samples.frombytes(subprocess.check_output(command))
    if sys.byteorder != "little":
        samples.byteswap()
    loop = circular_loop(samples, frames, overlap)
    pcm = array("h", (max(-32767, min(32767, round(value * 32767))) for value in loop))
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm.tobytes()


def verify(path, entry):
    if not path.is_file() or digest(path.read_bytes()) != entry["wav_sha256"]:
        raise ValueError("Music differs from its reviewed manifest: " + str(path))
    with wave.open(str(path), "rb") as stream:
        if (
            stream.getnchannels(),
            stream.getsampwidth(),
            stream.getframerate(),
            stream.getcomptype(),
        ) != (1, 2, entry.get("rate", RATE), "NONE"):
            raise ValueError("Unsupported theme audio format")
        if stream.getnframes() * 2 > 1024 * 1024:
            raise ValueError("Theme music exceeds runtime RAM cap")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="Convert pinned original MP3s with FFmpeg; otherwise verify only",
    )
    parser.add_argument(
        "--download",
        action="store_true",
        help="Download missing pinned inputs when rebuilding",
    )
    parser.add_argument("--cache", type=Path, default=ROOT / "build/theme-music")
    args = parser.parse_args()
    if args.rebuild and not shutil.which("ffmpeg"):
        parser.error(
            "Rebuilding theme music requires FFmpeg on PATH; normal builds do not."
        )
    manifest = json.loads((ROOT / "config/theme-music.json").read_text())
    for theme, entry in manifest["themes"].items():
        output = ROOT / manifest["theme_root"] / theme / "sounds/loop.wav"
        notice = output.parent.parent / "MUSIC-LICENSE.txt"
        credit = output.parent.parent / "MUSIC.md"
        if (
            not notice.is_file()
            or digest(notice.read_bytes()) != manifest["license_text_sha256"]
        ):
            raise ValueError("Missing or changed music license: " + str(notice))
        if (
            not credit.is_file()
            or entry["title"] not in credit.read_text()
            or entry["source_url"] not in credit.read_text()
        ):
            raise ValueError("Missing music attribution: " + str(credit))
        if args.rebuild:
            source = args.cache / (entry["isrc"] + ".mp3")
            if not source.is_file() and args.download:
                source.parent.mkdir(parents=True, exist_ok=True)
                request = urllib.request.Request(
                    entry["download_url"],
                    headers={"User-Agent": "Gloam theme music converter"},
                )
                with urllib.request.urlopen(request, timeout=60) as response:
                    data = response.read(64 * 1024 * 1024 + 1)
                if len(data) > 64 * 1024 * 1024:
                    raise ValueError("Oversized music input")
                if digest(data) != entry["source_sha256"]:
                    raise ValueError("Downloaded source hash mismatch")
                source.write_bytes(data)
            if (
                not source.is_file()
                or digest(source.read_bytes()) != entry["source_sha256"]
            ):
                raise ValueError("Missing or changed original music: " + str(source))
            pcm = convert(source, entry)
            output.parent.mkdir(parents=True, exist_ok=True)
            temporary = output.with_suffix(".tmp")
            try:
                with wave.open(str(temporary), "wb") as stream:
                    stream.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
                    stream.writeframes(pcm)
                if digest(temporary.read_bytes()) != entry["wav_sha256"]:
                    raise ValueError(
                        "Conversion differs from the reviewed FFmpeg output; original retained"
                    )
                temporary.replace(output)
            finally:
                temporary.unlink(missing_ok=True)
        verify(output, entry)
        print(theme + ": " + entry["title"])
    for theme, entry in manifest.get("retained", {}).items():
        verify(ROOT / manifest["theme_root"] / theme / "sounds/loop.wav", entry)
        print(theme + ": retained " + entry["title"])


if __name__ == "__main__":
    main()
