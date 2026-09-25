# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared paths and helpers for the generator's offline tests."""

from __future__ import annotations

import sys
from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parents[1]
if str(TOOL_DIR) not in sys.path:
    sys.path.insert(0, str(TOOL_DIR))

from qlcompat.fetch import CachedResponse, Fetcher  # noqa: E402

REPO = TOOL_DIR.parents[1]
FIXTURES = Path(__file__).resolve().parent / "fixtures"
SHARED = REPO / "tests" / "apps" / "qindalutris" / "data" / "compat"
SNAPSHOT = REPO / "src" / "apps" / "qindalutris" / "compat" / "compat-db-v1.json"
STAMP = "2026-09-20T10:00:00Z"


def fixture_bytes(name: str) -> bytes:
    return (FIXTURES / name).read_bytes()


def seeded_cache(directory: Path) -> Fetcher:
    """An offline fetcher whose cache holds every fixture source."""
    fetcher = Fetcher(directory, offline=True)
    seeds = {
        "umu-database.csv": "umu-database.csv",
        "areweanticheatyet-games.json": "areweanticheatyet.json",
        "lutris-battlenet.json": "lutris-battlenet.json",
        "winetricks-list-all.txt": "winetricks-list-all.txt",
    }
    for name, source in seeds.items():
        fetcher.store(name, CachedResponse("https://example.org/" + name, 200, STAMP,
                                           fixture_bytes(source)))
    fetcher.store("protondb-397540.json", CachedResponse(
        "https://www.protondb.com/x", 200, "2026-09-21T00:00:00Z",
        b'{"tier": "gold", "total": 5}'))
    fetcher.store("steamdeck-397540.json", CachedResponse(
        "https://store.steampowered.com/x", 200, "2026-09-22T00:00:00Z",
        fixture_bytes("steamdeck-playable.json")))
    fetcher.store("steamdeck-359550.json", CachedResponse(
        "https://store.steampowered.com/y", 200, "2026-09-22T00:00:00Z",
        b'{"success": 1, "results": []}'))
    fetcher.store("protondb-359550.json", CachedResponse(
        "https://www.protondb.com/y", 404, "2026-09-21T00:00:00Z", None))
    return fetcher


def log_sink(lines: list[str]):
    return lines.append
