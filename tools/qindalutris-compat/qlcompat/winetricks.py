# SPDX-License-Identifier: GPL-3.0-or-later
"""winetricks: the authoritative verb list every imported verb is checked on.

`winetricks list-all` prints category banners ("===== dlls =====") and one
verb per line, the verb being the first whitespace-separated token. The list
is cached like any fetch so --offline runs validate against the same list.
protontricks wraps winetricks and takes the same verbs, which is how Steam
users apply them; QindaLutris runs them as `umu-run winetricks <verbs>`.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile

from . import schema
from .fetch import CachedResponse, Fetcher, utc_stamp

SOURCE_ID = "winetricks"
SOURCE_URL = "https://github.com/Winetricks/winetricks"
CACHE_NAME = "winetricks-list-all.txt"


def parse_list(text: str) -> set[str]:
    verbs = set()
    for line in text.splitlines():
        if not line.strip() or line.startswith("=====") or line.startswith("Executing"):
            continue
        verb = line.split()[0]
        if schema.is_winetricks_verb(verb):
            verbs.add(verb)
    return verbs


def _run_list_all(binary: str) -> str | None:
    path = shutil.which(binary)
    if path is None:
        return None
    with tempfile.TemporaryDirectory() as scratch:
        # A throwaway, never-created prefix: listing verbs must not touch
        # the user's ~/.wine.
        env = dict(os.environ, WINEPREFIX=os.path.join(scratch, "unused-prefix"),
                   WINEDEBUG="-all")
        try:
            done = subprocess.run([path, "list-all"], env=env, capture_output=True,
                                  text=True, timeout=120, check=False)
        except (OSError, subprocess.TimeoutExpired):
            return None
    return done.stdout if done.returncode == 0 and done.stdout else None


def load_verbs(fetcher: Fetcher, binary: str, list_file: str | None,
               log) -> tuple[set[str] | None, str | None]:
    """The verb set and when it was taken; (None, None) if unavailable."""
    if list_file:
        with open(list_file, encoding="utf-8") as handle:
            return parse_list(handle.read()), None
    cached = fetcher.cached(CACHE_NAME)
    if not fetcher.offline and (cached is None or fetcher.refresh):
        text = _run_list_all(binary)
        if text is not None:
            cached = CachedResponse(SOURCE_URL, 200, utc_stamp(), text.encode("utf-8"))
            fetcher.store(CACHE_NAME, cached)
    if cached is None or not cached.ok:
        log("winetricks: no verb list (winetricks missing and nothing cached); "
            "verbs are checked for charset only")
        return None, None
    return parse_list(cached.body.decode("utf-8")), cached.retrieved


def filter_records(registry, verbs: set[str] | None,
                   curated_verbs: dict[str, list[str]], log) -> None:
    """Drop imported verbs winetricks does not know; refuse unknown curated ones.

    A curated verb is a human claim with evidence, so a typo there stops the
    generator; an imported verb that winetricks has retired is dropped with
    a log line.
    """
    for record in registry.sorted():
        kept = []
        for verb in record.winetricks:
            known = schema.is_winetricks_verb(verb) and (verbs is None or verb in verbs)
            if known:
                kept.append(verb)
            elif verb in curated_verbs.get(record.id, ()) and verbs is not None:
                raise ValueError(f"curated game {record.id}: unknown winetricks verb {verb!r}")
            else:
                log(f"winetricks: {record.id} drops unknown verb {verb!r}")
        record.winetricks = kept
