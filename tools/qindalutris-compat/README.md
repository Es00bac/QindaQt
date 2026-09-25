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
- Verbs are checked against `winetricks list-all` (run with a throwaway
  `WINEPREFIX`, so it never touches `~/.wine`); `--winetricks-list FILE`
  uses a saved listing instead.

## Add a curated fact

Edit `curated.toml`. Curated facts win over every upstream source, so every
one must say where it comes from:

- each `[[games]]` entry, `[builds."<name>"]`, `[[games.proton.avoid]]` and
  `[games.antiCheat]` needs `source`: a URL, or a dated observation that
  someone could repeat ("measured on qinda-top 2026-09-25 ...");
- a PCGamingWiki fact also puts the page in the game's `links`, and uses
  the page URL as `source`;
- winetricks verbs must exist in `winetricks list-all`;
- `PROTONPATH`, `WINEPREFIX`, `GAMEID`, `STORE`, `PATH`, `HOME`, `WINE` and
  `LD_*` are refused: the launch planner owns them.

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
  `winetricks`, `environment`, `dlloverrides` (becomes one
  `WINEDLLOVERRIDES`), `arguments` and `exe`. Lutris variables such as
  `$GAMEDIR`, HUD and shader-cache settings are never imported.

A field the curated entry sets replaces the imported value; notes and links
are added to.

Then regenerate, validate, and run the tests:

```sh
python3 -m unittest discover -s tests -t .
```

(ctest runs the same suite as `qindaqt.qindalutris-compat-generator`, and
`qindaqt.qindalutris-compat-snapshot` validates the committed snapshot.)

## Release flow

1. Regenerate with `--refresh`, review `curated.toml`, and read the diff of
   the snapshot (one game per line).
2. `./validate.py` the snapshot, then build and run
   `ctest -L qindalutris`: the C++ parser must load it too.
3. Commit the snapshot together with any `curated.toml` change.
4. Bump the overlay package `games-util/qindalutris-compat-db-YYYYMMDD`
   (the date of `generated`) to install it with the CMake component
   `QindaLutrisCompatDb` into `/usr/share/qindalutris/`.

The two validators (`qlcompat/schema.py` and the C++ parser) must stay in
step; each carries an `AGENT-CONTRACT` naming the other.
