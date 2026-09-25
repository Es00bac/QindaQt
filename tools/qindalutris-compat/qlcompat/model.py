# SPDX-License-Identifier: GPL-3.0-or-later
"""The generator's in-memory game records and their merge rules.

Upstream sources (umu-database, AreWeAntiCheatYet, then ProtonDB and the
Steam Deck report) fill facts; curated games absorb the records they name and
their own fields, including Lutris-derived ones, override. A later source
overwrites a scalar fact it owns and extends a key set. Strong keys (umu id, store id, Steam
appid) must end up naming exactly one record, as compat-db-v1 requires.
"""

from __future__ import annotations

import re
import unicodedata

from . import schema


def normalize_title(title: str) -> str:
    """Approximates the C++ normalizedTitleForMatch, for generator joins only.

    The runtime normalizes both sides itself, so this never has to be exact;
    it only decides which upstream rows describe the same game.
    """
    out, pending = [], False
    for ch in title:
        if ch.isalnum():
            if pending and out:
                out.append(" ")
            pending = False
            out.append(ch.casefold())
        else:
            pending = True
    return "".join(out)[:schema.MAX_TITLE]


def clean_text(text, limit: int = schema.MAX_TEXT) -> str:
    """Upstream text made schema-safe: controls become spaces, bounded."""
    if not isinstance(text, str):
        return ""
    text = "".join(" " if unicodedata.category(ch) == "Cc" else ch for ch in text)
    text = re.sub(r"\s+", " ", text).strip()
    while schema.u16len(text) > limit:
        text = text[:-1]
    return text


def game_id_from(text: str) -> str:
    """A schema-valid, stable game id derived from an upstream identifier."""
    text = re.sub(r"[^a-z0-9._:-]+", "-", text.lower()).strip("-._:")
    return text[:128] or "game"


class GameRecord:
    """One game as it is being assembled from several sources."""

    SCALARS = ("recommended", "antiCheat", "tier", "deck", "umuStore")
    LISTS = ("environment", "winetricks", "arguments", "notes", "links")

    def __init__(self, game_id: str, title: str):
        self.id = game_id
        self.title = title
        self.umu_id: str | None = None
        self.steam: set[str] = set()
        self.stores: dict[str, set[str]] = {}
        self.exe: dict[str, str] = {}  # casefolded -> spelling
        self.titles: set[str] = set()
        self.recommended: str | None = None
        self.avoid: dict[str, dict] = {}
        self.antiCheat: dict | None = None
        self.tier: str | None = None
        self.deck: dict | None = None  # {"category", "notes"?}
        self.umuStore: str | None = None
        self.environment: list[str] = []
        self.winetricks: list[str] = []
        self.arguments: list[str] = []
        self.notes: list[str] = []
        self.links: list[str] = []
        self.curated = False
        self.awacy_slug: str | None = None

    def add_exe(self, name: str) -> None:
        if schema.is_exe_name(name):
            self.exe.setdefault(name.casefold(), name)

    def add_title(self, title: str) -> None:
        title = clean_text(title, schema.MAX_TITLE)
        if title:
            self.titles.add(title)

    def extend(self, field: str, values) -> None:
        current = getattr(self, field)
        for value in values:
            if value and value not in current:
                current.append(value)

    def absorb(self, other: "GameRecord") -> None:
        """Merge another record's facts in; ours win on scalar conflicts."""
        if self.umu_id and other.umu_id and self.umu_id != other.umu_id:
            raise ValueError(f"cannot merge {self.id} and {other.id}: two umu ids")
        self.umu_id = self.umu_id or other.umu_id
        self.steam |= other.steam
        for store, ids in other.stores.items():
            self.stores.setdefault(store, set()).update(ids)
        for key, name in other.exe.items():
            self.exe.setdefault(key, name)
        self.titles |= other.titles | {other.title}
        for build, entry in other.avoid.items():
            self.avoid.setdefault(build, entry)
        for field in self.SCALARS:
            if getattr(self, field) is None:
                setattr(self, field, getattr(other, field))
        for field in self.LISTS:
            self.extend(field, getattr(other, field))
        self.awacy_slug = self.awacy_slug or other.awacy_slug
        self.curated = self.curated or other.curated


class Registry:
    """All records, with lookups by the keys sources join on."""

    def __init__(self):
        self.records: dict[str, GameRecord] = {}

    def add(self, record: GameRecord) -> GameRecord:
        base, n = record.id, 2
        while record.id in self.records:
            record.id = f"{base}-{n}"[:128]
            n += 1
        self.records[record.id] = record
        return record

    def remove(self, record: GameRecord) -> None:
        del self.records[record.id]

    def by_steam(self, appid: str) -> GameRecord | None:
        return next((r for r in self.records.values() if appid in r.steam), None)

    def by_umu(self, umu_id: str) -> GameRecord | None:
        return next((r for r in self.records.values() if r.umu_id == umu_id), None)

    def by_store(self, store: str, store_id: str) -> GameRecord | None:
        return next((r for r in self.records.values()
                     if store_id in r.stores.get(store, ())), None)

    def by_awacy(self, slug: str) -> GameRecord | None:
        return next((r for r in self.records.values() if r.awacy_slug == slug), None)

    def unique_by_title(self, title: str) -> GameRecord | None:
        wanted = normalize_title(title)
        if not wanted:
            return None
        hits = [r for r in self.records.values()
                if wanted == normalize_title(r.title)
                or any(wanted == normalize_title(t) for t in r.titles)]
        return hits[0] if len(hits) == 1 else None

    def sorted(self) -> list[GameRecord]:
        return [self.records[key] for key in sorted(self.records)]


class Index:
    """Fast lookups over a registry snapshot (rebuilt after bulk adds)."""

    def __init__(self, registry: Registry):
        self.steam: dict[str, GameRecord] = {}
        self.titles: dict[str, list[GameRecord]] = {}
        for record in registry.records.values():
            self.note(record)

    def note(self, record: GameRecord) -> None:
        for appid in record.steam:
            self.steam.setdefault(appid, record)
        for title in {record.title} | record.titles:
            bucket = self.titles.setdefault(normalize_title(title), [])
            if record not in bucket:
                bucket.append(record)

    def unique_title(self, title: str) -> GameRecord | None:
        hits = self.titles.get(normalize_title(title), [])
        return hits[0] if len(hits) == 1 else None
