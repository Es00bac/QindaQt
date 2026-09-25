# SPDX-License-Identifier: GPL-3.0-or-later
"""Lutris installer scripts: winetricks verbs, environment, DLL overrides.

Reads https://lutris.net/api/installers/<game slug> (a paginated object whose
`results` are installer versions, each with `slug`, `runner`, `published`,
`draft` and the YAML-derived `script`). Only a curated game names a slug and
the one installer version to trust; only `wine`-runner scripts are read, and
only the facts the curated entry lists in `take`:

- winetricks:  installer tasks {name: winetricks, app: "<verb> <verb>"}
- environment: script.system.env, minus Lutris variables ($GAMEDIR...), the
               non-compatibility keys in SKIP_ENV and any key outside the
               schema's environment allowlist
- dlloverrides: script.wine.overrides as one WINEDLLOVERRIDES assignment

AGENT-NOTE: Lutris scripts carry no license, so ADR-0275 section 7 limits
what is taken to uncopyrightable facts -- verb names and environment
settings -- and the game's Lutris page is always added to its links as
attribution. Arguments and executable paths are deliberately not imported.
"""

from __future__ import annotations

import json

from . import schema
from .fetch import Fetcher

URL_TEMPLATE = "https://lutris.net/api/installers/{slug}"
SOURCE_URL = "https://lutris.net/api/installers/"
SOURCE_ID = "lutris"
TAKE = frozenset(("winetricks", "environment", "dlloverrides"))

# HUDs, shader-cache locations and config-file paths: Lutris conveniences,
# not compatibility fixes, and mostly paths inside a Lutris game directory.
SKIP_ENV = frozenset(("DXVK_HUD", "DXVK_STATE_CACHE_PATH", "DXVK_CONFIG_FILE",
                      "__GL_SHADER_DISK_CACHE", "__GL_SHADER_DISK_CACHE_PATH",
                      "__GL_SHADER_DISK_CACHE_SKIP_CLEANUP", "MANGOHUD",
                      "MANGOHUD_CONFIG"))
DLL_MODES = {"disabled": "d", "d": "d", "native": "n", "n": "n", "builtin": "b",
             "b": "b", "native,builtin": "n,b", "n,b": "n,b", "builtin,native": "b,n",
             "b,n": "b,n"}


def _installer(data: bytes, installer_slug: str) -> dict | None:
    document = json.loads(data.decode("utf-8"))
    for result in document.get("results") or []:
        if not isinstance(result, dict) or result.get("slug") != installer_slug:
            continue
        if result.get("runner") != "wine" or not result.get("published") \
                or result.get("draft"):
            return None
        return result.get("script") if isinstance(result.get("script"), dict) else None
    return None


def _winetricks(script: dict) -> list[str]:
    verbs = []
    for step in script.get("installer") or []:
        task = step.get("task") if isinstance(step, dict) else None
        if isinstance(task, dict) and task.get("name") == "winetricks":
            verbs += [v for v in str(task.get("app", "")).split() if v not in verbs]
    return verbs


def _environment(script: dict) -> list[str]:
    env = (script.get("system") or {}).get("env") or {}
    lines = []
    for key in sorted(env):
        value = env[key]
        value = ("1" if value else "0") if isinstance(value, bool) else str(value)
        if key in SKIP_ENV or "$" in value:
            continue
        line = f"{key}={value}"
        if schema.is_env_assignment(line):
            lines.append(line)
    return lines


def _dll_overrides(script: dict) -> list[str]:
    overrides = (script.get("wine") or {}).get("overrides") or {}
    parts = []
    for dll in sorted(overrides):
        mode = DLL_MODES.get(str(overrides[dll]).strip().lower())
        if mode is not None and all(ch.isalnum() or ch in "_.,-" for ch in dll):
            parts.append(f"{dll}={mode}")
    return [f"WINEDLLOVERRIDES={';'.join(parts)}"] if parts else []


def extract(script: dict, take: set[str]) -> dict[str, list[str]]:
    facts = {"winetricks": _winetricks(script), "environment": _environment(script),
             "dlloverrides": _dll_overrides(script)}
    return {key: value for key, value in facts.items() if key in take}


def fetch_facts(fetcher: Fetcher, slug: str, installer: str,
                take: set[str], log) -> tuple[dict[str, list[str]], str | None]:
    """Facts from one installer version, and the retrieval stamp used."""
    response = fetcher.get(URL_TEMPLATE.format(slug=slug), f"lutris-{slug}.json")
    if response is None or not response.ok:
        log(f"lutris: {slug} unavailable; curated facts only")
        return {}, None
    script = _installer(response.body, installer)
    if script is None:
        log(f"lutris: {slug}/{installer} missing, unpublished or not a wine script")
        return {}, None
    return extract(script, take), response.retrieved
