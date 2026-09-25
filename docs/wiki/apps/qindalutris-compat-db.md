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

The copy with the newer `generated` stamp wins. A copy that is absent or
refused never wins; if both are equal the shipped copy is kept. When neither
loads, the app runs with an empty database: no advice, never a guess.

## Schema `compat-db-v1`

One UTF-8 JSON object, at most **32 MiB**. Every object has an exact key
set: an unknown key, a missing required key, a wrong type or any value
outside its set refuses the **whole document**. Lengths are counted in
UTF-16 code units. "Text" means a string with no control character (Unicode
category Cc, which includes newline and tab).

### Top level (all keys required)

| Key | Rule |
|---|---|
| `schema` | exactly `"qindalutris-compat-db"` |
| `version` | the number `1` (a later schema is a new file, `compat-db-v2.json`) |
| `generated` | `YYYY-MM-DDTHH:MM:SSZ`, a real UTC date and time |
| `sources` | list of at most 32 `{id, url, retrieved}`; `id` matches `[a-z0-9][a-z0-9-]{0,63}` and is unique; `url` is an https URL; `retrieved` is a timestamp like `generated` |
| `defaults` | `{recommendedBuild}`: `""` or a key of `builds` |
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
| `proton` | `{recommended?, avoid?}`: `recommended` is a build name; `avoid` is a list of at most 64 `{build, reason, source}` with a unique build name, `reason` and `source` text of 1–2048 |
| `environment` | at most 32 `KEY=VALUE` lines; the key is an ASCII identifier of at most 64, the line at most 1089 long with no control character. Reserved keys are refused: `PROTONPATH`, `WINEPREFIX`, `GAMEID`, `STORE`, `UMU_ID`, `UMU_RUNTIME_UPDATE`, `PROTON_VERB`, `STEAM_COMPAT_DATA_PATH`, `STEAM_COMPAT_CLIENT_INSTALL_PATH`, `PATH`, `HOME`, `WINE` and every `LD_*` |
| `winetricks` | at most 64 verbs matching `[a-z0-9_=.-]{1,64}` |
| `arguments` | at most 64 text arguments of 1–512 |
| `antiCheat` | `{status, notes?}`: `status` is `supported`, `running`, `broken`, `denied`, `planned`, `none` or `unknown`; `notes` text of 1–2048 |
| `protondbTier` | `platinum`, `gold`, `silver`, `bronze`, `borked`, `native`, `pending` or `unknown` |
| `steamDeck` | `{category, notes?}`: `category` is `verified`, `playable`, `unsupported` or `unknown` (Valve's Steam Deck rating); `notes` is a list of at most 64 text notes of 1–2048, Valve's limitation and advice results made readable |
| `umuStore` | umu's `STORE` value: `amazon`, `battlenet`, `ea`, `egs`, `gog`, `humble`, `itchio`, `steam`, `ubisoft`, `umu`, `zoomplatform` or `none` |
| `notes` | at most 64 text notes of 1–2048 |
| `links` | at most 64 https URLs |

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
| [ProtonDB](https://www.protondb.com/) per-app summaries | `protondbTier` for at most `--protondb-limit` Steam appids already in the database |
| Steam store Deck compatibility report | `steamDeck` for the same appids under the same limit; an undocumented endpoint the store page itself uses, so it is cached, rate-limited and skipped on any error |
| [Lutris](https://lutris.net/) installer scripts | winetricks verbs, environment, DLL overrides, arguments and executables, only for curated games that name one installer |
| winetricks `list-all` | the verb list every imported verb must appear in |
| `curated.toml` | everything a person has verified, each with its evidence |

ADR-0275 names ProtonDB's public data dumps; the generator reads the
per-appid summary endpoint instead, because it is small, cacheable and
bounded by a flag, where a dump is a bulk download of every report.

protontricks accepts the same verbs as winetricks and is how Steam users
apply them to a Steam game. QindaLutris applies them itself, once per
prefix, as `umu-run winetricks <verbs>`.

The refresh, curation and release procedure is in
`tools/qindalutris-compat/README.md`.

## The two validators

The C++ parser (`src/apps/qindalutris/core/compat_db_parse.cpp` and
`compat_db_rules.cpp`) and the generator's validator
(`tools/qindalutris-compat/qlcompat/schema.py`) implement the rules above
one for one; an `AGENT-CONTRACT` marker on each side names the other. Both
test suites judge the same fixtures in
`tests/apps/qindalutris/data/compat/{valid,refused}` and load the committed
snapshot, so a rule changed on one side only fails a gate.

| Test | What it proves |
|---|---|
| `qindaqt.qindalutris-compat-db` | every field (including `steamDeck`) loads; every refused fixture is refused whole; absent, symlink, directory, oversize and 100001-game documents; newer-wins; the committed snapshot loads |
| `qindaqt.qindalutris-compat-lookup` | key precedence table, ambiguity withheld, basename and alias matching, avoid reasons, recommended build, build status |
| `qindaqt.qindalutris-compat-generator` | the Python validator on the shared fixtures, each importer on offline copies of the real formats, deterministic offline generation, curated evidence rules |
| `qindaqt.qindalutris-compat-snapshot` | `validate.py` accepts the committed snapshot |
