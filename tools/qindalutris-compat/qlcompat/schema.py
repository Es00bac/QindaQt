# SPDX-License-Identifier: GPL-3.0-or-later
"""The compat-db-v1 validator.

AGENT-CONTRACT: this module is the Python twin of
src/apps/qindalutris/core/compat_db_parse.cpp and compat_db_rules.cpp. Every
rule here exists there and vice versa; change both in the same commit and
update docs/wiki/apps/qindalutris-compat-db.md. Lengths are UTF-16 code units
(u16len) because the C++ side measures QString::size(). The committed
snapshot is loaded by both test suites, so drift fails a gate.
"""

from __future__ import annotations

import datetime
import re
import unicodedata

SCHEMA_NAME = "qindalutris-compat-db"
SCHEMA_VERSION = 1

MAX_DB_BYTES = 32 * 1024 * 1024
MAX_GAMES = 100000
MAX_SOURCES = 32
MAX_BUILDS = 256
MAX_LIST = 64
MAX_ENV = 32  # kMaxExtraEnvironmentEntries
MAX_ENV_LINE = 1024 + 65  # kMaxEnvironmentValueChars + 65
MAX_TITLE = 256  # kMaxGameTitleChars
MAX_TEXT = 2048
MAX_URL = 2048
MAX_ARGUMENT = 512
MAX_BUILD_NAME = 128

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
# AGENT-GUARD: keys the launch planner owns, plus loader/search paths. A
# database that could set PROTONPATH would re-pin titles on refresh.
RESERVED_ENV = frozenset((
    "PROTONPATH", "WINEPREFIX", "GAMEID", "STORE", "UMU_ID",
    "UMU_RUNTIME_UPDATE", "PROTON_VERB", "STEAM_COMPAT_DATA_PATH",
    "STEAM_COMPAT_CLIENT_INSTALL_PATH", "PATH", "HOME", "WINE"))

_GAME_ID = re.compile(r"[a-z0-9][a-z0-9._:-]{0,127}")
_SOURCE_ID = re.compile(r"[a-z0-9][a-z0-9-]{0,63}")
_UMU_ID = re.compile(r"umu-[A-Za-z0-9._-]{1,124}")
_STEAM_ID = re.compile(r"[1-9][0-9]{0,9}")
_STORE_ID = re.compile(r"[A-Za-z0-9._:-]{1,128}")
_VERB = re.compile(r"[a-z0-9_=.-]{1,64}")
_BUILD = re.compile(r"[A-Za-z0-9][A-Za-z0-9 ._+()-]{0,127}")
_URL = re.compile(r"https://[A-Za-z0-9.-]+(:[0-9]{1,5})?([/?#][!-~]*)?")
_ENV_KEY = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,63}")
_TIMESTAMP = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z")


class Refused(ValueError):
    """The document breaks a rule; the whole document is refused."""


def u16len(text: str) -> int:
    # surrogatepass: a lone surrogate from a JSON \\ud800 escape is one
    # UTF-16 unit, as in QString, instead of an encoding crash.
    return len(text.encode("utf-16-le", "surrogatepass")) // 2


def is_text(text, max_len: int, allow_empty: bool = False) -> bool:
    if not isinstance(text, str) or u16len(text) > max_len:
        return False
    if not allow_empty and not text:
        return False
    return not any(unicodedata.category(ch) == "Cc" for ch in text)


def _full(pattern: re.Pattern, text) -> bool:
    return isinstance(text, str) and pattern.fullmatch(text) is not None


def is_game_id(text) -> bool: return _full(_GAME_ID, text)
def is_source_id(text) -> bool: return _full(_SOURCE_ID, text)
def is_umu_id(text) -> bool: return _full(_UMU_ID, text)
def is_steam_app_id(text) -> bool: return _full(_STEAM_ID, text)
def is_store_id(text) -> bool: return _full(_STORE_ID, text)
def is_winetricks_verb(text) -> bool: return _full(_VERB, text)


def is_build_name(text) -> bool:
    return _full(_BUILD, text) and not text.endswith(" ")


def is_https_url(text) -> bool:
    return _full(_URL, text) and len(text) <= MAX_URL


def is_exe_name(text) -> bool:
    return is_text(text, 128) and "/" not in text and "\\" not in text


def is_env_assignment(line) -> bool:
    """isValidEnvironmentAssignment plus the database's stricter key rule."""
    if not isinstance(line, str) or u16len(line) > MAX_ENV_LINE:
        return False
    equals = line.find("=")
    if equals <= 0 or not is_text(line, MAX_ENV_LINE):
        return False
    key = line[:equals]
    if not _full(_ENV_KEY, key) or key in RESERVED_ENV:
        return False
    return not key.startswith("LD_")


def parse_timestamp(text):
    """The exact "YYYY-MM-DDTHH:MM:SSZ" form; None on any deviation."""
    if not _full(_TIMESTAMP, text):
        return None
    try:
        return datetime.datetime.strptime(text, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=datetime.timezone.utc)
    except ValueError:
        return None


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
    _string_list(keys.get("steamAppIds", _MISSING), MAX_LIST, is_steam_app_id, f"{where}.steamAppIds")
    _string_list(keys.get("exeNames", _MISSING), MAX_LIST, is_exe_name, f"{where}.exeNames")
    _string_list(keys.get("titles", _MISSING), MAX_LIST, lambda t: is_text(t, MAX_TITLE),
                 f"{where}.titles")
    stores = keys.get("storeIds", {})
    _require(isinstance(stores, dict), f"{where}.storeIds")
    for store, ids in stores.items():
        _require(store in STORES, f"{where}.storeIds: unknown store {store!r}")
        _string_list(ids, MAX_LIST, is_store_id, f"{where}.storeIds.{store}", False)
        _require(len(ids) > 0, f"{where}.storeIds.{store}: empty")


def _check_proton(proton, where):
    _exact_keys(proton, (), ("recommended", "avoid"), where)
    if "recommended" in proton:
        _require(is_build_name(proton["recommended"]), f"{where}.recommended")
    avoid = proton.get("avoid", [])
    _require(isinstance(avoid, list) and len(avoid) <= MAX_LIST, f"{where}.avoid")
    builds = set()
    for item in avoid:
        _exact_keys(item, ("build", "reason", "source"), (), f"{where}.avoid")
        _require(is_build_name(item["build"]), f"{where}.avoid.build")
        _require(is_text(item["reason"], MAX_TEXT), f"{where}.avoid.reason")
        _require(is_text(item["source"], MAX_TEXT), f"{where}.avoid.source")
        _require(item["build"] not in builds, f"{where}.avoid: duplicate build")
        builds.add(item["build"])


def check_game(game, where="game"):
    _exact_keys(game, ("id", "title", "keys"),
                ("proton", "environment", "winetricks", "arguments", "antiCheat",
                 "protondbTier", "steamDeck", "umuStore", "notes", "links"), where)
    _require(is_game_id(game["id"]), f"{where}.id")
    where = f"game {game['id']}"
    _require(is_text(game["title"], MAX_TITLE), f"{where}.title")
    _check_keys(game["keys"], f"{where}.keys")
    if "proton" in game:
        _check_proton(game["proton"], f"{where}.proton")
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
    _string_list(game.get("environment", _MISSING), MAX_ENV, is_env_assignment, f"{where}.environment")
    _string_list(game.get("winetricks", _MISSING), MAX_LIST, is_winetricks_verb, f"{where}.winetricks")
    _string_list(game.get("arguments", _MISSING), MAX_LIST, lambda t: is_text(t, MAX_ARGUMENT),
                 f"{where}.arguments")
    _string_list(game.get("notes", _MISSING), MAX_LIST, lambda t: is_text(t, MAX_TEXT), f"{where}.notes")
    _string_list(game.get("links", _MISSING), MAX_LIST, is_https_url, f"{where}.links")


def _check_header(doc):
    _exact_keys(doc, ("schema", "version", "generated", "sources", "defaults",
                      "builds", "games"), (), "document")
    _require(doc["schema"] == SCHEMA_NAME, "schema")
    version = doc["version"]
    _require(isinstance(version, (int, float)) and not isinstance(version, bool)
             and version == SCHEMA_VERSION, "version")
    _require(parse_timestamp(doc["generated"]) is not None, "generated")
    sources = doc["sources"]
    _require(isinstance(sources, list) and len(sources) <= MAX_SOURCES, "sources")
    ids = set()
    for source in sources:
        _exact_keys(source, ("id", "url", "retrieved"), (), "sources")
        _require(is_source_id(source["id"]) and source["id"] not in ids, "sources.id")
        _require(is_https_url(source["url"]), "sources.url")
        _require(parse_timestamp(source["retrieved"]) is not None, "sources.retrieved")
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
    _require(isinstance(recommended, str) and (recommended == "" or recommended in builds),
             "defaults.recommendedBuild")


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


def validate_document(doc, byte_size: int | None = None) -> None:
    """Raise Refused on the first broken rule; return None when valid."""
    _require(byte_size is None or byte_size <= MAX_DB_BYTES, "document too large")
    _check_header(doc)
    games = doc["games"]
    _require(isinstance(games, list) and len(games) <= MAX_GAMES, "games")
    for game in games:
        check_game(game)
    _check_strong_keys(games)


def _refuse_constant(name):
    raise Refused(f"non-finite number {name}")


def load_bytes(data: bytes):
    """Parse and validate one document's bytes; raise Refused if invalid."""
    import json
    _require(len(data) <= MAX_DB_BYTES, "document too large")
    try:
        doc = json.loads(data.decode("utf-8"), parse_constant=_refuse_constant)
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise Refused(f"not JSON: {error}") from None
    validate_document(doc, len(data))
    return doc
