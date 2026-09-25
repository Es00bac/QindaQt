# SPDX-License-Identifier: GPL-3.0-or-later
"""AreWeAntiCheatYet: anti-cheat status of multiplayer games under Proton.

Upstream: https://github.com/AreWeAntiCheatYet/AreWeAntiCheatYet games.json,
a list of objects with name, slug, status (Supported | Running | Broken |
Denied | Planned), anticheats[], notes[[text, url]], native, storeIds
{steam: "<appid>", epic: {namespace, slug}}. The Epic ids are catalog
namespace/slug pairs, not the launcher app names umu and legendary use, so
they are not imported as egs keys.
"""

from __future__ import annotations

import json
import re

from . import schema
from .model import GameRecord, Index, Registry, clean_text, game_id_from

URL = ("https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/"
       "master/games.json")
SOURCE_ID = "areweanticheatyet"
MAX_ENTRIES = 50000

# AreWeAntiCheatYet's own vocabulary, mapped one-to-one onto compat-db-v1.
STATUS = {
    "supported": "supported",  # anti-cheat officially enabled for Proton/Linux
    "running": "running",      # works, but not officially supported
    "broken": "broken",        # anti-cheat blocks Proton
    "denied": "denied",        # developer refuses Proton/Linux support
    "planned": "planned",      # support announced
}


def _entry_notes(entry: dict) -> tuple[str, list[str]]:
    parts = []
    cheats = [clean_text(a, 64) for a in entry.get("anticheats") or [] if isinstance(a, str)]
    cheats = [c for c in cheats if c]
    if cheats:
        parts.append("Anti-cheat: " + ", ".join(cheats) + ".")
    if entry.get("native") is True:
        parts.append("Has a native Linux version.")
    links = []
    for note in entry.get("notes") or []:
        if isinstance(note, list) and note and isinstance(note[0], str):
            text = clean_text(note[0], 300)
            if text:
                parts.append(text if text.endswith(".") else text + ".")
            if len(note) > 1 and schema.is_https_url(note[1]):
                links.append(note[1])
    reference = entry.get("reference")
    if isinstance(reference, str) and schema.is_https_url(reference):
        links.append(reference)
    return clean_text(" ".join(parts)), links


def apply(registry: Registry, data: bytes, log) -> int:
    entries = json.loads(data.decode("utf-8"))
    if not isinstance(entries, list) or len(entries) > MAX_ENTRIES:
        raise ValueError("AreWeAntiCheatYet games.json is not a bounded list")
    index = Index(registry)
    applied = 0
    ordered = sorted((e for e in entries if isinstance(e, dict)),
                     key=lambda e: str(e.get("slug", "")))
    for entry in ordered:
        name = clean_text(entry.get("name"), schema.MAX_TITLE)
        slug = entry.get("slug")
        status = STATUS.get(str(entry.get("status", "")).lower(), "unknown")
        if not name or not isinstance(slug, str) or not re.fullmatch(r"[a-z0-9-]{1,100}", slug):
            continue
        store_ids = entry.get("storeIds") if isinstance(entry.get("storeIds"), dict) else {}
        steam = store_ids.get("steam")
        steam = steam if schema.is_steam_app_id(steam) else None
        record = index.steam.get(steam) if steam else None
        if record is None and not steam:
            record = index.unique_title(name)
        if record is not None and record.antiCheat is not None:
            log(f"areweanticheatyet: {slug} shares a game with another entry; skipped")
            continue
        if record is None:
            record = registry.add(GameRecord(game_id_from("awacy:" + slug), name))
        notes, links = _entry_notes(entry)
        record.antiCheat = {"status": status, "notes": notes} if notes else {"status": status}
        record.awacy_slug = slug
        if steam:
            record.steam.add(steam)
        if name != record.title:
            record.add_title(name)
        record.extend("links", [f"https://areweanticheatyet.com/game/{slug}"] + links)
        index.note(record)
        applied += 1
    return applied
