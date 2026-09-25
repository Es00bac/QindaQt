# SPDX-License-Identifier: GPL-3.0-or-later
"""winetricks: the committed verb allowlist and how it is refreshed.

tools/qindalutris-compat/winetricks-verbs.txt is the single list of verbs a
compat-db-v1 document may name. Both validators read it: this package at
import time (schema.py) and the C++ parser through a header CMake generates
from the same file at configure time. It is produced from `winetricks
list-all`, which prints category banners ("===== dlls =====") and one verb
per line (the first whitespace-separated token), restricted to the dlls,
fonts and settings categories: apps and benchmarks install programs, and
the prefix category only lists existing prefixes.

protontricks wraps winetricks and takes the same verbs, which is how Steam
users apply them; QindaLutris runs them as `umu-run winetricks <verbs>`.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from . import schema
from .fetch import utc_stamp

SOURCE_ID = "winetricks"
SOURCE_URL = "https://github.com/Winetricks/winetricks"
CATEGORIES = ("dlls", "fonts", "settings")
# Present in the settings category but never valid database advice: test
# verbs, interactive prompts, and a verb with an empty value.
EXCLUDED = frozenset(("bad", "good", "set_userpath", "set_mididevice", "winver="))


def is_forbidden(verb: str) -> bool:
    """Verbs that must never enter the allowlist, whatever winetricks says."""
    return (verb.startswith("-") or verb == "annihilate" or verb.startswith("prefix=")
            or verb.startswith("arch=") or verb.startswith("list") or verb in EXCLUDED)


def parse_list_all(text: str) -> dict[str, set[str]]:
    """`winetricks list-all` output as {category: verbs}."""
    categories: dict[str, set[str]] = {}
    current = None
    for line in text.splitlines():
        banner = re.fullmatch(r"===== (\S+) =====", line.strip())
        if banner:
            current = categories.setdefault(banner.group(1), set())
        elif current is not None and line.strip() and not line.startswith("Executing"):
            current.add(line.split()[0])
    return categories


def allowlist_from(text: str) -> list[str]:
    categories = parse_list_all(text)
    verbs = set().union(*(categories.get(c, set()) for c in CATEGORIES))
    return sorted(v for v in verbs if schema.is_verb_shape(v) and not is_forbidden(v))


def render_file(verbs: list[str], version: str, generated: str) -> str:
    header = [
        "# SPDX-License-Identifier: GPL-3.0-or-later",
        "# compat-db-v1 winetricks verb allowlist (ADR-0275 section 3).",
        "# AGENT-CONTRACT: the one source of truth for BOTH validators --",
        "# qlcompat/schema.py reads it and src/apps/qindalutris/core/CMakeLists.txt",
        "# turns it into a header for compat_db_rules.cpp. Do not edit by hand;",
        "# regenerate with: generate.py --update-winetricks-verbs",
        f"# categories: {' '.join(CATEGORIES)}",
        f"# winetricks-version: {version}",
        f"# generated: {generated}",
    ]
    return "\n".join(header + verbs) + "\n"


def _run(binary: str, *args: str) -> str | None:
    path = shutil.which(binary)
    if path is None:
        return None
    with tempfile.TemporaryDirectory() as scratch:
        # A throwaway, never-created prefix: listing verbs must not touch
        # the user's ~/.wine.
        env = dict(os.environ, WINEPREFIX=os.path.join(scratch, "unused-prefix"),
                   WINEDEBUG="-all")
        try:
            done = subprocess.run([path, *args], env=env, capture_output=True,
                                  text=True, timeout=120, check=False)
        except (OSError, subprocess.TimeoutExpired):
            return None
    return done.stdout if done.returncode == 0 and done.stdout else None


def update_file(binary: str, path: Path, log) -> int:
    """Regenerate the allowlist from the installed winetricks."""
    listing = _run(binary, "list-all")
    version = (_run(binary, "--version") or "").strip().splitlines()
    if listing is None or not version:
        log(f"winetricks: `{binary} list-all` failed; allowlist unchanged")
        return 1
    verbs = allowlist_from(listing)
    stamp = version[-1].split()[0]
    path.write_text(render_file(verbs, stamp, utc_stamp()), encoding="utf-8")
    log(f"winetricks: wrote {len(verbs)} verbs from winetricks {stamp} to {path}")
    return 0


def filter_records(registry, verbs: frozenset[str],
                   curated_verbs: dict[str, list[str]], log) -> None:
    """Drop imported verbs outside the allowlist; refuse curated ones.

    A curated verb is a human claim with evidence, so one outside the
    allowlist stops the generator; an imported one is dropped with a log line.
    """
    for record in registry.sorted():
        kept = []
        for verb in record.winetricks:
            if verb in verbs:
                kept.append(verb)
            elif verb in curated_verbs.get(record.id, ()):
                raise ValueError(f"curated game {record.id}: winetricks verb {verb!r} "
                                 "is not in winetricks-verbs.txt")
            else:
                log(f"winetricks: {record.id} drops verb {verb!r} (not in the allowlist)")
        record.winetricks = kept
