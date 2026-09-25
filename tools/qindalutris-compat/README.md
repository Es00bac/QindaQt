# QindaLutris compatibility-database generator

Produces `compat-db-v1.json`, the document QindaLutris reads to pin Proton
builds and apply per-game fixes (ADR-0275 §3). The schema and the lookup
rules are documented in `docs/wiki/apps/qindalutris-compat-db.md`.

Python 3.11 or newer, standard library only.

## Refresh the snapshot

```sh
cd tools/qindalutris-compat
# Fetch everything fresh (about ten minutes: ProtonDB and the Steam Deck
# report are each asked politely, one request per second per host, for at
# most --protondb-limit games).
./generate.py --refresh --protondb-limit 300 \
    --out ../../src/apps/qindalutris/compat/compat-db-v1.json
./validate.py ../../src/apps/qindalutris/compat/compat-db-v1.json
```

- Fetches are cached in `--cache-dir` (default
  `$XDG_CACHE_HOME/qindalutris-compat`), each with its URL, HTTP status and
  retrieval time. Without `--refresh` a cached copy is reused.
- `--offline` never touches the network and builds from the cache alone.
  A source with no cached copy is left out, and so is its `sources` entry.
- `--generated 2026-09-25T00:00:00Z` fixes the document stamp. With the same
  cache, `curated.toml` and stamp the output is byte-identical.
- Requests to one host are at least a second apart; a 429 or 503 answer is
  retried after its `Retry-After` (capped at two minutes, four attempts),
  then the cached copy is used.
- Every warning (a dropped verb, a withheld Steam appid, an unreadable
  report) is printed as it happens and counted on the final line.
- Verbs are checked against the committed `winetricks-verbs.txt`, never a
  live listing, so both validators agree.

## Refresh the winetricks verb allowlist

```sh
./generate.py --update-winetricks-verbs [--winetricks /usr/bin/winetricks]
```

This runs `winetricks list-all` with a throwaway `WINEPREFIX` (it never
touches `~/.wine`), keeps the `dlls`, `fonts` and `settings` categories,
drops the verbs that must never be advice (a leading `-`, `annihilate`,
`prefix=`, `arch=`, `list*`, `bad`, `good`, `set_userpath`,
`set_mididevice`, `winver=`, `mimeassoc=on`, `remove_mono`), and records the winetricks version and date
in the header. Commit the file with the snapshot it validated; re-run
CMake, which regenerates the C++ copy from it.

## Add a curated fact

Edit `curated.toml`. Curated facts win over every upstream source, so every
one must say where it comes from:

- each `[[games]]` entry, `[builds."<name>"]`, `[[games.proton.avoid]]` and
  `[games.antiCheat]` needs `source`: a URL, or a dated observation that
  someone could repeat ("measured on qinda-top 2026-09-25 ...");
- a PCGamingWiki fact also puts the page in the game's `links`, and uses
  the page URL as `source`;
- winetricks verbs must be in `winetricks-verbs.txt`;
- environment keys must be on the schema's allowlist of exact names (see
  the wiki page): `WINEDLLOVERRIDES`, listed value-only `DXVK_*`,
  `VKD3D_*`, `PROTON_*` and `__GL_*` switches, and a few Wine/Mesa settings
  -- anything else refuses the whole document;
- `[games.proton] recommended` must name a build listed here with status
  `tested`; an untested build can be described and avoided, never pinned.

A game table accepts `id`, `title`, `source`, `umuStore`, `keys` (as in the
schema), `environment`, `winetricks`, `arguments`, `notes`, `links`,
`[games.proton]` (`recommended`, `[[games.proton.avoid]]`),
`[games.antiCheat]` (`status`, `notes`, `source`), and two join helpers:

- `match = { umuId = "umu-…", awacy = "<AreWeAntiCheatYet slug>" }` pulls
  those upstream records into this game (so do the game's own umu id, Steam
  appids and store ids);
- `lutris = { slug = "<lutris game slug>", installer = "<installer slug>",
  take = [...] }` reads one published Wine installer from
  `https://lutris.net/api/installers/<slug>`. `take` chooses among
  `winetricks`, `environment` and `dlloverrides` (becomes one
  `WINEDLLOVERRIDES`). Lutris scripts carry no license, so only these
  uncopyrightable facts are taken (ADR-0275 §7) and the game's Lutris page
  is linked as attribution; arguments and executable names are written in
  the curated entry itself with their own evidence. Lutris variables such
  as `$GAMEDIR`, HUD and shader-cache settings and non-allowlisted keys are
  never imported.

A field the curated entry sets replaces the imported value; notes and links
are added to.

Then regenerate, validate, and run the tests:

```sh
python3 -m unittest discover -s tests -t .
```

(ctest runs the same suite as `qindaqt.qindalutris-compat-generator`;
`qindaqt.qindalutris-compat-snapshot` validates the committed snapshot and
`qindaqt.qindalutris-compat-differential` judges 172 mutation cases with
both validators.)

## Release flow

1. Optionally refresh `winetricks-verbs.txt`, then regenerate with
   `--refresh`, review `curated.toml` (including its TODOs), read the
   warnings, and read the diff of the snapshot (one game per line).
2. `./validate.py` the snapshot, then build and run
   `ctest -L qindalutris`: the C++ parser must load it too.
3. Commit the snapshot together with any `curated.toml` change.
4. Bump the overlay package `games-util/qindalutris-compat-db-YYYYMMDD`
   (the date of `generated`) to install it with the CMake component
   `QindaLutrisCompatDb` into `/usr/share/qindalutris/`.

The two validators (`qlcompat/schema.py` and the C++ parser) must stay in
step; each carries an `AGENT-CONTRACT` naming the other.
