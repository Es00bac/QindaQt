# SPDX-License-Identifier: GPL-3.0-or-later
"""Curated overrides (curated.toml): the highest-precedence facts.

Every curated fact carries its evidence: a `source` on each game, build,
avoid entry, anti-cheat verdict and on the defaults. A curated game may pull
in upstream records (`match`, or its own strong keys) and name one Lutris
installer to read facts from; its own fields then override everything.
The exact table layout is documented in tools/qindalutris-compat/README.md.
"""

from __future__ import annotations

import tomllib

from . import lutris, schema
from .fetch import Fetcher
from .model import GameRecord, Registry

GAME_KEYS = {"id", "title", "source", "match", "umuStore", "keys", "lutris",
             "environment", "winetricks", "arguments", "notes", "links", "proton",
             "antiCheat"}


class CuratedError(ValueError):
    """curated.toml breaks its own rules; generation stops."""


def _need(condition: bool, message: str) -> None:
    if not condition:
        raise CuratedError(message)


def _evidence(table: dict, where: str) -> None:
    _need(schema.is_text(table.get("source"), schema.MAX_TEXT),
          f"{where}: every curated fact needs a non-empty `source`")


def _check_game(game: dict, builds: dict) -> None:
    where = f"curated game {game.get('id')!r}"
    _need(isinstance(game, dict) and set(game) <= GAME_KEYS, f"{where}: unknown keys")
    _need(schema.is_game_id(game.get("id")), f"{where}: bad id")
    _need(schema.is_text(game.get("title"), schema.MAX_TITLE), f"{where}: bad title")
    _evidence(game, where)
    _need(set(game.get("match", {})) <= {"umuId", "awacy"}, f"{where}: bad match")
    for avoid in game.get("proton", {}).get("avoid", []):
        _evidence(avoid, f"{where} avoid {avoid.get('build')}")
    if "antiCheat" in game:
        _evidence(game["antiCheat"], f"{where} antiCheat")
    if "lutris" in game:
        spec = game["lutris"]
        _need(set(spec) == {"slug", "installer", "take"}
              and set(spec["take"]) <= lutris.TAKE, f"{where}: bad lutris table")
    # The emitted game must pass the schema on its own; check it now so an
    # error names curated.toml rather than the merged document.
    probe = {"id": game["id"], "title": game["title"], "keys": game.get("keys", {})}
    for field in ("environment", "winetricks", "arguments", "notes", "links", "umuStore"):
        if field in game:
            probe[field] = game[field]
    if "proton" in game:
        probe["proton"] = game["proton"]
    if "antiCheat" in game:
        probe["antiCheat"] = {k: v for k, v in game["antiCheat"].items() if k != "source"}
    try:
        schema.check_game(probe, builds, where)
    except schema.Refused as error:
        raise CuratedError(str(error)) from None


def load(path: str) -> dict:
    with open(path, "rb") as handle:
        data = tomllib.load(handle)
    _need(set(data) <= {"defaults", "builds", "games"}, "curated.toml: unknown table")
    defaults = data.get("defaults", {})
    _evidence(defaults, "curated defaults")
    for name, build in data.get("builds", {}).items():
        _need(schema.is_build_name(name), f"curated build {name!r}: bad name")
        _need(build.get("status") in schema.BUILD_STATUSES, f"curated build {name}: status")
        _evidence(build, f"curated build {name}")
    builds = data.get("builds", {})
    recommended = defaults.get("recommendedBuild", "")
    _need(recommended == "" or builds.get(recommended, {}).get("status") == "tested",
          "curated defaults.recommendedBuild must be a tested curated build")
    ids = set()
    for game in data.get("games", []):
        _check_game(game, builds)
        _need(game["id"] not in ids, f"curated game {game['id']}: duplicate id")
        ids.add(game["id"])
    return data


def _joined(registry: Registry, game: dict) -> list[GameRecord]:
    keys, match = game.get("keys", {}), game.get("match", {})
    found = []
    candidates = [registry.by_umu(u) for u in (match.get("umuId"), keys.get("umuId")) if u]
    candidates += [registry.by_awacy(match["awacy"])] if "awacy" in match else []
    candidates += [registry.by_steam(a) for a in keys.get("steamAppIds", [])]
    for store, ids in keys.get("storeIds", {}).items():
        candidates += [registry.by_store(store, i) for i in ids]
    for record in candidates:
        if record is not None and record not in found:
            found.append(record)
    return sorted(found, key=lambda r: r.id)


def _overlay(record: GameRecord, game: dict) -> None:
    keys = game.get("keys", {})
    record.umu_id = keys.get("umuId", record.umu_id)
    record.steam.update(keys.get("steamAppIds", []))
    for store, ids in keys.get("storeIds", {}).items():
        record.stores.setdefault(store, set()).update(ids)
    for exe in keys.get("exeNames", []):
        record.add_exe(exe)
    for title in keys.get("titles", []):
        record.add_title(title)
    for field in ("environment", "winetricks", "arguments"):
        if field in game:
            setattr(record, field, list(game[field]))
    record.notes = list(game.get("notes", [])) + [n for n in record.notes
                                                   if n not in game.get("notes", [])]
    record.extend("links", game.get("links", []))
    record.umuStore = game.get("umuStore", record.umuStore)
    proton = game.get("proton", {})
    record.recommended = proton.get("recommended", record.recommended)
    for avoid in proton.get("avoid", []):
        record.avoid[avoid["build"]] = {k: avoid[k] for k in ("build", "reason", "source")}
    if "antiCheat" in game:
        anti = game["antiCheat"]
        record.antiCheat = {"status": anti["status"]}
        if anti.get("notes"):
            record.antiCheat["notes"] = anti["notes"]
    record.curated = True


def _apply_lutris(record: GameRecord, facts: dict[str, list[str]], game: dict) -> None:
    if "winetricks" in facts and "winetricks" not in game:
        record.winetricks = facts["winetricks"]
    if ("environment" in facts or "dlloverrides" in facts) and "environment" not in game:
        record.environment = facts.get("environment", []) + facts.get("dlloverrides", [])


def apply(registry: Registry, data: dict, fetcher: Fetcher, log) -> str | None:
    """Apply curated games; returns the newest Lutris retrieval stamp used."""
    newest = None
    for game in data.get("games", []):
        record = GameRecord(game["id"], game["title"])
        for joined in _joined(registry, game):
            registry.remove(joined)
            record.absorb(joined)
        if "lutris" in game:
            spec = game["lutris"]
            facts, stamp = lutris.fetch_facts(fetcher, spec["slug"], spec["installer"],
                                              set(spec["take"]), log)
            _apply_lutris(record, facts, game)
            # Attribution for the facts taken (ADR-0275 section 7).
            record.extend("links", [f"https://lutris.net/games/{spec['slug']}/"])
            if stamp:
                newest = max(newest or stamp, stamp)
        _overlay(record, game)
        _need(record.id not in registry.records,
              f"curated game {record.id}: id already used by an upstream record")
        registry.add(record)
    return newest
