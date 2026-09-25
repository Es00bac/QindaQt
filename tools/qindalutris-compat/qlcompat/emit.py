# SPDX-License-Identifier: GPL-3.0-or-later
"""Turns the registry into a deterministic, schema-valid compat-db-v1 text."""

from __future__ import annotations

import json

from . import schema
from .model import GameRecord, Registry


def _steam_order(appid: str) -> tuple[int, str]:
    return (len(appid), appid)


def dedupe_strong_keys(registry: Registry, log) -> None:
    """Keep each strong key on one record: curated first, then by id.

    Upstream data occasionally lists one Steam appid or store id under two
    records; the schema refuses that, so the lower-precedence record loses
    the key (and keeps its other keys and facts).
    """
    owners: dict[tuple[str, str], str] = {}
    ordered = sorted(registry.records.values(), key=lambda r: (not r.curated, r.id))
    for record in ordered:
        if record.umu_id:
            if owners.setdefault(("umu", record.umu_id), record.id) != record.id:
                log(f"emit: {record.id} loses umu id {record.umu_id}")
                record.umu_id = None
        for appid in sorted(record.steam):
            if owners.setdefault(("steam", appid), record.id) != record.id:
                log(f"emit: {record.id} loses Steam appid {appid}")
                record.steam.discard(appid)
        for store, ids in record.stores.items():
            for store_id in sorted(ids):
                if owners.setdefault((store, store_id), record.id) != record.id:
                    log(f"emit: {record.id} loses {store} id {store_id}")
                    ids.discard(store_id)


def _bounded(values, limit: int, accept) -> list[str]:
    out = []
    for value in values:
        if accept(value) and value not in out:
            out.append(value)
        if len(out) == limit:
            break
    return out


def game_to_json(record: GameRecord) -> dict:
    keys: dict = {}
    if record.umu_id:
        keys["umuId"] = record.umu_id
    steam = _bounded(sorted(record.steam, key=_steam_order), schema.MAX_LIST,
                     schema.is_steam_app_id)
    if steam:
        keys["steamAppIds"] = steam
    stores = {store: _bounded(sorted(ids), schema.MAX_LIST, schema.is_store_id)
              for store, ids in sorted(record.stores.items()) if ids}
    stores = {store: ids for store, ids in stores.items() if ids}
    if stores:
        keys["storeIds"] = stores
    exe = _bounded([record.exe[k] for k in sorted(record.exe)], schema.MAX_LIST,
                   schema.is_exe_name)
    if exe:
        keys["exeNames"] = exe
    titles = _bounded(sorted(t for t in record.titles if t != record.title),
                      schema.MAX_LIST, lambda t: schema.is_text(t, schema.MAX_TITLE))
    if titles:
        keys["titles"] = titles
    game = {"id": record.id, "title": record.title, "keys": keys}
    proton: dict = {}
    if record.recommended:
        proton["recommended"] = record.recommended
    if record.avoid:
        proton["avoid"] = [record.avoid[b] for b in sorted(record.avoid)][:schema.MAX_LIST]
    if proton:
        game["proton"] = proton
    lists = (("environment", schema.MAX_ENV, schema.is_env_assignment),
             ("winetricks", schema.MAX_LIST, schema.is_winetricks_verb),
             ("arguments", schema.MAX_LIST,
              lambda t: schema.is_text(t, schema.MAX_ARGUMENT)),
             ("notes", schema.MAX_LIST, lambda t: schema.is_text(t, schema.MAX_TEXT)))
    for field, limit, accept in lists:
        values = _bounded(getattr(record, field), limit, accept)
        if values:
            game[field] = values
    if record.antiCheat:
        game["antiCheat"] = dict(record.antiCheat)
    if record.tier:
        game["protondbTier"] = record.tier
    if record.deck:
        game["steamDeck"] = dict(record.deck)
    if record.umuStore:
        game["umuStore"] = record.umuStore
    links = _bounded(sorted(record.links), schema.MAX_LIST, schema.is_https_url)
    if links:
        game["links"] = links
    return game


def build_document(registry: Registry, generated: str, sources: list[dict],
                   defaults: dict, builds: dict, log) -> dict:
    dedupe_strong_keys(registry, log)
    return {
        "schema": schema.SCHEMA_NAME,
        "version": schema.SCHEMA_VERSION,
        "generated": generated,
        "sources": sorted(sources, key=lambda s: s["id"]),
        "defaults": {"recommendedBuild": defaults.get("recommendedBuild", "")},
        "builds": {name: {"status": b["status"], "notes": b.get("notes", "")}
                   for name, b in sorted(builds.items())},
        "games": [game_to_json(r) for r in registry.sorted()],
    }


def dumps(document: dict) -> str:
    """Stable text: header pretty-printed, one game per line for small diffs."""
    def one(value) -> str:
        return json.dumps(value, ensure_ascii=False, sort_keys=True,
                          separators=(",", ":"))
    lines = ["{"]
    for key in ("schema", "version", "generated", "sources", "defaults", "builds"):
        text = json.dumps(document[key], ensure_ascii=False, sort_keys=True, indent=1)
        lines.append(f' "{key}": ' + text.replace("\n", "\n ") + ",")
    games = document["games"]
    lines.append(' "games": [')
    for position, game in enumerate(games):
        lines.append("  " + one(game) + ("," if position + 1 < len(games) else ""))
    lines.append(" ]")
    lines.append("}")
    return "\n".join(lines) + "\n"
