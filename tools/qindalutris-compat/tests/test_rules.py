# SPDX-License-Identifier: GPL-3.0-or-later
"""The Python twin of tests/apps/qindalutris/tst_compat_rules.cpp.

Same tables, same verdicts: the environment and verb allowlists, pins only
on tested builds, the future-stamp rule and the byte-level JSON rules.
"""

from __future__ import annotations

import json
import unittest

import tests.support  # noqa: F401  (puts the tool on sys.path)
from qlcompat import schema

NOW = schema.parse_timestamp("2026-09-25T12:00:00Z")


def document(game: dict, generated="2026-09-01T00:00:00Z", sources=(), default="") -> bytes:
    return json.dumps({
        "schema": "qindalutris-compat-db", "version": 1, "generated": generated,
        "sources": list(sources), "defaults": {"recommendedBuild": default},
        "builds": {"Tested-1": {"status": "tested", "notes": ""},
                   "Untested-1": {"status": "untested", "notes": ""}},
        "games": [dict({"id": "g", "title": "G", "keys": {}}, **game)]},
        separators=(",", ":")).encode()


def accepted(data: bytes, now=NOW) -> bool:
    try:
        schema.load_bytes(data, now)
    except schema.Refused:
        return False
    return True


REFUSED_ENV_KEYS = (
    "PYTHONPATH", "PYTHONHOME", "PYTHONSTARTUP", "BASH_ENV", "ENV", "WINELOADER",
    "WINESERVER", "WINEDLLPATH", "WINEPREFIX", "WINE", "VK_ICD_FILENAMES",
    "VK_ADD_LAYER_PATH", "VK_INSTANCE_LAYERS", "GCONV_PATH", "LIBGL_DRIVERS_PATH",
    "__EGL_VENDOR_LIBRARY_FILENAMES", "GIO_MODULE_DIR", "PRESSURE_VESSEL_FILESYSTEMS_RW",
    "PRESSURE_VESSEL_SHELL", "UMU_ZENITY", "UMU_RUNTIME_UPDATE", "UMU_ID", "PROTONPATH",
    "GAMEID", "STORE", "STEAM_COMPAT_DATA_PATH", "STEAM_COMPAT_MOUNTS", "LD_PRELOAD",
    "LD_LIBRARY_PATH", "ld_preload", "path", "PATH", "HOME", "BROWSER", "PERL5OPT",
    "NODE_OPTIONS", "DXVK_LOG_PATH", "DXVK_LOG_LEVEL", "DXVK_CONFIG_FILE",
    "DXVK_STATE_CACHE_PATH", "VKD3D_SHADER_CACHE_PATH", "VKD3D_LOG_FILE", "PROTON_LOG",
    "PROTON_LOG_DIR", "PROTON_VERB", "PROTON_CRASH_REPORT_DIR", "PROTON_ENABLE_NVAPI",
    "MESA_GLTHREAD", "dxvk_async", "DXVK_", "__GL_WRITE_TEXT_SECTION",
    "__GL_SHADER_DISK_CACHE_PATH")
ACCEPTED_ENV = (
    "WINEDLLOVERRIDES=locationapi=d;nvapi,nvapi64=d", "DXVK_ASYNC=1", "DXVK_HUD=fps",
    "DXVK_FRAME_RATE=60", "VKD3D_CONFIG=dxr11", "VKD3D_FEATURE_LEVEL=12_1",
    "PROTON_NO_ESYNC=1", "PROTON_NO_WM_DECORATION=1", "PROTON_ENABLE_WAYLAND=1",
    "PROTON_FORCE_NVAPI=1", "WINE_FULLSCREEN_FSR=1", "WINE_FULLSCREEN_FSR_STRENGTH=2",
    "RADV_PERFTEST=gpl", "mesa_glthread=true", "STAGING_SHARED_MEMORY=1",
    "__GL_SHADER_DISK_CACHE=1", "__GL_THREADED_OPTIMIZATIONS=1", "DXVK_ASYNC=")
VERBS = (("corefonts", True), ("win10", True), ("d3dcompiler_47", True),
         ("renderer=vulkan", True), ("vd=off", True), ("annihilate", False), ("-q", False),
         ("--self-update", False), ("prefix=evil", False), ("arch=win32", False),
         ("list-all", False), ("list", False), ("apps", False), ("7zip", False),
         ("steam", False), ("bad", False), ("winver=", False), ("notaverb", False),
         ("Corefonts", False))


class Rules(unittest.TestCase):
    def test_baseline(self):
        self.assertTrue(accepted(document({"notes": ["n"]})))

    def test_environment_keys_outside_the_allowlist_refuse_the_document(self):
        for key in REFUSED_ENV_KEYS:
            with self.subTest(key):
                self.assertFalse(accepted(document({"environment": [key + "=1"]})))

    def test_allowlisted_environment_is_accepted(self):
        for line in ACCEPTED_ENV:
            with self.subTest(line):
                self.assertTrue(accepted(document({"environment": [line]})))

    def test_environment_values_cannot_expand_or_repeat(self):
        for line in ("DXVK_HUD=$HOME", "DXVK_HUD=`id`", "DXVK_HUD=a\x01"):
            self.assertFalse(accepted(document({"environment": [line]})), line)
        self.assertFalse(accepted(document({"environment": ["DXVK_ASYNC=1", "DXVK_ASYNC=0"]})))
        self.assertTrue(accepted(document({"environment": ["DXVK_ASYNC=1", "DXVK_HUD=0"]})))
        longest = "DXVK_HUD=" + "x" * (1089 - 9)
        self.assertTrue(accepted(document({"environment": [longest]})))
        self.assertFalse(accepted(document({"environment": [longest + "x"]})))

    def test_winetricks_verbs_must_be_in_the_committed_allowlist(self):
        for verb, ok in VERBS:
            with self.subTest(verb):
                self.assertEqual(accepted(document({"winetricks": [verb]})), ok)

    def test_pins_must_name_tested_builds(self):
        self.assertTrue(accepted(document({"proton": {"recommended": "Tested-1"}})))
        self.assertFalse(accepted(document({"proton": {"recommended": "Untested-1"}})))
        self.assertFalse(accepted(document({"proton": {"recommended": "Unknown-1"}})))
        self.assertTrue(accepted(document({"notes": ["n"]}, default="Tested-1")))
        self.assertFalse(accepted(document({"notes": ["n"]}, default="Untested-1")))
        avoid = {"avoid": [{"build": "Unknown-2", "reason": "r", "source": "s"}]}
        self.assertTrue(accepted(document({"proton": avoid})))

    def test_stamps_more_than_a_day_ahead_are_refused(self):
        game = {"notes": ["n"]}
        self.assertTrue(accepted(document(game, "2026-09-26T12:00:00Z")))
        self.assertFalse(accepted(document(game, "2026-09-26T12:00:01Z")))
        source = {"id": "s", "url": "https://s.org/", "retrieved": "2026-09-27T00:00:00Z"}
        self.assertFalse(accepted(document(game, sources=[source])))
        source["retrieved"] = "2026-09-26T00:00:00Z"
        self.assertTrue(accepted(document(game, sources=[source])))

    def test_byte_level_json_rules_match_cpp(self):
        good = document({"notes": ["n"]})
        cases = {
            "BOM": (b"\xef\xbb\xbf" + good, False),
            "version 1.": (good.replace(b'"version":1', b'"version":1.'), False),
            "version .1e1": (good.replace(b'"version":1', b'"version":.1e1'), False),
            "version 1.0": (good.replace(b'"version":1', b'"version":1.0'), True),
            "version 01": (good.replace(b'"version":1', b'"version":01'), False),
            "duplicate key": (good.replace(b'"version":1', b'"version":1,"version":1'), False),
            "escaped duplicate": (good.replace(b'"version":1',
                                               b'"version":1,"v\\u0065rsion":1'), False),
            "raw tab": (good.replace(b'"n"', b'"a\tb"'), False),
            "form feed": (good.replace(b',"sources"', b',\f"sources"'), False),
            "invalid utf-8": (good.replace(b'"n"', b'"\xff"'), False),
            "overlong": (good.replace(b'"n"', b'"\xc0\x80"'), False),
            "encoded surrogate": (good.replace(b'"n"', b'"\xed\xa0\x80"'), False),
            "trailing garbage": (good + b" x", False),
            "trailing comma": (good.replace(b'"notes":["n"]', b'"notes":["n",]'), False),
            "single quotes": (good.replace(b'"n"', b"'n'"), False),
            "deep nesting": (good[:-1] + b',"x":' + b"[" * 600 + b"]" * 600 + b"}", False),
        }
        for name, (data, ok) in cases.items():
            with self.subTest(name):
                self.assertEqual(accepted(data), ok)


if __name__ == "__main__":
    unittest.main()
