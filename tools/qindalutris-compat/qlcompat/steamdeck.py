# SPDX-License-Identifier: GPL-3.0-or-later
"""Valve's Steam Deck compatibility rating ("Deck Verified").

Reads https://store.steampowered.com/saleaction/ajaxgetdeckappcompatibility
report?nAppID=<appid>&l=english, the undocumented endpoint the Steam store
page uses: {success: 1, results: {resolved_category: 3|2|1|0,
resolved_items: [{display_type, loc_token}]}}; `results` is an empty list
for an appid Valve has not rated. Categories: 3 Verified, 2 Playable,
1 Unsupported, 0 Unknown. Observed display types: 4 is a passed check, 3 a
limitation, 2 a blocker, 1 advice; every item except passed checks becomes
a readable note. Being undocumented, it is treated like ProtonDB: the same
appids in the same order under the same limit, cached, rate-limited, and
skipped on any error.
"""

from __future__ import annotations

import json
import re

from . import protondb
from .fetch import Fetcher
from .model import Registry, clean_text

URL_TEMPLATE = ("https://store.steampowered.com/saleaction/"
                "ajaxgetdeckappcompatibilityreport?nAppID={appid}&l=english")
SOURCE_URL = "https://store.steampowered.com/"
SOURCE_ID = "steam-deck"
CATEGORIES = {3: "verified", 2: "playable", 1: "unsupported"}
PASSED = 4


def readable(token: str) -> str:
    """#SteamDeckVerified_TestResult_InterfaceTextIsNotLegible ->
    "Interface text is not legible"."""
    name = token.rsplit("_", 1)[-1] if "_" in token else token.lstrip("#")
    words = re.findall(r"[A-Z]+(?=[A-Z][a-z]|\b|[0-9])|[A-Z]?[a-z]+|[0-9]+|[A-Z]+", name)
    if not words:
        return ""
    text = " ".join(w if w.isupper() and len(w) > 1 else w.lower() for w in words)
    return clean_text(text[:1].upper() + text[1:], 300)


def parse(data: bytes) -> dict | None:
    """The steamDeck object for one report, or None when Valve has no rating."""
    document = json.loads(data.decode("utf-8"))
    results = document.get("results") if isinstance(document, dict) else None
    if document.get("success") != 1 or not isinstance(results, dict):
        return None
    category = CATEGORIES.get(results.get("resolved_category"))
    if category is None:
        return None
    notes = []
    for item in results.get("resolved_items") or []:
        if not isinstance(item, dict) or item.get("display_type") == PASSED:
            continue
        note = readable(str(item.get("loc_token", "")))
        if note and note not in notes:
            notes.append(note)
    return {"category": category, "notes": notes} if notes else {"category": category}


def apply(registry: Registry, fetcher: Fetcher, limit: int, log) -> str | None:
    """Set Steam Deck ratings; returns the newest retrieval stamp used."""
    newest = None
    for appid, record in protondb.candidates(registry)[:max(0, limit)]:
        response = fetcher.get(URL_TEMPLATE.format(appid=appid), f"steamdeck-{appid}.json")
        if response is None or not response.ok:
            continue
        try:
            deck = parse(response.body)
        except (ValueError, AttributeError):
            log(f"steam-deck: unreadable report for {appid}")
            continue
        if deck is None:
            continue
        record.deck = deck
        record.extend("links", [f"https://store.steampowered.com/app/{appid}/"])
        newest = max(newest or response.retrieved, response.retrieved)
    return newest
