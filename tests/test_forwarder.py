"""The installable tile must have consistent identity/content and no save volumes."""

import hashlib
import struct
import unittest

from tools.build_forwarder import TITLE_ID, unsigned_tmd


class ForwarderTests(unittest.TestCase):
    def fixture(self):
        rom = bytearray(0x4000)
        rom[12:16] = b"GLDX"
        rom[0x12] = 3
        struct.pack_into("<Q", rom, 0x230, TITLE_ID)
        return rom

    def test_metadata_matches_exact_binary_without_a_signature(self):
        rom = self.fixture()
        tmd = unsigned_tmd(rom)
        self.assertEqual(len(tmd), 520)
        self.assertEqual(tmd[4:0x104], bytes(256))
        self.assertEqual(struct.unpack_from(">Q", tmd, 0x18C)[0], TITLE_ID)
        self.assertEqual(struct.unpack_from(">Q", tmd, 0x1EC)[0], len(rom))
        self.assertEqual(tmd[0x1F4:], hashlib.sha1(rom).digest())
        rom[-1] = 1
        self.assertNotEqual(unsigned_tmd(rom)[0x1F4:], tmd[0x1F4:])

    def test_refuses_wrong_identity_mode_or_save_requirements(self):
        for offset in [12, 0x12, 0x230, 0x238, 0x23C]:
            rom = self.fixture()
            rom[offset] ^= 1
            with self.assertRaises(ValueError):
                unsigned_tmd(rom)
        with self.assertRaises(ValueError):
            unsigned_tmd(bytes(512))
