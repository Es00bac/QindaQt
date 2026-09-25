# SPDX-License-Identifier: GPL-3.0-or-later
"""ProtonDB: the community tier of Steam games under Proton.

Reads https://www.protondb.com/api/v1/reports/summaries/<appid>.json, a JSON
object whose `tier` is one of platinum, gold, silver, bronze, borked or
pending. Only Steam appids already in the database are asked about, in a
fixed priority order (curated games, then AreWeAntiCheatYet's widely played
multiplayer titles, then umu-database games), and at most `limit` of them,
whether the answer comes from the cache or the network, so the output
depends only on the limit and the cache.
Requests are rate-limited by the fetcher; failures leave the tier unset.
"""

from __future__ import annotations

import json

from .fetch import Fetcher
from .model import Registry

URL_TEMPLATE = "https://www.protondb.com/api/v1/reports/summaries/{appid}.json"
SOURCE_URL = "https://www.protondb.com/"
SOURCE_ID = "protondb"
TIERS = frozenset(("platinum", "gold", "silver", "bronze", "borked", "pending",
                   "native"))


def candidates(registry: Registry) -> list[tuple[str, object]]:
    ranked = []
    for record in registry.records.values():
        if not record.steam:
            continue
        appid = min(record.steam, key=lambda a: (len(a), a))
        rank = 0 if record.curated else (1 if record.awacy_slug else 2)
        ranked.append(((rank, len(appid), appid), appid, record))
    ranked.sort(key=lambda item: item[0])
    return [(appid, record) for _, appid, record in ranked]


def apply(registry: Registry, fetcher: Fetcher, limit: int, log) -> str | None:
    """Set tiers; returns the newest retrieval stamp used, or None."""
    newest = None
    failures = 0
    for appid, record in candidates(registry)[:max(0, limit)]:
        response = fetcher.get(URL_TEMPLATE.format(appid=appid), f"protondb-{appid}.json")
        if response is None or not response.ok:
            failures += 1 if response is None else 0
            continue
        try:
            tier = json.loads(response.body.decode("utf-8")).get("tier")
        except (ValueError, AttributeError):
            continue
        if tier not in TIERS:
            continue
        record.tier = tier
        record.extend("links", [f"https://www.protondb.com/app/{appid}"])
        newest = max(newest or response.retrieved, response.retrieved)
    if failures:
        log(f"protondb: {failures} summaries unavailable (offline or network error)")
    return newest
