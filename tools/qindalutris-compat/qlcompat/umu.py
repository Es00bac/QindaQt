# SPDX-License-Identifier: GPL-3.0-or-later
"""umu-database: umu ids, store codenames, titles, notes, executable names.

Upstream: https://github.com/Open-Wine-Components/umu-database (CSV with
columns TITLE, STORE, CODENAME, UMU_ID, COMMON ACRONYM (Optional),
NOTE (Optional), EXE_STRINGS (Optional)). Its README fixes two conventions
this module relies on: STORE is one of umu's store names or "none", and an
all-digit umu id suffix is the game's Steam appid ("umu-<steam appid>").
"""

from __future__ import annotations

import csv
import io
import re

from . import schema
from .model import GameRecord, Registry, clean_text, game_id_from

URL = ("https://raw.githubusercontent.com/Open-Wine-Components/umu-database/"
       "main/umu-database.csv")
SOURCE_ID = "umu-database"
MAX_ROWS = 200000
COLUMNS = ("TITLE", "STORE", "CODENAME", "UMU_ID", "COMMON ACRONYM", "NOTE",
           "EXE_STRINGS")

# Basenames too generic to identify one game: matching "Launcher.exe" would
# attach one game's advice to every other game that ships a Launcher.exe.
GENERIC_EXE = frozenset(name.casefold() for name in (
    "launcher.exe", "uplaylaunch.exe", "game.exe", "start.exe", "play.exe",
    "setup.exe", "install.exe", "unins000.exe", "main.exe", "client.exe"))


def _column_map(header: list[str]) -> dict[str, str]:
    mapping = {}
    for wanted in COLUMNS:
        match = next((h for h in header if h.strip().upper().startswith(wanted)), None)
        if match is None:
            raise ValueError(f"umu-database CSV lacks column {wanted}")
        mapping[wanted] = match
    return mapping


def _steam_appid(umu_id: str, codenames: set[str]) -> str | None:
    """The Steam appid an umu id encodes, when it plausibly is one.

    The README allows a non-Steam game to use its GOG product id as the umu
    suffix (umu-1445591825); such a suffix equals one of the game's own
    codenames and GOG ids are ten digits, so neither case is a Steam appid.
    """
    suffix = umu_id[4:]
    if not re.fullmatch(r"[1-9][0-9]{0,8}", suffix) or suffix in codenames:
        return None
    return suffix


def _pick_title(titles: list[str]) -> str:
    counts: dict[str, int] = {}
    for title in titles:
        counts[title] = counts.get(title, 0) + 1
    return sorted(counts, key=lambda t: (-counts[t], len(t), t))[0]


def parse_rows(data: bytes) -> list[dict[str, str]]:
    text = data.decode("utf-8-sig")
    reader = csv.reader(io.StringIO(text))
    header = next(reader)
    columns = _column_map(header)
    positions = {key: header.index(name) for key, name in columns.items()}
    rows = []
    for number, row in enumerate(reader):
        if number >= MAX_ROWS:
            raise ValueError("umu-database CSV has too many rows")
        if not any(cell.strip() for cell in row):
            continue
        rows.append({key: (row[pos].strip() if pos < len(row) else "")
                     for key, pos in positions.items()})
    return rows


def apply(registry: Registry, data: bytes, log) -> int:
    """Add one record per umu id. Returns the number of records added."""
    groups: dict[str, list[dict[str, str]]] = {}
    skipped = 0
    for row in parse_rows(data):
        if not schema.is_umu_id(row["UMU_ID"]) or not clean_text(row["TITLE"]):
            skipped += 1
            continue
        groups.setdefault(row["UMU_ID"], []).append(row)
    for umu_id in sorted(groups):
        rows = groups[umu_id]
        titles = [clean_text(r["TITLE"], schema.MAX_TITLE) for r in rows]
        record = GameRecord(game_id_from(umu_id), _pick_title(titles))
        record.umu_id = umu_id
        for title in titles:
            if title != record.title:
                record.add_title(title)
        codenames = {r["CODENAME"] for r in rows}
        store_names = {r["STORE"].lower() for r in rows}
        for row in rows:
            store, codename = row["STORE"].lower(), row["CODENAME"]
            if store == "steam" and schema.is_steam_app_id(codename):
                record.steam.add(codename)
            elif store in schema.STORES and codename.lower() != "none" \
                    and schema.is_store_id(codename):
                record.stores.setdefault(store, set()).add(codename)
            exe = row["EXE_STRINGS"].replace("\\", "/").rsplit("/", 1)[-1].strip()
            if exe and exe.casefold() not in GENERIC_EXE:
                record.add_exe(exe)
            note = clean_text(row["NOTE"])
            if note:
                prefix = "" if store == "none" else f"{store}: "
                record.extend("notes", [clean_text(prefix + note)])
        appid = _steam_appid(umu_id, codenames)
        if appid:
            record.steam.add(appid)
        if len(store_names) == 1 and next(iter(store_names)) in schema.UMU_STORES:
            record.umuStore = next(iter(store_names))
        registry.add(record)
    if skipped:
        log(f"umu-database: skipped {skipped} rows without a valid umu id or title")
    return len(groups)
