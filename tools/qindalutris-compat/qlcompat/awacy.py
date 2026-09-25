# SPDX-License-Identifier: GPL-3.0-or-later
"""AreWeAntiCheatYet: anti-cheat status of multiplayer games under Proton.

Upstream: https://github.com/AreWeAntiCheatYet/AreWeAntiCheatYet games.json,
a list of objects with name, slug, status (Supported | Running | Broken |
Denied | Planned), anticheats[], notes[[text, url]], native, storeIds
{steam: "<appid>", epic: {namespace, slug}}. The Epic ids are catalog
namespace/slug pairs, not the launcher app names umu and legendary use, so
they are not imported as egs keys.

Several entries sometimes claim one Steam appid (a renamed game kept under
both names, or a wrong id). The appid is then given only to the entry whose
name is Steam's own name for it; if none or several match, no entry keys
that appid. Every such decision is logged.
"""

from __future__ import annotations

import hashlib
import json
import re
import unicodedata
import urllib.parse

from . import schema
from .model import GameRecord, Index, Registry, clean_text, game_id_from, normalize_title

URL = ("https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/"
       "master/games.json")
SOURCE_ID = "areweanticheatyet"
MAX_ENTRIES = 50000
MAX_SLUG = 200

# AreWeAntiCheatYet's own vocabulary, mapped one-to-one onto compat-db-v1.
STATUS = {
    "supported": "supported",  # anti-cheat officially enabled for Proton/Linux
    "running": "running",      # works, but not officially supported
    "broken": "broken",        # anti-cheat blocks Proton
    "denied": "denied",        # developer refuses Proton/Linux support
    "planned": "planned",      # support announced
}


def slug_id(slug: str) -> str | None:
    """A stable, schema-safe game id for any slug, including encoded ones.

    "s%26box" and "light⚡️nite" are percent-decoded, folded to ASCII and
    given a short hash of the original slug when folding lost characters,
    so two different slugs never collapse into one id.
    """
    if not isinstance(slug, str) or not slug or len(slug) > MAX_SLUG:
        return None
    decoded = urllib.parse.unquote(slug)
    folded = unicodedata.normalize("NFKD", decoded).encode("ascii", "ignore").decode()
    base = re.sub(r"[^a-z0-9]+", "-", folded.lower()).strip("-")
    lossless = base == decoded
    if not lossless:
        base = (base[:100] + "-" if base else "") + hashlib.sha256(slug.encode()).hexdigest()[:8]
    return game_id_from("awacy:" + base)


def page_url(slug: str) -> str:
    return "https://areweanticheatyet.com/game/" + urllib.parse.quote(slug, safe="%-._~")


def _sentence(text: str) -> str:
    return text if text.endswith((".", "!", "?")) else text + "."


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
                parts.append(_sentence(text))
            if len(note) > 1 and schema.is_https_url(note[1]):
                links.append(note[1])
    reference = entry.get("reference")
    if isinstance(reference, str) and schema.is_https_url(reference):
        links.append(reference)
    return clean_text(" ".join(parts)), links


def _steam_of(entry: dict) -> str | None:
    ids = entry.get("storeIds") if isinstance(entry.get("storeIds"), dict) else {}
    steam = ids.get("steam")
    return steam if schema.is_steam_app_id(steam) else None


def settle_shared_appids(entries: list[dict], name_of, log) -> dict[str, str]:
    """{slug: appid} for the entries allowed to key their Steam appid."""
    claims: dict[str, list[dict]] = {}
    for entry in entries:
        appid = _steam_of(entry)
        if appid:
            claims.setdefault(appid, []).append(entry)
    keyed = {}
    for appid, group in sorted(claims.items()):
        if len(group) == 1:
            keyed[group[0]["slug"]] = appid
            continue
        real = name_of(appid) if name_of else None
        winners = [e for e in group if real and normalize_title(e["name"]) == normalize_title(real)]
        slugs = ", ".join(sorted(e["slug"] for e in group))
        if len(winners) == 1:
            keyed[winners[0]["slug"]] = appid
            log(f"areweanticheatyet: Steam appid {appid} ({real!r}) claimed by {slugs}; "
                f"kept {winners[0]['slug']}, others withheld from the appid")
        else:
            log(f"areweanticheatyet: Steam appid {appid} claimed by {slugs}; Steam name "
                f"{real!r} matches {len(winners)} of them, so all are withheld from it")
    return keyed


def apply(registry: Registry, data: bytes, log, name_of=None) -> int:
    entries = json.loads(data.decode("utf-8"))
    if not isinstance(entries, list) or len(entries) > MAX_ENTRIES:
        raise ValueError("AreWeAntiCheatYet games.json is not a bounded list")
    usable = []
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        name = clean_text(entry.get("name"), schema.MAX_TITLE)
        if not name or slug_id(entry.get("slug")) is None:
            log(f"areweanticheatyet: entry {str(entry.get('slug'))[:60]!r} has no usable "
                "name or slug; skipped")
            continue
        usable.append(dict(entry, name=name))
    usable.sort(key=lambda e: e["slug"])
    keyed = settle_shared_appids(usable, name_of, log)
    withheld = {_steam_of(e) for e in usable if _steam_of(e) and e["slug"] not in keyed}
    index = Index(registry)
    applied = 0
    for entry in usable:
        slug, name = entry["slug"], entry["name"]
        steam = keyed.get(slug)
        record = index.steam.get(steam) if steam else None
        if record is None and not steam:
            record = index.unique_title(name)
            # A withheld claimant must not reach its appid through a title.
            if record is not None and record.steam & withheld:
                record = None
        if record is not None and record.antiCheat is not None:
            log(f"areweanticheatyet: {slug} names the same game as {record.awacy_slug}; skipped")
            continue
        if record is None:
            record = registry.add(GameRecord(slug_id(slug), name))
        notes, links = _entry_notes(entry)
        status = STATUS.get(str(entry.get("status", "")).lower(), "unknown")
        record.antiCheat = {"status": status, "notes": notes} if notes else {"status": status}
        record.awacy_slug = slug
        if steam:
            record.steam.add(steam)
        if name != record.title:
            record.add_title(name)
        record.extend("links", [page_url(slug)] + links)
        index.note(record)
        applied += 1
    return applied
