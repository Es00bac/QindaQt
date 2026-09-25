# SPDX-License-Identifier: GPL-3.0-or-later
"""Scalar rules of compat-db-v1 (strings, ids, URLs, environment, verbs).

AGENT-CONTRACT: the Python twin of src/apps/qindalutris/core/
compat_db_rules.cpp. Every rule here exists there and vice versa; change
both in the same commit and update docs/wiki/apps/qindalutris-compat-db.md.
Lengths are UTF-16 code units (u16len) because the C++ side measures
QString::size().
"""

from __future__ import annotations

import datetime
import re
import unicodedata
from pathlib import Path

MAX_ENV_LINE = 1024 + 65  # kMaxEnvironmentValueChars + 65
MAX_TEXT = 2048
MAX_URL = 2048

VERB_FILE = Path(__file__).resolve().parents[1] / "winetricks-verbs.txt"

# AGENT-GUARD: the environment is an ALLOWLIST. A key is accepted only when it
# is listed here or is a DXVK_/VKD3D_ behaviour switch (see is_env_key). Every
# PROTON_* flag was checked against GE-Proton11-6 and 11-7's `proton` script;
# every __GL_* flag against NVIDIA's driver README (value-only settings, no
# paths). Anything else -- loader, interpreter, search-path, Vulkan layer and
# ICD, Wine binary, pressure-vessel, umu and Steam variables -- refuses the
# whole document, so a refreshed download can neither re-pin a title nor
# make the launch run code of its choosing. Widening this list is a schema
# decision: change compat_db_rules.cpp in the same commit.
ENV_EXACT = frozenset((
    "WINEDLLOVERRIDES", "WINE_FULLSCREEN_FSR", "WINE_FULLSCREEN_FSR_STRENGTH",
    "WINE_FULLSCREEN_FSR_MODE", "RADV_PERFTEST", "mesa_glthread",
    "STAGING_SHARED_MEMORY",
    "PROTON_NO_WM_DECORATION", "PROTON_USE_WINED3D", "PROTON_USE_WINED3D11",
    "PROTON_NO_ESYNC", "PROTON_NO_FSYNC", "PROTON_NO_NTSYNC",
    "PROTON_FORCE_LARGE_ADDRESS_AWARE", "PROTON_HIDE_NVIDIA_GPU",
    "PROTON_HIDE_INTEL_GPU", "PROTON_ENABLE_WAYLAND", "PROTON_USE_XALIA",
    "PROTON_PREFER_SDL", "PROTON_ENABLE_HDR", "PROTON_DISABLE_NVAPI",
    "PROTON_FORCE_NVAPI", "PROTON_NO_D3D10", "PROTON_NO_D3D11", "PROTON_DXVK_D3D8",
    "PROTON_HEAP_DELAY_FREE", "PROTON_HEAP_ZERO_MEMORY", "PROTON_OLD_GL_STRING",
    "PROTON_NO_XIM", "PROTON_SET_GAME_DRIVE",
    "__GL_SHADER_DISK_CACHE", "__GL_SHADER_DISK_CACHE_SIZE",
    "__GL_THREADED_OPTIMIZATIONS", "__GL_SYNC_TO_VBLANK", "__GL_VRR_ALLOWED",
    "__GL_YIELD", "__GL_FSAA_MODE", "__GL_SHARPEN_ENABLE", "__GL_SHARPEN_VALUE",
    "__GL_ALLOW_FXAA_USAGE",
))
ENV_PREFIXES = ("DXVK_", "VKD3D_")

_GAME_ID = re.compile(r"[a-z0-9][a-z0-9._:-]{0,127}")
_SOURCE_ID = re.compile(r"[a-z0-9][a-z0-9-]{0,63}")
_UMU_ID = re.compile(r"umu-[A-Za-z0-9._-]{1,124}")
_STEAM_ID = re.compile(r"[1-9][0-9]{0,9}")
_STORE_ID = re.compile(r"[A-Za-z0-9._:-]{1,128}")
_VERB = re.compile(r"[a-z0-9_=.-]{1,64}")
_BUILD = re.compile(r"[A-Za-z0-9][A-Za-z0-9 ._+()-]{0,127}")
_URL = re.compile(r"https://[A-Za-z0-9.-]+(:[0-9]{1,5})?([/?#][!-~]*)?")
_ENV_KEY = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,63}")
_ENV_SUFFIX = re.compile(r"[A-Z0-9_]+")
_TIMESTAMP = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z")


def u16len(text: str) -> int:
    # surrogatepass: a lone surrogate from a JSON \\ud800 escape is one
    # UTF-16 unit, as in QString, instead of an encoding crash.
    return len(text.encode("utf-16-le", "surrogatepass")) // 2


def is_text(text, max_len: int, allow_empty: bool = False) -> bool:
    if not isinstance(text, str) or u16len(text) > max_len:
        return False
    if not allow_empty and not text:
        return False
    return not any(unicodedata.category(ch) == "Cc" for ch in text)


def _full(pattern: re.Pattern, text) -> bool:
    return isinstance(text, str) and pattern.fullmatch(text) is not None


def is_game_id(text) -> bool: return _full(_GAME_ID, text)
def is_source_id(text) -> bool: return _full(_SOURCE_ID, text)
def is_umu_id(text) -> bool: return _full(_UMU_ID, text)
def is_steam_app_id(text) -> bool: return _full(_STEAM_ID, text)
def is_store_id(text) -> bool: return _full(_STORE_ID, text)
def is_verb_shape(text) -> bool: return _full(_VERB, text)


def is_build_name(text) -> bool:
    return _full(_BUILD, text) and not text.endswith(" ")


def is_https_url(text) -> bool:
    return _full(_URL, text) and len(text) <= MAX_URL


def is_exe_name(text) -> bool:
    return is_text(text, 128) and "/" not in text and "\\" not in text


def load_verb_file(path: Path = VERB_FILE) -> tuple[frozenset[str], dict[str, str]]:
    """(verbs, header fields) from winetricks-verbs.txt; '#' lines are comments."""
    verbs, header = set(), {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("#"):
            key, _, value = line[1:].partition(":")
            header[key.strip()] = value.strip()
        elif line:
            if not is_verb_shape(line):
                raise ValueError(f"{path}: bad verb line {line!r}")
            verbs.add(line)
    return frozenset(verbs), header


WINETRICKS_VERBS, WINETRICKS_HEADER = load_verb_file()


def is_winetricks_verb(text) -> bool:
    return is_verb_shape(text) and text in WINETRICKS_VERBS


def is_env_key(key: str) -> bool:
    """Case-sensitive: the exact list, or a DXVK_/VKD3D_ behaviour switch."""
    if key in ENV_EXACT:
        return True
    prefix = next((p for p in ENV_PREFIXES if key.startswith(p)), None)
    if prefix is None or not _full(_ENV_SUFFIX, key[len(prefix):]):
        return False
    return not (key.endswith(("_PATH", "_FILE", "_DIR")) or "LOG" in key
                or "CONFIG_FILE" in key)


def env_key(line: str) -> str:
    return line[:line.find("=")]


def is_env_assignment(line) -> bool:
    """One allowlisted KEY=VALUE line. Value: no '$', no backtick, no control."""
    if not isinstance(line, str) or not is_text(line, MAX_ENV_LINE):
        return False
    equals = line.find("=")
    if equals <= 0:
        return False
    key, value = line[:equals], line[equals + 1:]
    if not _full(_ENV_KEY, key) or not is_env_key(key):
        return False
    return "$" not in value and "`" not in value


def parse_timestamp(text):
    """The exact "YYYY-MM-DDTHH:MM:SSZ" form; None on any deviation."""
    if not _full(_TIMESTAMP, text):
        return None
    try:
        return datetime.datetime.strptime(text, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=datetime.timezone.utc)
    except ValueError:
        return None
