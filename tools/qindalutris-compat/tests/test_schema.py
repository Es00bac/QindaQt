# SPDX-License-Identifier: GPL-3.0-or-later
"""The Python validator judges the SAME fixtures as the C++ parser.

tests/apps/qindalutris/data/compat/{valid,refused} are read by
tst_compat_db.cpp too; a fixture accepted on one side and refused on the
other is schema drift (AGENT-CONTRACT in qlcompat/schema.py).
"""

from __future__ import annotations

import copy
import json
import unittest

from tests.support import SHARED
from qlcompat import schema


def _load(path):
    return schema.load_bytes(path.read_bytes())


class SharedFixtures(unittest.TestCase):
    def test_valid_fixtures_are_accepted(self):
        names = sorted(p.name for p in (SHARED / "valid").glob("*.json"))
        self.assertEqual(names, ["basic.json", "minimal.json", "newer.json"])
        for path in (SHARED / "valid").glob("*.json"):
            with self.subTest(path.name):
                _load(path)

    def test_every_refused_fixture_is_refused(self):
        paths = sorted((SHARED / "refused").glob("*.json"))
        self.assertGreaterEqual(len(paths), 40)
        for path in paths:
            with self.subTest(path.name):
                with self.assertRaises(schema.Refused):
                    _load(path)


class Bounds(unittest.TestCase):
    def setUp(self):
        self.minimal = json.loads((SHARED / "valid" / "minimal.json").read_text())

    def test_oversize_bytes_are_refused(self):
        data = (SHARED / "valid" / "minimal.json").read_bytes()
        padded = data + b" " * (schema.MAX_DB_BYTES - len(data))
        schema.load_bytes(padded)
        with self.assertRaises(schema.Refused):
            schema.load_bytes(padded + b" ")

    def test_too_many_games_is_refused(self):
        doc = copy.deepcopy(self.minimal)
        doc["games"] = [{"id": f"g{i}", "title": "G", "keys": {}}
                        for i in range(schema.MAX_GAMES + 1)]
        with self.assertRaises(schema.Refused):
            schema.validate_document(doc)

    def test_lengths_count_utf16_units_like_qstring(self):
        doc = copy.deepcopy(self.minimal)
        # 128 astral characters are 256 UTF-16 units: exactly the title cap.
        doc["games"] = [{"id": "g", "title": "\U0001F3AE" * 128, "keys": {}}]
        schema.validate_document(doc)
        doc["games"][0]["title"] += "x"
        with self.assertRaises(schema.Refused):
            schema.validate_document(doc)

    def test_environment_rules(self):
        self.assertTrue(schema.is_env_assignment("WINEDLLOVERRIDES=locationapi=d"))
        self.assertTrue(schema.is_env_assignment("EMPTY="))
        for bad in ("PROTONPATH=/x", "STORE=egs", "LD_LIBRARY_PATH=/x", "PATH=/x",
                    "=x", "1A=x", "A-B=x", "A=\x1b", "Ünï=1"):
            with self.subTest(bad):
                self.assertFalse(schema.is_env_assignment(bad))

    def test_timestamps_are_exact(self):
        self.assertIsNotNone(schema.parse_timestamp("2026-09-25T23:59:59Z"))
        for bad in ("2026-09-25T24:00:00Z", "2026-09-25T00:00:00+00:00",
                    "2026-9-25T00:00:00Z", "2026-09-25T00:00:00Z\n", "0000-01-01T00:00:00Z"):
            with self.subTest(bad):
                self.assertIsNone(schema.parse_timestamp(bad))

    def test_urls(self):
        self.assertTrue(schema.is_https_url("https://lutris.net/games/battlenet/"))
        self.assertTrue(schema.is_https_url("https://host:8443?q=1#f"))
        for bad in ("http://x.org/", "https://", "https://x.org/a b", "https://x.org/é",
                    "https://user@x.org/", "ftp://x.org/"):
            with self.subTest(bad):
                self.assertFalse(schema.is_https_url(bad))


if __name__ == "__main__":
    unittest.main()
