# SPDX-License-Identifier: GPL-3.0-or-later
"""Each upstream importer, against small offline copies of the real formats."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from tests.support import fixture_bytes, log_sink, seeded_cache
from qlcompat import awacy, lutris, protondb, steamdeck, umu, winetricks
from qlcompat.model import GameRecord, Registry


def _umu_registry(lines=None) -> Registry:
    registry = Registry()
    umu.apply(registry, fixture_bytes("umu-database.csv"), log_sink(lines if lines is not None else []))
    return registry


class UmuDatabase(unittest.TestCase):
    def test_groups_rows_by_umu_id_and_derives_keys(self):
        lines: list[str] = []
        registry = _umu_registry(lines)
        self.assertEqual(sorted(registry.records),
                         ["umu-1445591825", "umu-15750", "umu-397540", "umu-genshin"])
        bl3 = registry.records["umu-397540"]
        self.assertEqual(bl3.umu_id, "umu-397540")
        self.assertEqual(bl3.steam, {"397540"})
        self.assertEqual(bl3.stores, {"egs": {"Catnip"}})
        self.assertIsNone(bl3.umuStore)  # sold on egs and standalone
        self.assertEqual(bl3.notes, ["Standalone"])
        self.assertTrue(any("skipped 1 rows" in line for line in lines))

    def test_gog_numbered_umu_id_is_not_a_steam_appid(self):
        fe = _umu_registry().records["umu-1445591825"]
        self.assertEqual(fe.steam, set())
        self.assertEqual(fe.stores, {"gog": {"1445591825"}})
        self.assertEqual(fe.umuStore, "gog")
        self.assertEqual(list(fe.exe.values()), ["LegendaryHeroes.exe"])

    def test_generic_executable_names_are_not_keys(self):
        oddworld = _umu_registry().records["umu-15750"]
        self.assertEqual(oddworld.steam, {"15750"})
        self.assertEqual(oddworld.exe, {})

    def test_standalone_only_game_gets_store_none(self):
        genshin = _umu_registry().records["umu-genshin"]
        self.assertEqual(genshin.umuStore, "none")
        self.assertEqual(genshin.notes, ["Standalone PC installer"])

    def test_missing_column_is_an_error(self):
        with self.assertRaises(ValueError):
            umu.apply(Registry(), b"TITLE,STORE\nx,y\n", print)


class AreWeAntiCheatYet(unittest.TestCase):
    def setUp(self):
        self.lines: list[str] = []
        self.registry = _umu_registry()
        awacy.apply(self.registry, fixture_bytes("areweanticheatyet.json"),
                    log_sink(self.lines))

    def test_merges_by_steam_appid_and_maps_status(self):
        bl3 = self.registry.records["umu-397540"]
        self.assertEqual(bl3.antiCheat["status"], "supported")
        self.assertEqual(bl3.antiCheat["notes"], "Anti-cheat: Easy Anti-Cheat. Works online.")
        self.assertIn("https://example.org/note", bl3.links)
        self.assertIn("https://areweanticheatyet.com/game/borderlands-3", bl3.links)
        self.assertEqual(bl3.stores, {"egs": {"Catnip"}})  # epic catalog ids not imported

    def test_unique_title_merges_and_unknown_status_is_unknown(self):
        genshin = self.registry.records["umu-genshin"]
        self.assertEqual(genshin.antiCheat, {"status": "unknown"})
        self.assertEqual(genshin.awacy_slug, "genshin-impact")

    def test_unmatched_entries_become_records(self):
        wow = self.registry.records["awacy:wow"]
        self.assertEqual(wow.title, "World of Warcraft")
        self.assertEqual(wow.antiCheat["status"], "running")
        self.assertEqual(self.registry.records["awacy:r6"].steam, {"359550"})

    def test_second_entry_for_one_game_is_skipped_and_bad_slugs_ignored(self):
        self.assertNotIn("awacy:r6x", self.registry.records)
        self.assertTrue(any("r6x" in line for line in self.lines))
        self.assertFalse(any("bad" in key for key in self.registry.records))


class LutrisScripts(unittest.TestCase):
    def setUp(self):
        self.script = lutris._installer(fixture_bytes("lutris-battlenet.json"),
                                        "battlenet-standard")

    def test_only_the_named_published_wine_installer_is_read(self):
        self.assertIsNotNone(self.script)
        self.assertIsNone(lutris._installer(fixture_bytes("lutris-battlenet.json"),
                                            "battlenet-draft"))
        self.assertIsNone(lutris._installer(fixture_bytes("lutris-linux-only.json"),
                                            "native-standard"))

    def test_extracts_each_fact_kind(self):
        facts = lutris.extract(self.script, set(lutris.TAKE))
        self.assertEqual(facts["winetricks"], ["corefonts", "arial"])
        # $GAMEDIR paths, HUD/cache keys and planner-owned keys never pass.
        self.assertEqual(facts["environment"], ["STAGING_SHARED_MEMORY=1"])
        self.assertEqual(facts["dlloverrides"],
                         ["WINEDLLOVERRIDES=locationapi=d;nvapi,nvapi64=d"])
        self.assertEqual(facts["arguments"], ["--in-process", "--lang enUS"])
        self.assertEqual(facts["exe"], ["Battle.net Launcher.exe"])

    def test_take_limits_the_facts(self):
        self.assertEqual(set(lutris.extract(self.script, {"exe"})), {"exe"})


class Winetricks(unittest.TestCase):
    def test_parses_list_all_output(self):
        verbs = winetricks.parse_list(fixture_bytes("winetricks-list-all.txt").decode())
        self.assertEqual(verbs, {"arial", "corefonts", "vcrun2019", "renderer=vulkan", "win10"})

    def test_filter_drops_unknown_imported_verbs_and_refuses_curated_typos(self):
        registry = Registry()
        record = registry.add(GameRecord("g", "G"))
        record.winetricks = ["corefonts", "notaverb", "Bad;verb"]
        lines: list[str] = []
        winetricks.filter_records(registry, {"corefonts"}, {}, log_sink(lines))
        self.assertEqual(record.winetricks, ["corefonts"])
        self.assertEqual(len(lines), 2)
        record.winetricks = ["corefnts"]
        with self.assertRaises(ValueError):
            winetricks.filter_records(registry, {"corefonts"}, {"g": ["corefnts"]}, print)

    def test_without_a_list_only_the_charset_is_checked(self):
        registry = Registry()
        record = registry.add(GameRecord("g", "G"))
        record.winetricks = ["anything", "NOPE"]
        winetricks.filter_records(registry, None, {"g": ["anything"]}, lambda _: None)
        self.assertEqual(record.winetricks, ["anything"])


class ProtonDb(unittest.TestCase):
    def test_tiers_come_from_cache_within_the_limit(self):
        with tempfile.TemporaryDirectory() as cache:
            fetcher = seeded_cache(Path(cache))
            registry = _umu_registry()
            awacy.apply(registry, fixture_bytes("areweanticheatyet.json"), lambda _: None)
            order = [appid for appid, _ in protondb.candidates(registry)]
            # AreWeAntiCheatYet games first, then umu-only ones; short ids first.
            self.assertEqual(order, ["359550", "397540", "15750"])
            stamp = protondb.apply(registry, fetcher, 2, lambda _: None)
            self.assertEqual(stamp, "2026-09-21T00:00:00Z")
            self.assertEqual(registry.records["umu-397540"].tier, "gold")
            self.assertIn("https://www.protondb.com/app/397540",
                          registry.records["umu-397540"].links)
            self.assertIsNone(registry.records["awacy:r6"].tier)  # cached 404
            self.assertEqual(fetcher.network_requests, 0)
            self.assertIsNone(protondb.apply(_umu_registry(), fetcher, 0, print))

    def test_unexpected_tier_is_ignored(self):
        with tempfile.TemporaryDirectory() as cache:
            fetcher = seeded_cache(Path(cache))
            from qlcompat.fetch import CachedResponse
            fetcher.store("protondb-397540.json", CachedResponse(
                "https://x.org/", 200, "2026-09-21T00:00:00Z", json.dumps({"tier": "mythic"}).encode()))
            registry = _umu_registry()
            protondb.apply(registry, fetcher, 5, lambda _: None)
            self.assertIsNone(registry.records["umu-397540"].tier)


class SteamDeck(unittest.TestCase):
    def test_report_becomes_category_and_readable_notes(self):
        deck = steamdeck.parse(fixture_bytes("steamdeck-playable.json"))
        # Passed checks (display_type 4) are not notes; the rest read as text.
        self.assertEqual(deck, {"category": "playable", "notes": [
            "Default controller config not fully functional",
            "Interface text is not legible",
            "First time setup requires active internet connection"]})

    def test_unrated_and_unknown_reports_give_nothing(self):
        self.assertIsNone(steamdeck.parse(b'{"success": 1, "results": []}'))
        self.assertIsNone(steamdeck.parse(
            b'{"success": 1, "results": {"resolved_category": 0, "resolved_items": []}}'))
        self.assertIsNone(steamdeck.parse(b'{"success": 2, "results": {}}'))

    def test_categories_map_faithfully(self):
        for number, name in ((3, "verified"), (2, "playable"), (1, "unsupported")):
            body = json.dumps({"success": 1, "results": {"resolved_category": number,
                                                         "resolved_items": []}})
            self.assertEqual(steamdeck.parse(body.encode()), {"category": name})

    def test_tokens_become_sentences(self):
        self.assertEqual(steamdeck.readable(
            "#SteamDeckVerified_TestResult_UnsupportedAntiCheatConfiguration"),
            "Unsupported anti cheat configuration")
        self.assertEqual(steamdeck.readable("#X_TestResult_HDRNotSupported"),
                         "HDR not supported")


if __name__ == "__main__":
    unittest.main()
