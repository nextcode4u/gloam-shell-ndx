"""Font authoring inputs are pinned, optional and independent of normal builds."""

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.theme_font_sources import verified_source


class FontSourceTests(unittest.TestCase):
    def test_cached_input_requires_matching_digest_without_network(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            data = b"synthetic font input"
            record = {"sha256": hashlib.sha256(data).hexdigest(), "url": "unused"}
            (cache / "sample.ttf").write_bytes(data)
            with patch(
                "urllib.request.urlopen",
                side_effect=AssertionError("unexpected network"),
            ):
                self.assertEqual(
                    verified_source("sample", "font", record, cache=cache).read_bytes(),
                    data,
                )
                (cache / "sample.ttf").write_bytes(b"changed")
                with self.assertRaisesRegex(ValueError, "checksum"):
                    verified_source("sample", "font", record, cache=cache)
                with self.assertRaisesRegex(FileNotFoundError, "download-fonts"):
                    verified_source("missing", "font", record, cache=cache)

    def test_invalid_identifiers_and_downloads_do_not_stage_files(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            for family in ("../escape", "a/b", "", "é"):
                with self.assertRaises(ValueError):
                    verified_source(family, "font", {}, cache=cache)
            with self.assertRaises(ValueError):
                verified_source(
                    "sample",
                    "font",
                    {"url": "http://example.com/font"},
                    download=True,
                    cache=cache,
                )
            self.assertEqual(list(cache.iterdir()), [])

    def test_failed_download_checksum_is_not_cached(self):
        import io

        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            record = {
                "url": "https://raw.githubusercontent.com/google/fonts/pin/ofl/sample/font.ttf",
                "sha256": "0" * 64,
            }
            with patch(
                "urllib.request.urlopen", return_value=io.BytesIO(b"wrong input")
            ):
                with self.assertRaisesRegex(ValueError, "checksum"):
                    verified_source(
                        "sample", "font", record, download=True, cache=cache
                    )
            self.assertEqual(list(cache.iterdir()), [])
