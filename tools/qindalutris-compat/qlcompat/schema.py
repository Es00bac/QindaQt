# SPDX-License-Identifier: GPL-3.0-or-later
"""The compat-db-v1 validator: document structure and cross-field rules.

AGENT-CONTRACT: this module (with schema_rules.py) is the Python twin of
src/apps/qindalutris/core/compat_db_parse.cpp, compat_json_scan.cpp and
compat_db_rules.cpp. Every rule here exists there and vice versa; change
both in the same commit and update docs/wiki/apps/qindalutris-compat-db.md.
The shared fixtures and the differential cases under
tests/apps/qindalutris/ are judged by both sides, so drift fails a gate.
"""

from __future__ import annotations

import datetime
import json

from .schema_rules import (  # noqa: F401  (re-exported for the generator)
    MAX_ENV_LINE, MAX_TEXT, MAX_URL, WINETRICKS_HEADER, WINETRICKS_VERBS, env_key,
    is_build_name, is_env_assignment, is_env_key, is_exe_name, is_game_id,
    is_https_url, is_source_id, is_steam_app_id, is_store_id, is_text, is_umu_id,
    is_verb_shape, is_winetricks_verb, parse_timestamp, u16len)

SCHEMA_NAME = "qindalutris-compat-db"
SCHEMA_VERSION = 1

MAX_DB_BYTES = 32 * 1024 * 1024
MAX_GAMES = 100000
MAX_SOURCES = 32
MAX_BUILDS = 256
MAX_LIST = 64
MAX_ENV = 32  # kMaxExtraEnvironmentEntries
MAX_TITLE = 256  # kMaxGameTitleChars
MAX_ARGUMENT = 512
MAX_BUILD_NAME = 128
# A document stamped further ahead than this is refused: a wrong clock or a
# forged stamp must not let one copy win chooseNewer forever.
FUTURE_SLACK = datetime.timedelta(hours=24)

STORES = ("egs", "gog", "amazon", "ubisoft", "ea", "battlenet", "humble",
          "itchio", "zoomplatform")
UMU_STORES = ("amazon", "battlenet", "ea", "egs", "gog", "humble", "itchio",
              "steam", "ubisoft", "umu", "zoomplatform", "none")
BUILD_STATUSES = ("tested", "known-issues", "untested")
ANTICHEAT_STATUSES = ("supported", "running", "broken", "denied", "planned",
                      "none", "unknown")
PROTONDB_TIERS = ("platinum", "gold", "silver", "bronze", "borked", "native",
                  "pending", "unknown")
STEAM_DECK_CATEGORIES = ("verified", "playable", "unsupported", "unknown")


class Refused(ValueError):
    """The document breaks a rule; the whole document is refused."""


def _require(condition: bool, where: str) -> None:
    if not condition:
        raise Refused(where)


def _exact_keys(obj, required, optional=(), where="") -> None:
    _require(isinstance(obj, dict), f"{where}: not an object")
    for key in required:
        _require(key in obj, f"{where}: missing {key}")
    allowed = set(required) | set(optional)
    for key in obj:
        _require(key in allowed, f"{where}: unknown key {key!r}")


_MISSING = object()


def _string_list(value, max_entries, accept, where, optional=True):
    if value is _MISSING and optional:
        return []
    _require(isinstance(value, list) and len(value) <= max_entries,
             f"{where}: not a bounded list")
    seen = set()
    for entry in value:
        _require(isinstance(entry, str) and accept(entry), f"{where}: bad {entry!r}")
        _require(entry not in seen, f"{where}: duplicate {entry!r}")
        seen.add(entry)
    return value


def _check_keys(keys, where):
    _exact_keys(keys, (), ("umuId", "steamAppIds", "storeIds", "exeNames",
                           "titles"), where)
    if "umuId" in keys:
        _require(is_umu_id(keys["umuId"]), f"{where}.umuId")
    _string_list(keys.get("steamAppIds", _MISSING), MAX_LIST, is_steam_app_id,
                 f"{where}.steamAppIds")
    _string_list(keys.get("exeNames", _MISSING), MAX_LIST, is_exe_name, f"{where}.exeNames")
    _string_list(keys.get("titles", _MISSING), MAX_LIST, lambda t: is_text(t, MAX_TITLE),
                 f"{where}.titles")
    stores = keys.get("storeIds", {})
    _require(isinstance(stores, dict), f"{where}.storeIds")
    for store, ids in stores.items():
        _require(store in STORES, f"{where}.storeIds: unknown store {store!r}")
        _string_list(ids, MAX_LIST, is_store_id, f"{where}.storeIds.{store}", False)
        _require(len(ids) > 0, f"{where}.storeIds.{store}: empty")


def _is_tested(builds: dict, name) -> bool:
    return isinstance(name, str) and builds.get(name, {}).get("status") == "tested"


def _check_proton(proton, builds, where):
    _exact_keys(proton, (), ("recommended", "avoid"), where)
    if "recommended" in proton:
        _require(is_build_name(proton["recommended"]), f"{where}.recommended")
        # A pin may only name a build the database lists as tested.
        _require(_is_tested(builds, proton["recommended"]),
                 f"{where}.recommended: not a tested build")
    avoid = proton.get("avoid", [])
    _require(isinstance(avoid, list) and len(avoid) <= MAX_LIST, f"{where}.avoid")
    seen = set()
    for item in avoid:
        _exact_keys(item, ("build", "reason", "source"), (), f"{where}.avoid")
        _require(is_build_name(item["build"]), f"{where}.avoid.build")
        _require(is_text(item["reason"], MAX_TEXT), f"{where}.avoid.reason")
        _require(is_text(item["source"], MAX_TEXT), f"{where}.avoid.source")
        _require(item["build"] not in seen, f"{where}.avoid: duplicate build")
        seen.add(item["build"])


def _check_environment(value, where):
    lines = _string_list(value, MAX_ENV, is_env_assignment, where)
    keys = [env_key(line) for line in lines]
    _require(len(keys) == len(set(keys)), f"{where}: a key is set twice")


def check_game(game, builds: dict, where="game"):
    _exact_keys(game, ("id", "title", "keys"),
                ("proton", "environment", "winetricks", "arguments", "antiCheat",
                 "protondbTier", "steamDeck", "umuStore", "notes", "links"), where)
    _require(is_game_id(game["id"]), f"{where}.id")
    where = f"game {game['id']}"
    _require(is_text(game["title"], MAX_TITLE), f"{where}.title")
    _check_keys(game["keys"], f"{where}.keys")
    if "proton" in game:
        _check_proton(game["proton"], builds, f"{where}.proton")
    if "antiCheat" in game:
        anti = game["antiCheat"]
        _exact_keys(anti, ("status",), ("notes",), f"{where}.antiCheat")
        _require(anti["status"] in ANTICHEAT_STATUSES, f"{where}.antiCheat.status")
        if "notes" in anti:
            _require(is_text(anti["notes"], MAX_TEXT), f"{where}.antiCheat.notes")
    if "protondbTier" in game:
        _require(game["protondbTier"] in PROTONDB_TIERS, f"{where}.protondbTier")
    if "steamDeck" in game:
        deck = game["steamDeck"]
        _exact_keys(deck, ("category",), ("notes",), f"{where}.steamDeck")
        _require(deck["category"] in STEAM_DECK_CATEGORIES, f"{where}.steamDeck.category")
        _string_list(deck.get("notes", _MISSING), MAX_LIST, lambda t: is_text(t, MAX_TEXT),
                     f"{where}.steamDeck.notes")
    if "umuStore" in game:
        _require(game["umuStore"] in UMU_STORES, f"{where}.umuStore")
    _check_environment(game.get("environment", _MISSING), f"{where}.environment")
    _string_list(game.get("winetricks", _MISSING), MAX_LIST, is_winetricks_verb,
                 f"{where}.winetricks")
    _string_list(game.get("arguments", _MISSING), MAX_LIST,
                 lambda t: is_text(t, MAX_ARGUMENT), f"{where}.arguments")
    _string_list(game.get("notes", _MISSING), MAX_LIST, lambda t: is_text(t, MAX_TEXT),
                 f"{where}.notes")
    _string_list(game.get("links", _MISSING), MAX_LIST, is_https_url, f"{where}.links")


def _check_stamp(text, now, where):
    stamp = parse_timestamp(text)
    _require(stamp is not None, where)
    _require(stamp <= now + FUTURE_SLACK, f"{where}: more than 24 h in the future")


def _check_header(doc, now):
    _exact_keys(doc, ("schema", "version", "generated", "sources", "defaults",
                      "builds", "games"), (), "document")
    _require(doc["schema"] == SCHEMA_NAME, "schema")
    version = doc["version"]
    _require(isinstance(version, (int, float)) and not isinstance(version, bool)
             and version == SCHEMA_VERSION, "version")
    _check_stamp(doc["generated"], now, "generated")
    sources = doc["sources"]
    _require(isinstance(sources, list) and len(sources) <= MAX_SOURCES, "sources")
    ids = set()
    for source in sources:
        _exact_keys(source, ("id", "url", "retrieved"), (), "sources")
        _require(is_source_id(source["id"]) and source["id"] not in ids, "sources.id")
        _require(is_https_url(source["url"]), "sources.url")
        _check_stamp(source["retrieved"], now, "sources.retrieved")
        ids.add(source["id"])
    builds = doc["builds"]
    _require(isinstance(builds, dict) and len(builds) <= MAX_BUILDS, "builds")
    for name, info in builds.items():
        _require(is_build_name(name), f"builds: bad name {name!r}")
        _exact_keys(info, ("status", "notes"), (), f"builds.{name}")
        _require(info["status"] in BUILD_STATUSES, f"builds.{name}.status")
        _require(is_text(info["notes"], MAX_TEXT, True), f"builds.{name}.notes")
    defaults = doc["defaults"]
    _exact_keys(defaults, ("recommendedBuild",), (), "defaults")
    recommended = defaults["recommendedBuild"]
    _require(recommended == "" or _is_tested(builds, recommended),
             "defaults.recommendedBuild: not empty and not a tested build")


def _check_strong_keys(games):
    """CompatDatabase::fromDocument: ids and strong keys name one game."""
    seen = {}
    game_ids = set()
    for game in games:
        _require(game["id"] not in game_ids, f"duplicate game id {game['id']}")
        game_ids.add(game["id"])
        keys = game["keys"]
        claims = []
        if "umuId" in keys:
            claims.append(("umu", keys["umuId"]))
        claims += [("steam", appid) for appid in keys.get("steamAppIds", [])]
        for store, ids in keys.get("storeIds", {}).items():
            claims += [("store", f"{store}\x1f{i}") for i in ids]
        for claim in claims:
            owner = seen.setdefault(claim, game["id"])
            _require(owner == game["id"], f"strong key {claim[0]} {claim[1]!r} shared by "
                                          f"{owner} and {game['id']}")


def utc_now() -> datetime.datetime:
    return datetime.datetime.now(datetime.timezone.utc)


def validate_document(doc, byte_size: int | None = None, now=None) -> None:
    """Raise Refused on the first broken rule; return None when valid.

    `now` is the injected clock (UTC datetime); default the real one.
    """
    now = now or utc_now()
    _require(byte_size is None or byte_size <= MAX_DB_BYTES, "document too large")
    _check_header(doc, now)
    games = doc["games"]
    _require(isinstance(games, list) and len(games) <= MAX_GAMES, "games")
    for game in games:
        check_game(game, doc["builds"])
    _check_strong_keys(games)


def _refuse_constant(name):
    raise Refused(f"non-finite number {name}")


def _refuse_duplicates(pairs):
    # AGENT-CONTRACT: duplicate object keys refuse the document on both
    # sides (compat_json_scan.cpp does the same on the raw bytes).
    result = {}
    for key, value in pairs:
        if key in result:
            raise Refused(f"duplicate object key {key!r}")
        result[key] = value
    return result


def load_bytes(data: bytes, now=None):
    """Parse and validate one document's bytes; raise Refused if invalid."""
    _require(len(data) <= MAX_DB_BYTES, "document too large")
    try:
        # Strict UTF-8 without BOM skipping: a BOM is refused, as in C++.
        doc = json.loads(data.decode("utf-8"), parse_constant=_refuse_constant,
                         object_pairs_hook=_refuse_duplicates)
    except (UnicodeDecodeError, RecursionError, ValueError) as error:
        # ValueError covers JSONDecodeError and the int-digit limit.
        if isinstance(error, Refused):
            raise
        raise Refused(f"not JSON: {error}") from None
    validate_document(doc, len(data), now)
    return doc
