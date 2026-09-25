#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the QindaLutris compatibility database (compat-db-v1).

    generate.py --out compat-db-v1.json [--cache-dir DIR] [--offline]
                [--generated 2026-09-25T00:00:00Z] [--protondb-limit 300]

Sources, lowest precedence first: umu-database, AreWeAntiCheatYet, ProtonDB
and Valve's Steam Deck ratings (both bounded by --protondb-limit), Lutris
installer scripts (for curated games that name one), curated.toml.
Every fetch is cached in --cache-dir with its retrieval time; --offline uses
only the cache. The output is validated with the same rules as the C++
parser before it is written, and is byte-identical for the same cache,
curated file and --generated stamp.
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from qlcompat import (awacy, curated, emit, protondb, schema, steamdeck, umu,  # noqa: E402
                      winetricks)
from qlcompat.fetch import Fetcher, utc_stamp  # noqa: E402
from qlcompat.model import Registry  # noqa: E402

HERE = Path(__file__).resolve().parent


def _log(message: str) -> None:
    print(message, file=sys.stderr)


def _default_cache() -> Path:
    base = os.environ.get("XDG_CACHE_HOME") or os.path.join(Path.home(), ".cache")
    return Path(base) / "qindalutris-compat"


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True, help="output JSON file")
    parser.add_argument("--cache-dir", type=Path, default=_default_cache())
    parser.add_argument("--offline", action="store_true",
                        help="use cached copies only; never touch the network")
    parser.add_argument("--refresh", action="store_true",
                        help="refetch sources even when a cached copy exists")
    parser.add_argument("--generated", help="the document's UTC stamp "
                        "(YYYY-MM-DDTHH:MM:SSZ); default now")
    parser.add_argument("--protondb-limit", type=int, default=300,
                        help="at most this many Steam appids asked of ProtonDB and "
                        "of the Steam Deck report (default 300)")
    parser.add_argument("--curated", default=str(HERE / "curated.toml"))
    parser.add_argument("--winetricks", default="winetricks",
                        help="winetricks binary used for `list-all`")
    parser.add_argument("--winetricks-list", help="a saved `winetricks list-all` "
                        "output to validate verbs against instead")
    args = parser.parse_args(argv)
    if args.generated is None:
        args.generated = utc_stamp()
    if schema.parse_timestamp(args.generated) is None:
        parser.error("--generated must look like 2026-09-25T00:00:00Z")
    return args


def _source(sources: list, source_id: str, url: str, retrieved: str | None) -> None:
    if retrieved:
        sources.append({"id": source_id, "url": url, "retrieved": retrieved})


def generate(args, fetcher: Fetcher, log=_log) -> dict:
    curated_data = curated.load(args.curated)
    registry = Registry()
    sources: list[dict] = []
    response = fetcher.get(umu.URL, "umu-database.csv")
    if response is not None and response.ok:
        log(f"umu-database: {umu.apply(registry, response.body, log)} umu ids")
        _source(sources, umu.SOURCE_ID, umu.URL, response.retrieved)
    else:
        log("umu-database: unavailable; continuing without it")
    response = fetcher.get(awacy.URL, "areweanticheatyet-games.json")
    if response is not None and response.ok:
        log(f"areweanticheatyet: {awacy.apply(registry, response.body, log)} entries")
        _source(sources, awacy.SOURCE_ID, awacy.URL, response.retrieved)
    else:
        log("areweanticheatyet: unavailable; continuing without it")
    stamp = curated.apply(registry, curated_data, fetcher, log)
    _source(sources, "lutris", curated.lutris.SOURCE_URL, stamp)
    stamp = protondb.apply(registry, fetcher, args.protondb_limit, log)
    _source(sources, protondb.SOURCE_ID, protondb.SOURCE_URL, stamp)
    stamp = steamdeck.apply(registry, fetcher, args.protondb_limit, log)
    _source(sources, steamdeck.SOURCE_ID, steamdeck.SOURCE_URL, stamp)
    verbs, stamp = winetricks.load_verbs(fetcher, args.winetricks, args.winetricks_list, log)
    curated_verbs = {g["id"]: g.get("winetricks", []) for g in curated_data.get("games", [])}
    winetricks.filter_records(registry, verbs, curated_verbs, log)
    _source(sources, winetricks.SOURCE_ID, winetricks.SOURCE_URL, stamp)
    document = emit.build_document(registry, args.generated, sources,
                                   curated_data.get("defaults", {}),
                                   curated_data.get("builds", {}), log)
    schema.validate_document(document)
    return document


def main(argv=None) -> int:
    args = parse_args(argv)
    fetcher = Fetcher(args.cache_dir, offline=args.offline, refresh=args.refresh)
    try:
        document = generate(args, fetcher)
    except (curated.CuratedError, schema.Refused, ValueError, OSError) as error:
        _log(f"generate: {error}")
        return 1
    text = emit.dumps(document)
    schema.load_bytes(text.encode("utf-8"))  # the bytes we write must validate too
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    temporary = out.with_name(out.name + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(out)
    _log(f"wrote {out}: {len(document['games'])} games, {len(text.encode())} bytes, "
         f"{fetcher.network_requests} network requests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
