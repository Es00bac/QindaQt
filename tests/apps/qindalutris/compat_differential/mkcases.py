# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the compat-db-v1 differential mutation cases into a directory.

    mkcases.py OUT_DIR REPO_ROOT

The cases began as the independent reviewer's 121 mutations of the shared
minimal fixture (2026-09-25) -- byte-level JSON forms, stamps, bounds,
strong keys, verbs and environment -- plus one case per environment key and
verb the allowlists must refuse or accept. run_differential.py judges every
case with both validators and compares them with expected.txt.
"""
import json, copy, pathlib, sys
R = pathlib.Path(sys.argv[2])
sys.path.insert(0, str(R / "tools" / "qindalutris-compat"))
out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for f in out.glob("*"): f.unlink()
minimal = json.loads((R/"tests/apps/qindalutris/data/compat/valid/minimal.json").read_bytes())
def w(name, data):
    if isinstance(data, (dict, list)): data = json.dumps(data, ensure_ascii=False).encode("utf-8")
    if isinstance(data, str): data = data.encode("utf-8", "surrogatepass")
    (out/name).write_bytes(data)
def game(**kw):
    g = {"id": "g1", "title": "Game One", "keys": {}}
    g.update(kw); return g
def doc(*games, **top):
    d = copy.deepcopy(minimal); d["games"] = list(games); d.update(top); return d
M = json.dumps(doc(game(title="TITLEX")), ensure_ascii=False)
def raw(name, title_json_fragment):  # substitute raw JSON text for the title string
    w(name, M.replace('"TITLEX"', title_json_fragment))
w("bom", b"\xef\xbb\xbf" + json.dumps(minimal).encode())
w("invalid-utf8-title", M.replace("TITLEX", "A\udcffB").encode("utf-8","surrogateescape") if False else M.encode().replace(b"TITLEX", b"A\xffB"))
w("overlong-utf8", M.encode().replace(b"TITLEX", b"A\xc0\x80B"))
w("encoded-surrogate-utf8", M.encode().replace(b"TITLEX", b"A\xed\xa0\x80B"))
w("truncated-utf8", M.encode().replace(b"TITLEX", b"A\xe2\x82B"))
raw("lone-surrogate-escape", '"A\\ud800B"')
raw("nul-escape", '"A\\u0000B"')
raw("c1-nel", '"A\\u0085B"')
raw("u2028", '"A\\u2028B"')
raw("raw-tab", '"A\tB"')
raw("raw-del", '"A\x7fB"')
raw("fffe", '"A\\ufffeB"')
w("trailing-nul", json.dumps(minimal).encode() + b"\x00")
w("trailing-garbage", json.dumps(minimal).encode() + b" x")
w("trailing-second-doc", json.dumps(minimal).encode() + b"{}")
w("formfeed-ws", json.dumps(minimal).replace(", ", ",\f").encode())
w("vt-ws", json.dumps(minimal).replace(", ", ",\v").encode())
w("nbsp-ws", json.dumps(minimal).replace(", ", ", ").encode())
w("trailing-comma", json.dumps(minimal)[:-1] + ",}")
for v in ["1.0", "1e0", "1.0000000000000001", "01", "+1", "-0", "1e400", "true", '"1"', "1E0", "0.1e1", "10e-1", "1.", ".1e1"]:
    w("version-" + v.replace('"','q').replace('+','plus').replace('.','p'), json.dumps(minimal).replace('"version": 1', '"version": ' + v))
for ts in ["2026-09-01T00:00:60Z", "2026-09-01T24:00:00Z", "0000-01-01T00:00:00Z", "0001-01-01T00:00:00Z", "2025-02-29T00:00:00Z", "2024-02-29T00:00:00Z", "9999-12-31T23:59:59Z", "2026-13-01T00:00:00Z", "2026-09-01t00:00:00Z", "2026-09-01T00:00:00z", "２026-09-01T00:00:00Z", "2026-09-31T00:00:00Z"]:
    w("generated-" + ts.replace(":", "_"), doc(generated=ts))
# duplicate keys
s = json.dumps(minimal)
w("dup-generated-bad-first", s.replace('"generated"', '"generated": "nope", "generated"', 1))
w("dup-generated-bad-last", s.replace('"generated": "2026-09-01T00:00:00Z"', '"generated": "2026-09-01T00:00:00Z", "generated": "nope"', 1))
gm = json.dumps(doc(game(environment=["A=1"])))
w("dup-env-bad-first", gm.replace('"environment": ["A=1"]', '"environment": ["LD_PRELOAD=/x"], "environment": ["A=1"]'))
w("dup-env-bad-last", gm.replace('"environment": ["A=1"]', '"environment": ["A=1"], "environment": ["LD_PRELOAD=/x"]'))
# duplicate game object key for store ids
st = json.dumps(doc(game(keys={"storeIds": {"egs": ["a"]}})))
w("dup-store-key", st.replace('"storeIds": {"egs": ["a"]}', '"storeIds": {"egs": ["a"], "egs": ["b"]}'))
# deep nesting inside unknown key and inside a known list
for depth in (100, 1000, 1025, 5000, 100000):
    w(f"deep-{depth}", s[:-1] + ', "x": ' + "[" * depth + "]" * depth + "}")
    w(f"deep-env-{depth}", json.dumps(doc(game())).replace('"keys": {}', '"keys": {}, "environment": ' + "[" * depth + "]" * depth))
# UTF-16 length bounds
w("title-128-astral", doc(game(title="\U0001F600" * 128)))
w("title-129-astral", doc(game(title="\U0001F600" * 129)))
w("title-256-bmp", doc(game(title="é" * 256)))
w("title-257-bmp", doc(game(title="é" * 257)))
# strong keys
w("same-umu-two-games", doc(game(id="a", keys={"umuId": "umu-1"}), game(id="b", keys={"umuId": "umu-1"})))
w("same-exe-two-games", doc(game(id="a", keys={"exeNames": ["Launcher.exe"]}), game(id="b", keys={"exeNames": ["launcher.EXE"]})))
w("exe-dot", doc(game(keys={"exeNames": [".."]})))
w("exe-empty", doc(game(keys={"exeNames": [""]})))
w("store-empty-obj", doc(game(keys={"storeIds": {}})))
w("store-case", doc(game(keys={"storeIds": {"EGS": ["x"]}})))
w("umu-null", doc(game(keys={"umuId": None})))
w("list-null-entry", doc(game(notes=[None])))
w("verb-leading-dash", doc(game(winetricks=["--self-update"])))
w("verb-annihilate", doc(game(winetricks=["annihilate"])))
w("verb-prefix", doc(game(winetricks=["prefix=evil"])))
w("arg-leading-dash", doc(game(arguments=["--foo"])))
w("env-lowercase-ld", doc(game(environment=["ld_preload=/x"])))
w("env-pythonpath", doc(game(environment=["PYTHONPATH=/tmp"])))
w("env-key-unicode", doc(game(environment=["É=1"])))
w("env-key-digit-first", doc(game(environment=["1A=1"])))
w("env-empty-value", doc(game(environment=["A="])))
w("env-long-key", doc(game(environment=["A" * 65 + "=1"])))
w("env-1089", doc(game(environment=["A=" + "x" * 1087])))
w("env-1090", doc(game(environment=["A=" + "x" * 1088])))
w("env-1089-astral", doc(game(environment=["A=" + "x" * 1085 + "\U0001F600"])))
w("env-1090-astral", doc(game(environment=["A=" + "x" * 1086 + "\U0001F600"])))
w("env-33", doc(game(environment=[f"A{i}=1" for i in range(33)])))
w("env-32", doc(game(environment=[f"A{i}=1" for i in range(32)])))
w("url-port6", doc(game(links=["https://a:123456/"])))
w("url-userinfo", doc(game(links=["https://user@evil/"])))
w("url-host-only", doc(game(links=["https://a"])))
w("url-empty-host", doc(game(links=["https:///x"])))
w("url-https-upper", doc(game(links=["HTTPS://a/"])))
w("url-2049", doc(game(links=["https://a/" + "x" * 2039])))
w("url-2048", doc(game(links=["https://a/" + "x" * 2038])))
w("build-trailing-space", doc(builds={"A ": {"status": "tested", "notes": ""}}))
w("build-dotdot", doc(builds={"..": {"status": "tested", "notes": ""}}))
w("build-leading-dash", doc(game(proton={"recommended": "-x"})))
w("recommended-unknown-build", doc(game(proton={"recommended": "Nonexistent-1"})))
w("avoid-empty", doc(game(proton={"avoid": []})))
w("proton-empty", doc(game(proton={})))
w("anticheat-notes-empty", doc(game(antiCheat={"status": "none", "notes": ""})))
w("deck-notes-empty-list", doc(game(steamDeck={"category": "verified", "notes": []})))
w("game-id-trailing-dash", doc(game(id="a-")))
w("games-100001", doc(*[game(id=f"g{i}") for i in range(100001)]))
w("games-100000", doc(*[game(id=f"g{i}") for i in range(100000)]))
w("sources-33", doc(sources=[{"id": f"s{i}", "url": "https://a/", "retrieved": "2026-09-01T00:00:00Z"} for i in range(33)]))
w("builds-257", doc(builds={f"B{i}": {"status": "tested", "notes": ""} for i in range(257)}))
w("defaults-null", doc(defaults={"recommendedBuild": None}))
w("root-array", [minimal])
w("empty-file", b"")
w("big-33MiB", json.dumps(minimal).encode()[:-1] + b',"pad":"' + b"x" * (32*1024*1024) + b'"}')
w("exact-32MiB-ws", (json.dumps(minimal).encode() + b" " * (32*1024*1024))[:32*1024*1024])
w("titles-alias-bad", doc(game(keys={"titles": [""]})))
w("steam-appid-11digits", doc(game(keys={"steamAppIds": ["12345678901"]})))
w("steam-appid-10digits-overflow", doc(game(keys={"steamAppIds": ["9999999999"]})))
w("umu-store-steam", doc(game(umuStore="steam")))
w("source-id-dup-case", doc(sources=[{"id": "a", "url": "https://a/", "retrieved": "2026-09-01T00:00:00Z"}, {"id": "a", "url": "https://b/", "retrieved": "2026-09-01T00:00:00Z"}]))
w("escaped-key-schema", s.replace('"schema"', '"sch\\u0065ma"'))
w("key-with-nul-suffix", s.replace('"schema"', '"schema\\u0000"'))


# Allowlist cases added with the review repair.
for key in ["PYTHONPATH", "PYTHONHOME", "BASH_ENV", "ENV", "WINELOADER", "WINESERVER",
            "WINEDLLPATH", "VK_ICD_FILENAMES", "VK_ADD_LAYER_PATH", "GCONV_PATH",
            "LIBGL_DRIVERS_PATH", "__EGL_VENDOR_LIBRARY_FILENAMES", "GIO_MODULE_DIR",
            "PRESSURE_VESSEL_FILESYSTEMS_RW", "UMU_ZENITY", "STEAM_COMPAT_MOUNTS", "path",
            "BROWSER", "PERL5OPT", "NODE_OPTIONS", "DXVK_LOG_PATH", "DXVK_CONFIG_FILE",
            "PROTON_LOG", "PROTON_VERB", "DXVK_ASYNC", "VKD3D_CONFIG", "PROTON_NO_ESYNC",
            "WINE_FULLSCREEN_FSR", "mesa_glthread", "MESA_GLTHREAD",
            # vkd3d-proton environment forwarding and path-taking switches.
            "VKD3D_UNIX_ENV", "VKD3D_UNIX_POST_ENV", "VKD3D_QUEUE_PROFILE",
            "VKD3D_SHADER_OVERRIDE", "VKD3D_QA_HASHES", "DXVK_HUD", "VKD3D_FRAME_RATE"]:
    w("envkey-" + key, doc(game(environment=[key + "=1"])))
w("env-dollar", doc(game(environment=["DXVK_HUD=$HOME"])))
w("env-backtick", doc(game(environment=["DXVK_HUD=`id`"])))
w("env-dup-key", doc(game(environment=["DXVK_HUD=1", "DXVK_HUD=0"])))
# The forwarding variables must be refused whatever they carry.
w("env-vkd3d-unix-env-ldpreload", doc(game(environment=["VKD3D_UNIX_ENV=LD_PRELOAD=/tmp/x.so"])))
for verb in ["corefonts", "win10", "arch=win32", "list-all", "7zip", "bad", "-q", "notaverb",
             "mimeassoc=on", "mimeassoc=off", "remove_mono"]:
    w("verbcase-" + verb, doc(game(winetricks=[verb])))
w("recommended-untested", doc(game(proton={"recommended": "U"}),
                              builds={"U": {"status": "untested", "notes": ""}}))
w("recommended-tested", doc(game(proton={"recommended": "T"}),
                            builds={"T": {"status": "tested", "notes": ""}}))
w("generated-plus-25h", doc(generated="2026-09-26T01:00:00Z"))
w("generated-plus-23h", doc(generated="2026-09-25T23:00:00Z"))
# 33 distinct names from the environment allowlist (both validators list them).
ALLOWED_KEYS = sorted(__import__("importlib").import_module("qlcompat.schema_rules").ENV_EXACT)
# The reviewer's length/count env cases use keys the allowlist now refuses,
# so the bounds are exercised again with an allowlisted key.
w("env-allowed-1089", doc(game(environment=["DXVK_HUD=" + "x" * 1080])))
w("env-allowed-1090", doc(game(environment=["DXVK_HUD=" + "x" * 1081])))
w("env-allowed-1089-astral", doc(game(environment=["DXVK_HUD=" + "x" * 1078 + "\U0001F600"])))
w("env-allowed-1090-astral", doc(game(environment=["DXVK_HUD=" + "x" * 1079 + "\U0001F600"])))
w("env-allowed-32", doc(game(environment=[f"{k}=1" for k in ALLOWED_KEYS[:32]])))
w("env-allowed-33", doc(game(environment=[f"{k}=1" for k in ALLOWED_KEYS[:33]])))
w("env-allowed-empty-value", doc(game(environment=["DXVK_HUD="])))
