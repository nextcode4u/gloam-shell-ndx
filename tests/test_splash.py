from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave
import zlib

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pack_splash
import splash_builder


def fixture(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    Image.new("RGB", (256, 192), "red").save(root / "top.png")
    Image.new("RGB", (256, 192), "blue").save(root / "bottom.png")
    Image.new("RGBA", (32, 32), "lime").save(root / "logo.png")
    values = dict(
        top=root / "top.png",
        bottom=root / "bottom.png",
        top_logo=root / "logo.png",
        logo_width=32,
        duration=2,
        effect="fade",
        transition=0.5,
    )
    data = pack_splash.build(**values)
    (root / "valid.gsp").write_bytes(data)
    with wave.open(str(root / "sound.wav"), "wb") as audio:
        audio.setparams((2, 2, 8000, 0, "NONE", "not compressed"))
        audio.writeframes(struct.pack("<2h", 8000, -4000) * 8000)
    (root / "sound.gsp").write_bytes(
        pack_splash.build(**values, sound=root / "sound.wav")
    )
    direct = pack_splash.build(**values, direct=True)
    theme = root / "_nds/dsi-frontend/themes/probe"
    default = root / "_nds/dsi-frontend/splashes"
    theme.mkdir(parents=True, exist_ok=True)
    default.mkdir(parents=True, exist_ok=True)
    (theme / "splash.gsp").write_bytes(direct)
    (default / "default.gsp").write_bytes(pack_splash.build(bottom=root / "bottom.png"))
    for name, offset, fmt, value in (
        ("duration", 16, "H", 0),
        ("layers", 20, "B", 5),
        ("width", 36, "H", 257),
        ("frames", 40, "H", 65535),
        ("offset", 52, "I", 0),
        ("sound", 28, "I", 4),
    ):
        bad = bytearray(data)
        struct.pack_into("<" + fmt, bad, offset, value)
        struct.pack_into("<I", bad, 12, zlib.crc32(bad[16:]))
        (root / ("bad-" + name + ".gsp")).write_bytes(bad)
    return values, data


class SplashTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.values, self.data = fixture(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def test_layout_timing_palette_and_reduced_motion(self):
        data = self.data
        self.assertEqual(data[:8], b"GLOMSP01")
        self.assertEqual(struct.unpack_from("<I", data, 8)[0], len(data))
        self.assertEqual(struct.unpack_from("<I", data, 12)[0], zlib.crc32(data[16:]))
        self.assertLess(len(data), 103000)
        first = splash_builder.render(data, 0)
        final = splash_builder.render(data, 30)
        reduced = splash_builder.render(data, 0, True)
        self.assertEqual(first.getpixel((128, 96)), (255, 0, 0))
        self.assertEqual(final.getpixel((128, 96)), (0, 255, 0))
        self.assertEqual(final.getpixel((128, 288)), (0, 0, 255))
        self.assertEqual(reduced.tobytes(), final.tobytes())

    def test_gif_timing_and_numbered_frames(self):
        first = Image.new("RGB", (16, 16), "red")
        second = Image.new("RGB", (16, 16), "blue")
        path = self.root / "animation.gif"
        first.save(
            path, save_all=True, append_images=[second], duration=[100, 200], loop=0
        )
        frames = pack_splash.source_frames(path, 20)
        self.assertEqual(len(frames), 6)
        self.assertEqual(frames[1].getpixel((0, 0))[:3], (255, 0, 0))
        self.assertEqual(frames[2].getpixel((0, 0))[:3], (0, 0, 255))
        folder = self.root / "frames"
        folder.mkdir()
        first.save(folder / "frame2.png")
        second.save(folder / "frame10.png")
        frames = pack_splash.source_frames(folder, 20)
        self.assertEqual(frames[0].getpixel((0, 0))[:3], (255, 0, 0))

    def test_transparency_black_and_direct_colour(self):
        image = Image.new("RGBA", (2, 1), (0, 0, 0, 0))
        image.putpixel((1, 0), (0, 0, 0, 255))
        indexed = pack_splash.encode_frames([image], False)
        self.assertEqual(indexed[512], 0)
        self.assertNotEqual(indexed[513], 0)
        self.assertEqual(struct.unpack_from("<H", indexed, indexed[513] * 2)[0], 0x8000)
        self.assertEqual(pack_splash.encode_frames([image], True), b"\0\0\0\x80")
        direct = pack_splash.build(**self.values, direct=True)
        self.assertEqual(
            splash_builder.render(direct, 30).tobytes(),
            splash_builder.render(self.data, 30).tobytes(),
        )

    def test_sound_conversion_clipping_and_alignment(self):
        # A stereo 8 kHz fixture becomes centered mono 16 kHz PCM, trimmed
        # to the splash duration. Storage is aligned for the ARM7 sound DMA.
        pcm = pack_splash.sound_pcm(self.root / "sound.wav", 30)
        self.assertEqual(len(pcm), 16000)
        self.assertEqual(set(struct.unpack("<8000h", pcm)), {2000})
        data = (self.root / "sound.gsp").read_bytes()
        offset, size = struct.unpack_from("<2I", data, 24)
        self.assertEqual(size, 32000)
        self.assertEqual(offset + size, len(data))
        self.assertEqual(offset % 4, 0)
        with wave.open(str(self.root / "unsigned.wav"), "wb") as audio:
            audio.setparams((1, 1, 16000, 0, "NONE", "not compressed"))
            audio.writeframes(bytes([0, 128, 255]))
        self.assertEqual(
            struct.unpack("<4h", pack_splash.sound_pcm(self.root / "unsigned.wav", 30)),
            (-32768, 0, 32512, 0),
        )

    def test_limits_and_export_preservation(self):
        for change in (
            {"fps": 17},
            {"duration": 11},
            {"duration": 0},
            {"duration": float("nan")},
            {"poster": 2},
            {"logo_width": 300},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                pack_splash.build(**(self.values | change))
        output = self.root / "export.gsp"
        pack_splash.write_package(output, self.data)
        with self.assertRaises(ValueError):
            pack_splash.write_package(output, b"replacement")
        self.assertEqual(output.read_bytes(), self.data)

    def test_custom_install_preserves_navigation_and_unknown_settings(self):
        sd = self.root / "sd"
        folder = sd / "_nds/dsi-frontend"
        folder.mkdir(parents=True)
        previous = b"version=1\nunknown=keep\nfavorite=sd:/a.nds\nfavorite=sd:/b.nds\nsplash_mode=0\nsplash_once=1\n"
        (folder / "state.ini").write_bytes(previous)
        target = splash_builder.install_custom(sd, self.data)
        self.assertEqual(target.read_bytes(), self.data)
        self.assertEqual((folder / "state.bak").read_bytes(), previous)
        state = (folder / "state.ini").read_text()
        self.assertIn("unknown=keep", state)
        self.assertIn("favorite=sd:/a.nds\nfavorite=sd:/b.nds", state)
        self.assertIn("splash_once=1", state)
        self.assertIn("splash_mode=2", state)
        self.assertEqual(state.count("splash_mode="), 1)
        (folder / "state.ini").write_bytes(b"broken state")
        with self.assertRaises(ValueError):
            splash_builder.install_custom(sd, self.data)
        self.assertEqual((folder / "state.ini").read_bytes(), b"broken state")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--fixture":
        fixture(sys.argv[2])
    else:
        unittest.main()
