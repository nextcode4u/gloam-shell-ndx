"""Validate the offline packer's format and refusal to crop oversized glyphs."""

import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT = Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf")


@unittest.skipUnless(
    importlib.util.find_spec("PIL") and FONT.exists(), "Pillow and DejaVu required"
)
class FontPackTests(unittest.TestCase):
    def test_native_envelope_and_rejected_overflow(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "ui.dsfont"
            command = [
                sys.executable,
                str(ROOT / "tools/pack_theme_font.py"),
                str(FONT),
                str(output),
            ]
            subprocess.run(command, check=True, capture_output=True)
            data = output.read_bytes()
            self.assertEqual(len(data), 2296)
            self.assertEqual(data[:16], b"DSFONT01" + bytes([7, 12, 32, 95, 0, 0, 0, 0]))
            self.assertTrue(all(row <= 0x3FFF for row in struct.unpack("<1140H", data[16:])))
            before = data
            result = subprocess.run(command + ["--size", "30"], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_bytes(), before)

    def test_latin1_and_reproducible_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "latin.dsfont"
            header = Path(directory) / "fallback.h"
            command = [
                sys.executable,
                str(ROOT / "tools/pack_theme_font.py"),
                str(FONT),
                str(output),
                "--size",
                "10",
                "--baseline",
                "10",
                "--latin1",
                "--fallback-header",
                str(header),
            ]
            subprocess.run(command, check=True, capture_output=True)
            data = output.read_bytes()
            self.assertEqual(len(data), 4600)
            self.assertEqual(data[:16], b"DSFONT02" + bytes([7, 12, 32, 95, 160, 96, 0, 0]))
            self.assertIn("ui_font_latin[96][12]", header.read_text())
            self.assertTrue(all(row <= 0x3FFF for row in struct.unpack("<2292H", data[16:])))

    def test_shared_extended_fallback_is_reproducible_without_theme_memory_growth(self):
        from tools.make_font import extended_header
        source = FONT.with_name('DejaVuSansMono.ttf')
        generated = extended_header(source)
        self.assertEqual(generated, (ROOT / 'arm9/source/ui/font_extended.h').read_text())
        for codepoint in ('0141', '03AC', '0416', '044F'):
            self.assertIn('U+' + codepoint, generated)

    def test_monochrome_bearings_keep_complete_glyphs(self):
        # FreeType's regular 11px w extends left; R extends right. Compare the
        # packed ink against an unclipped reference, not grayscale metrics.
        from PIL import Image, ImageDraw, ImageFont
        from tools.pack_theme_font import pack_font

        source = FONT.with_name("DejaVuSansMono.ttf")
        font = ImageFont.truetype(str(source), 11)
        packed = pack_font(source, 11, 9)
        for character in ("w", "R"):
            reference = Image.new("L", (40, 40))
            drawing = ImageDraw.Draw(reference)
            drawing.fontmode = "1"
            drawing.text((15, 20), character, font=font, fill=255, anchor="ls")
            expected = reference.crop(reference.getbbox())
            actual = Image.new("L", (7, 12))
            rows = struct.unpack_from("<12H", packed, 16 + (ord(character) - 32) * 24)
            for y, row in enumerate(rows):
                for x in range(7):
                    actual.putpixel((x, y), 255 if (row >> (x * 2)) & 3 else 0)
            actual = actual.crop(actual.getbbox())
            self.assertEqual(actual.size, expected.size)
            self.assertEqual(actual.tobytes(), expected.tobytes())
