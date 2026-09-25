# SPDX-License-Identifier: GPL-3.0-or-later
"""End to end, offline: cache in, deterministic schema-valid document out."""

from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
import urllib.error
from pathlib import Path

from tests.support import FIXTURES, SNAPSHOT, STAMP, seeded_cache
from qlcompat import curated, emit, schema
from qlcompat.fetch import CachedResponse, Fetcher

import generate  # noqa: E402  (tools/qindalutris-compat is the test top level)


def _args(**overrides):
    values = dict(out="unused", generated="2026-09-25T00:00:00Z", protondb_limit=10,
                  curated=str(FIXTURES / "curated.toml"), winetricks="winetricks-absent",
                  winetricks_list=None)
    values.update(overrides)
    return type("Args", (), values)()


class OfflineGeneration(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.cache = Path(self.temp.name) / "cache"
        seeded_cache(self.cache)

    def tearDown(self):
        self.temp.cleanup()

    def _generate(self, **overrides) -> dict:
        fetcher = Fetcher(self.cache, offline=True)
        return generate.generate(_args(**overrides), fetcher, log=lambda _: None)

    def test_output_is_valid_and_deterministic(self):
        first = emit.dumps(self._generate())
        second = emit.dumps(self._generate())
        self.assertEqual(first, second)
        schema.load_bytes(first.encode())
        games = [json.loads(line.rstrip(",")) for line in first.splitlines()
                 if line.startswith('  {"')]
        self.assertEqual([g["id"] for g in games], sorted(g["id"] for g in games))

    def test_sources_record_retrieval_dates(self):
        doc = self._generate()
        self.assertEqual({s["id"]: s["retrieved"] for s in doc["sources"]}, {
            "areweanticheatyet": STAMP, "lutris": STAMP, "protondb": "2026-09-21T00:00:00Z",
            "steam-deck": "2026-09-22T00:00:00Z", "umu-database": STAMP, "winetricks": STAMP})

    def test_curated_facts_win_and_pull_in_upstream_records(self):
        games = {g["id"]: g for g in self._generate()["games"]}
        wow = games["world-of-warcraft"]
        self.assertNotIn("awacy:wow", games)  # joined via match.awacy
        self.assertEqual(wow["antiCheat"]["status"], "running")
        self.assertEqual(wow["proton"]["avoid"][0]["build"], "GE-Proton11-7-x86_64")
        self.assertEqual(wow["keys"]["storeIds"], {"battlenet": ["wow"]})
        bl3 = games["borderlands-3"]  # joined via its Steam appid
        self.assertNotIn("umu-397540", games)
        self.assertEqual(bl3["keys"]["umuId"], "umu-397540")
        self.assertEqual(bl3["winetricks"], ["win10"])
        self.assertEqual(bl3["protondbTier"], "gold")
        self.assertEqual(bl3["steamDeck"]["category"], "playable")
        self.assertIn("https://store.steampowered.com/app/397540/", bl3["links"])
        self.assertIn("Borderlands 3", [bl3["title"]] + bl3["keys"].get("titles", []))

    def test_lutris_facts_are_validated_against_winetricks(self):
        launcher = {g["id"]: g for g in self._generate()["games"]}["battlenet-launcher"]
        self.assertEqual(launcher["winetricks"], ["corefonts", "arial"])
        self.assertEqual(launcher["environment"], [
            "STAGING_SHARED_MEMORY=1", "WINEDLLOVERRIDES=locationapi=d;nvapi,nvapi64=d"])
        self.assertEqual(launcher["keys"]["exeNames"], ["Battle.net Launcher.exe"])
        self.assertIn("https://lutris.net/games/battlenet/", launcher["links"])

    def test_empty_cache_offline_still_yields_curated_only_document(self):
        fetcher = Fetcher(Path(self.temp.name) / "empty", offline=True)
        doc = generate.generate(_args(), fetcher, log=lambda _: None)
        self.assertEqual(doc["sources"], [])
        self.assertEqual(sorted(g["id"] for g in doc["games"]),
                         ["battlenet-launcher", "borderlands-3", "world-of-warcraft"])

    def test_main_writes_a_validated_file(self):
        out = Path(self.temp.name) / "out" / "compat-db-v1.json"
        with contextlib.redirect_stderr(io.StringIO()):
            status = generate.main(["--out", str(out), "--cache-dir", str(self.cache),
                                    "--offline", "--generated", "2026-09-25T00:00:00Z",
                                    "--curated", str(FIXTURES / "curated.toml"),
                                    "--winetricks", "winetricks-absent"])
        self.assertEqual(status, 0)
        schema.load_bytes(out.read_bytes())


class CuratedRules(unittest.TestCase):
    def _load(self, text: str):
        with tempfile.NamedTemporaryFile("w", suffix=".toml", delete=False) as handle:
            handle.write(text)
        try:
            return curated.load(handle.name)
        finally:
            Path(handle.name).unlink()

    def test_facts_without_evidence_are_refused(self):
        base = '[defaults]\nsource = "x"\n'
        with self.assertRaises(curated.CuratedError):
            self._load('[defaults]\nrecommendedBuild = ""\n')
        with self.assertRaises(curated.CuratedError):
            self._load(base + '[[games]]\nid = "g"\ntitle = "G"\n')
        with self.assertRaises(curated.CuratedError):
            self._load(base + '[[games]]\nid = "g"\ntitle = "G"\nsource = "s"\n'
                       '[[games.proton.avoid]]\nbuild = "B"\nreason = "r"\n')

    def test_schema_breaking_curated_values_are_refused(self):
        base = '[defaults]\nsource = "x"\n[[games]]\nid = "g"\ntitle = "G"\nsource = "s"\n'
        for extra in ('environment = ["PROTONPATH=/opt"]', 'links = ["http://x.org"]',
                      'winetricks = ["Bad"]', 'unknown = 1'):
            with self.subTest(extra):
                with self.assertRaises(curated.CuratedError):
                    self._load(base + extra + "\n")

    def test_the_real_curated_file_loads(self):
        data = curated.load(str(Path(generate.HERE) / "curated.toml"))
        self.assertEqual(data["defaults"]["recommendedBuild"], "GE-Proton11-6-x86_64")


class Fetching(unittest.TestCase):
    def test_http_is_refused_and_errors_fall_back_to_cache(self):
        with tempfile.TemporaryDirectory() as cache:
            def failing(request, timeout):
                raise urllib.error.URLError("offline")
            fetcher = Fetcher(Path(cache), refresh=True, min_interval=0, opener=failing,
                              log=lambda _: None)
            with self.assertRaises(ValueError):
                fetcher.get("http://example.org/", "x")
            self.assertIsNone(fetcher.get("https://example.org/", "x"))
            fetcher.store("x", CachedResponse("https://example.org/", 200, STAMP, b"old"))
            self.assertEqual(fetcher.get("https://example.org/", "x").body, b"old")

    def test_success_is_cached_with_its_date(self):
        with tempfile.TemporaryDirectory() as cache:
            def ok(request, timeout):
                return io.BytesIO(b"fresh")
            fetcher = Fetcher(Path(cache), min_interval=0, opener=ok)
            response = fetcher.get("https://example.org/a", "a")
            self.assertEqual(response.body, b"fresh")
            self.assertIsNotNone(schema.parse_timestamp(response.retrieved))
            self.assertEqual(Fetcher(Path(cache), offline=True).get(
                "https://example.org/a", "a").body, b"fresh")


class CommittedSnapshot(unittest.TestCase):
    def test_snapshot_validates_and_carries_the_curated_pins(self):
        doc = schema.load_bytes(SNAPSHOT.read_bytes())
        games = {g["id"]: g for g in doc["games"]}
        self.assertGreater(len(games), 1000)
        self.assertEqual(doc["defaults"]["recommendedBuild"], "GE-Proton11-6-x86_64")
        wow = games["world-of-warcraft"]
        self.assertEqual(wow["proton"]["avoid"][0]["build"], "GE-Proton11-7-x86_64")
        wc3 = games["warcraft-iii-reforged"]
        self.assertEqual(wc3["proton"]["recommended"], "GE-Proton11-6-wc3crypt32-x86_64")
        self.assertEqual(emit.dumps(doc), SNAPSHOT.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
