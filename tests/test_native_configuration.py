import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import configuration
from native_configuration import decode_records, encode_records


class NativeConfigurationTests(unittest.TestCase):
    def test_native_round_trip_and_validation(self):
        records = {"state.ini": b"version=1\n", "appearance-1.ini": b"version=1\n"}
        archive = encode_records(records)
        self.assertEqual(decode_records(archive), records)
        for damaged in [archive[:-1], archive + b"x", archive[:8] + struct.pack("<I", 1025)]:
            with self.assertRaises(ValueError):
                decode_records(damaged)
        damaged = bytearray(archive)
        damaged[-1] ^= 1
        with self.assertRaises(ValueError):
            decode_records(damaged)
        for records in [{"../escape.ini": b"a"}, {"state.ini": b"\0"}]:
            with self.assertRaises(ValueError):
                encode_records(records)

    def test_computer_tool_native_export_import(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, destination = root / "source", root / "destination"
            source.mkdir()
            destination.mkdir()
            private = source / configuration.OWNER
            private.mkdir(parents=True)
            (private / "state.ini").write_text("version=1\ntheme=1\n")
            archive = root / "export.dsconfig"
            self.assertEqual(configuration.export_configuration(source, archive), 1)
            configuration.import_configuration(destination, archive)
            self.assertEqual(
                (destination / configuration.OWNER / "state.ini").read_bytes(),
                b"version=1\ntheme=1\n",
            )


if __name__ == "__main__":
    unittest.main()
