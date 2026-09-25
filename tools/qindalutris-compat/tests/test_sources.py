# SPDX-License-Identifier: GPL-3.0-or-later
"""Each upstream importer, against small offline copies of the real formats."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from tests.support import fixture_bytes, log_sink, seeded_cache
from qlcompat import awacy, lutris, protondb, schema, schema_rules, steamdeck, umu, winetricks
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


def _steam_name(appid):
    return {"359550": "Rainbow Six Siege", "961200": "Predecessor"}.get(appid)


class AreWeAntiCheatYet(unittest.TestCase):
    def setUp(self):
        self.lines: list[str] = []
        self.registry = _umu_registry()
        awacy.apply(self.registry, fixture_bytes("areweanticheatyet.json"),
                    log_sink(self.lines), _steam_name)

    def test_merges_by_steam_appid_and_maps_status(self):
        bl3 = self.registry.records["umu-397540"]
        self.assertEqual(bl3.antiCheat["status"], "supported")
        # "Show your support!" keeps its own punctuation.
        self.assertEqual(bl3.antiCheat["notes"],
                         "Anti-cheat: Easy Anti-Cheat. Works online. Show your support!")
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

    def test_shared_appid_goes_only_to_the_entry_named_like_steam(self):
        self.assertEqual(self.registry.records["awacy:r6"].steam, {"359550"})
        self.assertEqual(self.registry.records["awacy:r6x"].steam, set())
        self.assertEqual(self.registry.records["awacy:r6x"].antiCheat["status"], "broken")
        self.assertEqual(self.registry.records["awacy:predecessor"].steam, {"961200"})
        self.assertEqual(self.registry.records["awacy:final-fantasy-xiv"].steam, set())
        decisions = [line for line in self.lines if "claimed by" in line]
        self.assertEqual(len(decisions), 2)
        self.assertTrue(all("withheld" in line for line in decisions))

    def test_without_steam_names_every_claimant_is_withheld(self):
        registry, lines = Registry(), []
        awacy.apply(registry, fixture_bytes("areweanticheatyet.json"), log_sink(lines))
        for slug in ("r6", "r6x", "predecessor", "final-fantasy-xiv"):
            self.assertEqual(registry.records["awacy:" + slug].steam, set(), slug)
        self.assertTrue(any("all are withheld" in line for line in lines))

    def test_withheld_entry_cannot_reach_its_appid_through_a_title(self):
        registry = Registry()
        owner = registry.add(GameRecord("umu-359550", "Rainbow Six Siege X"))
        owner.steam.add("359550")
        awacy.apply(registry, fixture_bytes("areweanticheatyet.json"), lambda _: None,
                    _steam_name)
        # r6 keeps the appid and joins the owner; r6x may not title-join it.
        self.assertEqual(owner.awacy_slug, "r6")
        self.assertIn("awacy:r6x", registry.records)

    def test_encoded_and_non_ascii_slugs_get_stable_safe_ids(self):
        sbox = awacy.slug_id("s%26box")
        light = awacy.slug_id("light⚡️nite")
        self.assertRegex(sbox, r"^awacy:s-box-[0-9a-f]{8}$")
        self.assertRegex(light, r"^awacy:lightnite-[0-9a-f]{8}$")
        self.assertEqual(sbox, awacy.slug_id("s%26box"))
        self.assertNotEqual(awacy.slug_id("s-box"), sbox)
        self.assertEqual(awacy.slug_id("plain-slug"), "awacy:plain-slug")
        self.assertIn(sbox, self.registry.records)
        self.assertIn("https://areweanticheatyet.com/game/s%26box",
                      self.registry.records[sbox].links)
        self.assertIn("https://areweanticheatyet.com/game/light%E2%9A%A1%EF%B8%8Fnite",
                      self.registry.records[light].links)

    def test_entries_without_a_name_are_logged_and_skipped(self):
        self.assertTrue(any("no-name" in line for line in self.lines))


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

    def test_extracts_only_verbs_and_environment_settings(self):
        facts = lutris.extract(self.script, set(lutris.TAKE))
        self.assertEqual(set(facts), {"winetricks", "environment", "dlloverrides"})
        self.assertEqual(facts["winetricks"], ["corefonts", "arial"])
        # $GAMEDIR paths, HUD/cache keys and non-allowlisted keys never pass.
        self.assertEqual(facts["environment"], ["STAGING_SHARED_MEMORY=1"])
        self.assertEqual(facts["dlloverrides"],
                         ["WINEDLLOVERRIDES=locationapi=d;nvapi,nvapi64=d"])

    def test_take_limits_the_facts(self):
        self.assertEqual(set(lutris.extract(self.script, {"winetricks"})), {"winetricks"})


class Winetricks(unittest.TestCase):
    def test_list_all_is_restricted_to_dlls_fonts_and_settings(self):
        text = fixture_bytes("winetricks-list-all.txt").decode()
        self.assertEqual(winetricks.allowlist_from(text),
                         ["arial", "corefonts", "renderer=vulkan", "vcrun2019", "win10"])

    def test_forbidden_verbs_never_enter_the_list(self):
        for verb in ("-q", "annihilate", "prefix=x", "arch=win32", "list-all", "bad",
                     "winver="):
            self.assertTrue(winetricks.is_forbidden(verb), verb)
        self.assertFalse(winetricks.is_forbidden("corefonts"))

    def test_rendered_file_round_trips_through_the_schema_loader(self):
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / "verbs.txt"
            path.write_text(winetricks.render_file(["corefonts", "win10"], "20260125",
                                                   "2026-09-25T00:00:00Z"))
            verbs, header = schema_rules.load_verb_file(path)
        self.assertEqual(verbs, {"corefonts", "win10"})
        self.assertEqual(header["winetricks-version"], "20260125")

    def test_committed_list_is_clean(self):
        verbs = schema.WINETRICKS_VERBS
        self.assertGreater(len(verbs), 300)
        self.assertFalse([v for v in verbs if winetricks.is_forbidden(v)])
        self.assertTrue({"corefonts", "win10", "d3dcompiler_47"} <= verbs)
        self.assertNotIn("7zip", verbs)  # apps category
        self.assertRegex(schema.WINETRICKS_HEADER["winetricks-version"], r"^[0-9]{8}$")

    def test_filter_drops_unknown_imported_verbs_and_refuses_curated_ones(self):
        registry = Registry()
        record = registry.add(GameRecord("g", "G"))
        record.winetricks = ["corefonts", "notaverb", "Bad;verb"]
        lines: list[str] = []
        winetricks.filter_records(registry, frozenset({"corefonts"}), {}, log_sink(lines))
        self.assertEqual(record.winetricks, ["corefonts"])
        self.assertEqual(len(lines), 2)
        record.winetricks = ["corefnts"]
        with self.assertRaises(ValueError):
            winetricks.filter_records(registry, frozenset({"corefonts"}),
                                      {"g": ["corefnts"]}, print)


class ProtonDb(unittest.TestCase):
    def test_tiers_come_from_cache_within_the_limit(self):
        with tempfile.TemporaryDirectory() as cache:
            fetcher = seeded_cache(Path(cache))
            registry = _umu_registry()
            awacy.apply(registry, fixture_bytes("areweanticheatyet.json"), lambda _: None,
                        _steam_name)
            order = [appid for appid, _ in protondb.candidates(registry)]
            # AreWeAntiCheatYet games first, then umu-only ones; short ids first.
            self.assertEqual(order, ["359550", "397540", "961200", "15750"])
            stamp = protondb.apply(registry, fetcher, 3, lambda _: None)
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
            "Some functions are not reachable with the default controller layout.",
            "Some text is small and may be hard to read.",
            "First-time setup needs an internet connection."]})

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

    def test_known_tokens_become_sentences(self):
        self.assertEqual(steamdeck.readable(
            "#SteamDeckVerified_TestResult_UnsupportedAntiCheatConfiguration"),
            "The game's anti-cheat is not set up to allow SteamOS.")
        for suffix in ("Retired", "VR"):
            text = steamdeck.readable(
                f"#SteamDeckVerified_TestResult_SteamOSDoesNotSupport_{suffix}")
            self.assertIn("SteamOS", text)
            self.assertGreater(len(text.split()), 4)
        self.assertEqual(steamdeck.readable(
            "#SteamDeckVerified_TestResult_UnsupportedAntiCheat_Other"),
            "The game's anti-cheat does not support SteamOS.")

    def test_unknown_tokens_split_the_whole_remainder(self):
        self.assertEqual(steamdeck.readable("#X_TestResult_HDRNotSupported"),
                         "HDR not supported.")
        self.assertEqual(steamdeck.readable("#SteamDeckVerified_TestResult_New_ThingBroken"),
                         "New thing broken.")


if __name__ == "__main__":
    unittest.main()
