# QindaLutris compatibility database

QindaLutris keeps one shipped, refreshable document of what the Linux gaming
ecosystem already knows about running Windows games under Proton: which
build to pin a game to, which builds to avoid and why, which winetricks
verbs, environment variables and arguments a game needs, and whether its
anti-cheat works. The decision is [ADR-0275](../adr/0275-qindalutris-installs-games-and-manages-pinned-proton.md)
§3; the application that consumes it is described on the
[QindaLutris](qindalutris.md) page.

The document is advice. It never re-pins an installed title by itself: its
recommended build becomes the pin only for a new install, and an avoid entry
turns into a warning that offers a move.

## Where it lives

| Copy | Path | Written by |
|---|---|---|
| Shipped | `<datadir>/qindalutris/compat-db-v1.json` (normally `/usr/share/qindalutris/`) | `games-util/qindalutris-compat-db` (CMake component `QindaLutrisCompatDb`) |
| Refreshed | `$XDG_DATA_HOME/qindalutris/compat-db-v1.json` | the app's manual "check for newer information" |
| Source snapshot | `src/apps/qindalutris/compat/compat-db-v1.json` | `tools/qindalutris-compat/generate.py` |

The copy with the newer `generated` stamp wins. A copy that is absent,
refused, or stamped more than 24 hours after the current time never wins; if
both are equal the shipped copy is kept. When neither loads, the app runs
with an empty database: no advice, never a guess.

The loader opens the file with `O_NOFOLLOW` and checks its type and size
with `fstat` on the same descriptor it reads, so a symlink, a FIFO, a
directory or an oversized file is refused before a byte is parsed.

## Schema `compat-db-v1`

One JSON object, at most **32 MiB**. Every object has an exact key set: an
unknown key, a missing required key, a wrong type or any value outside its
set refuses the **whole document**. Lengths are counted in UTF-16 code
units. "Text" means a string with no control character (Unicode category
Cc, which includes newline and tab).

The bytes themselves must be one strict RFC 8259 JSON text: strict UTF-8
with no byte-order mark, only space, tab, LF and CR as whitespace, the RFC
number grammar (so `1.`, `.1e1`, `01` and `+1` are refused), and **no object
with two equal keys**, compared after unescaping. Python checks duplicates
with an `object_pairs_hook`; the C++ side runs a strict scanner over the raw
bytes (`compat_json_scan.cpp`) before `QJsonDocument`, which would otherwise
skip a BOM, accept those number forms and silently keep the last duplicate.
Nesting is capped at 512 levels by the scanner; deeper input could never
match the schema anyway.

### Top level (all keys required)

| Key | Rule |
|---|---|
| `schema` | exactly `"qindalutris-compat-db"` |
| `version` | the number `1` (a later schema is a new file, `compat-db-v2.json`) |
| `generated` | `YYYY-MM-DDTHH:MM:SSZ`, a real UTC date and time, at most 24 hours after the clock the document is judged against |
| `sources` | list of at most 32 `{id, url, retrieved}`; `id` matches `[a-z0-9][a-z0-9-]{0,63}` and is unique; `url` is an https URL; `retrieved` is a timestamp with the same rules as `generated` |
| `defaults` | `{recommendedBuild}`: `""` or a key of `builds` whose status is `tested` |
| `builds` | object of at most 256 entries: build name → `{status, notes}`; `status` is `tested`, `known-issues` or `untested`; `notes` is text of at most 2048 (may be empty) |
| `games` | list of at most 100000 game objects |

A **build name** is a Proton directory name: 1–128 characters from
`A–Z a–z 0–9 space . _ + ( ) -`, starting with a letter or digit and not
ending in a space (`GE-Proton11-6-x86_64`, `Proton 9.0 (Beta)`).

An **https URL** matches `https://[A-Za-z0-9.-]+(:[0-9]{1,5})?([/?#][!-~]*)?`
and is at most 2048 long: printable ASCII only, no user-info, no spaces.

### Game object

`id`, `title` and `keys` are required; everything else is optional and
absent when empty.

| Key | Rule |
|---|---|
| `id` | `[a-z0-9][a-z0-9._:-]{0,127}`, unique; stable across regenerations |
| `title` | text, 1–256 |
| `keys` | object; see below |
| `proton` | `{recommended?, avoid?}`: `recommended` must be a key of `builds` whose status is `tested`; `avoid` is a list of at most 64 `{build, reason, source}` with a unique, valid build name (any build, listed or not), `reason` and `source` text of 1–2048 |
| `environment` | at most 32 `KEY=VALUE` lines, each key at most once; see [the environment allowlist](#the-environment-allowlist) |
| `winetricks` | at most 64 verbs, each listed in `tools/qindalutris-compat/winetricks-verbs.txt`; see [the verb allowlist](#the-winetricks-verb-allowlist) |
| `arguments` | at most 64 text arguments of 1–512 |
| `antiCheat` | `{status, notes?}`: `status` is `supported`, `running`, `broken`, `denied`, `planned`, `none` or `unknown`; `notes` text of 1–2048 |
| `protondbTier` | `platinum`, `gold`, `silver`, `bronze`, `borked`, `native`, `pending` or `unknown` |
| `steamDeck` | `{category, notes?}`: `category` is `verified`, `playable`, `unsupported` or `unknown` (Valve's Steam Deck rating); `notes` is a list of at most 64 text notes of 1–2048, Valve's limitation and advice results made readable |
| `umuStore` | umu's `STORE` value: `amazon`, `battlenet`, `ea`, `egs`, `gog`, `humble`, `itchio`, `steam`, `ubisoft`, `umu`, `zoomplatform` or `none` |
| `notes` | at most 64 text notes of 1–2048 |
| `links` | at most 64 https URLs |

### The environment allowlist

A line is text of at most 1089 units: a key of at most 64 ASCII letters,
digits and underscores (not starting with a digit), `=`, and a value with no
`$` and no backtick. Keys are case-sensitive and must be on this list:

- exactly `WINEDLLOVERRIDES`, `WINE_FULLSCREEN_FSR`,
  `WINE_FULLSCREEN_FSR_STRENGTH`, `WINE_FULLSCREEN_FSR_MODE`,
  `RADV_PERFTEST`, `mesa_glthread`, `STAGING_SHARED_MEMORY`;
- these Proton behaviour flags, each present in the `proton` script of
  GE-Proton11-6 and 11-7: `PROTON_NO_WM_DECORATION`, `PROTON_USE_WINED3D`,
  `PROTON_USE_WINED3D11`, `PROTON_NO_ESYNC`, `PROTON_NO_FSYNC`,
  `PROTON_NO_NTSYNC`, `PROTON_FORCE_LARGE_ADDRESS_AWARE`,
  `PROTON_HIDE_NVIDIA_GPU`, `PROTON_HIDE_INTEL_GPU`, `PROTON_ENABLE_WAYLAND`,
  `PROTON_USE_XALIA`, `PROTON_PREFER_SDL`, `PROTON_ENABLE_HDR`,
  `PROTON_DISABLE_NVAPI`, `PROTON_FORCE_NVAPI`, `PROTON_NO_D3D10`,
  `PROTON_NO_D3D11`, `PROTON_DXVK_D3D8`, `PROTON_HEAP_DELAY_FREE`,
  `PROTON_HEAP_ZERO_MEMORY`, `PROTON_OLD_GL_STRING`, `PROTON_NO_XIM`,
  `PROTON_SET_GAME_DRIVE`;
- these NVIDIA OpenGL settings, all value-only in NVIDIA's driver README:
  `__GL_SHADER_DISK_CACHE`, `__GL_SHADER_DISK_CACHE_SIZE`,
  `__GL_THREADED_OPTIMIZATIONS`, `__GL_SYNC_TO_VBLANK`, `__GL_VRR_ALLOWED`,
  `__GL_YIELD`, `__GL_FSAA_MODE`, `__GL_SHARPEN_ENABLE`,
  `__GL_SHARPEN_VALUE`, `__GL_ALLOW_FXAA_USAGE`;
- any `DXVK_*` or `VKD3D_*` key whose suffix is upper-case letters, digits
  and underscores, except one ending in `_PATH`, `_FILE` or `_DIR` or
  containing `LOG` or `CONFIG_FILE`.

Every other key refuses the document. That includes the launch planner's
own (`PROTONPATH`, `WINEPREFIX`, `GAMEID`, `STORE`, `UMU_*`,
`PROTON_VERB`), loaders and interpreters (`LD_*`, `PYTHON*`, `BASH_ENV`,
`ENV`, `PERL5OPT`, `NODE_OPTIONS`), Wine binaries and search paths
(`WINE`, `WINELOADER`, `WINESERVER`, `WINEDLLPATH`, `PATH`, `GCONV_PATH`,
`LIBGL_DRIVERS_PATH`, `GIO_MODULE_DIR`), Vulkan and EGL layers and drivers
(`VK_*`, `__EGL_VENDOR_LIBRARY_FILENAMES`), `PRESSURE_VESSEL_*`,
`STEAM_COMPAT_*`, `PROTON_LOG*` and `BROWSER`. A refreshed download can
therefore neither re-pin a title nor make its launch run code of its
choosing. Widening the list is a schema change made on both sides at once.

### The winetricks verb allowlist

`tools/qindalutris-compat/winetricks-verbs.txt` is the only list of verbs a
document may name, and the single source both validators read: Python loads
it directly, and CMake turns it into `compat_winetricks_verbs.inc` for the
C++ rules at configure time. It is generated from `winetricks list-all`
(its header records the winetricks version and date), restricted to the
`dlls`, `fonts` and `settings` categories. Application and benchmark
installers, the `prefix` listing, a leading `-`, `annihilate`, `prefix=`,
`arch=`, anything starting with `list`, and winetricks' test and
interactive verbs (`bad`, `good`, `set_userpath`, `set_mididevice`,
`winver=`) are never in it.

`keys` (all optional):

| Key | Rule |
|---|---|
| `umuId` | `umu-[A-Za-z0-9._-]{1,124}` |
| `steamAppIds` | at most 64 of `[1-9][0-9]{0,9}` |
| `storeIds` | object keyed by `egs`, `gog`, `amazon`, `ubisoft`, `ea`, `battlenet`, `humble`, `itchio`, `zoomplatform`; each a non-empty list of at most 64 ids matching `[A-Za-z0-9._:-]{1,128}` |
| `exeNames` | at most 64 executable basenames: text of 1–128 without `/` or `\` |
| `titles` | at most 64 title aliases: text of 1–256 |

Every list refuses a repeated entry. Across the document, a game `id`, an
`umuId`, a Steam appid and a (store, id) pair may each belong to only one
game; a shared strong key refuses the document. Duplicate object keys are
resolved the same way by both parsers (the last one wins); the generator
never writes them.

## Looking a game up

`CompatDatabase::lookup(GameKeys)` tries the keys a title has, strongest
first, and reports which one matched:

1. umu id
2. store id (only together with its store)
3. Steam appid
4. executable basename, case-insensitively (a full Windows or POSIX path is reduced to its basename)
5. title, after the same normalization the library uses to de-duplicate
   Steam and Lutris games (`normalizedTitleForMatch`), including the `titles` aliases

Executable names and titles are weak keys: when one names two games it is
skipped rather than guessed, and lookup falls through to the next key.

For the install and move flows the model also answers:

- `recommendedBuildFor(db, advice)`: the game's recommended build, else the
  database default. This is the pin for a new install.
- `avoidReasonFor(advice, build)`: the plain-language reason a build is
  known-bad for this game, or empty.
- `buildStatus(name)` / `buildNotes(name)`: `Tested` shows as "Tested by
  QindaQt"; an unknown build is `Untested`.

The launch planner should take `STORE` from the matched store when the match
was a store id, and from `umuStore` otherwise. `GAMEID` is the `umuId` when
there is one, else `umu-0`.

## Where the facts come from

`tools/qindalutris-compat/generate.py` merges these, lowest precedence
first, and records each source's retrieval time in `sources`:

| Source | What it contributes |
|---|---|
| [umu-database](https://github.com/Open-Wine-Components/umu-database) CSV | umu ids, store codenames, titles, notes, executable names; a numeric umu id is the Steam appid, as its README defines |
| [AreWeAntiCheatYet](https://areweanticheatyet.com/) `games.json` | anti-cheat status (its five states map one-to-one), anti-cheat names and notes, Steam appids |
| Steam store app details | Steam's own name for an appid, asked only when several AreWeAntiCheatYet entries claim that appid |
| [ProtonDB](https://www.protondb.com/) per-app summaries | `protondbTier` for at most `--protondb-limit` Steam appids already in the database |
| Steam store Deck compatibility report | `steamDeck` for the same appids under the same limit; an undocumented endpoint the store page itself uses, so it is cached, rate-limited and skipped on any error |
| [Lutris](https://lutris.net/) installer scripts | winetricks verb names and environment settings (including DLL overrides), only for curated games that name one installer |
| `winetricks-verbs.txt` | the committed verb allowlist every verb must appear in |
| `curated.toml` | everything a person has verified, each with its evidence |

When several AreWeAntiCheatYet entries claim one Steam appid (a renamed
game listed under both names, or a wrong id), the appid goes only to the
entry whose name matches Steam's own name for it; when none or several
match, no entry keys that appid, and a withheld entry may not reach it
through a title either. Entries whose slugs are percent-encoded or
non-ASCII keep an id derived from the decoded slug plus a short hash. Every
such decision is printed and counted among the generator's warnings.

Valve's Steam Deck results arrive as tokens such as
`#SteamDeckVerified_TestResult_InterfaceTextIsNotLegible`; known tokens are
turned into sentences from a table, and an unknown one into the words of
its whole name after `_TestResult_`. Passed checks are not recorded.

**Data licensing** (ADR-0275 §7). PCGamingWiki is only linked, never
copied. Lutris install scripts carry no license, so only uncopyrightable
facts are taken from them -- winetricks verb names and environment
settings -- and each such game links its Lutris page as attribution.
ProtonDB data is under the ODbL: the `protondb` source entry and the per-game
ProtonDB link are the attribution, and any screen that shows a tier must
label it as ProtonDB's community rating.

protontricks accepts the same verbs as winetricks and is how Steam users
apply them to a Steam game. QindaLutris applies them itself, once per
prefix, as `umu-run winetricks <verbs>`.

The refresh, curation and release procedure is in
`tools/qindalutris-compat/README.md`.

## The two validators

The C++ parser (`src/apps/qindalutris/core/compat_db_parse.cpp`,
`compat_db_rules.cpp` and `compat_json_scan.cpp`) and the generator's
validator (`tools/qindalutris-compat/qlcompat/schema.py` and
`schema_rules.py`) implement the rules above one for one; an
`AGENT-CONTRACT` marker on each side names the other. Both judge against an
injected clock (`validate.py --now`, the `now` argument of
`parseCompatDocument`/`loadCompatDatabase`/`chooseNewer`). Both test suites
judge the same fixtures in `tests/apps/qindalutris/data/compat/{valid,refused}`,
the same rules tables (`tst_compat_rules.cpp` and `test_rules.py`), the
differential cases, and the committed snapshot, so a rule changed on one
side only fails a gate.

| Test | What it proves |
|---|---|
| `qindaqt.qindalutris-compat-db` | every field (including `steamDeck`) loads; every refused fixture is refused whole; absent, symlink, FIFO, directory, oversize and 100001-game documents; newer-wins; the committed snapshot loads |
| `qindaqt.qindalutris-compat-rules` | the environment and verb allowlists key by key, pins on tested builds only, the 24-hour future rule and `chooseNewer`, and the byte-level JSON rules |
| `qindaqt.qindalutris-compat-differential` | 172 mutation cases (the independent review's 121 plus allowlist and clock cases) judged by both validators, which must agree with each other and with `compat_differential/expected.txt` |
| `qindaqt.qindalutris-compat-lookup` | key precedence table, ambiguity withheld, basename and alias matching, avoid reasons, recommended build, build status |
| `qindaqt.qindalutris-compat-generator` | the Python validator on the shared fixtures, each importer on offline copies of the real formats, deterministic offline generation, curated evidence rules |
| `qindaqt.qindalutris-compat-snapshot` | `validate.py` accepts the committed snapshot |
