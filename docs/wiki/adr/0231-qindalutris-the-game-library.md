# ADR-0231: QindaLutris is the desktop's game library

- **Status:** Proposed
- **Date:** 2026-09-21
- **Owners:** Bundled applications
- **Supersedes:** None
- **Superseded by:** None

## Context

The operator asked for "Steam, Lutris, something like Lutris, but native
QindaQt/QindaTk" — one window that lists every game on the machine and
launches it. The desktop already knows about games in three places: the
compositor reads Steam's `libraryfolders.vdf` and `appmanifest_*.acf` to name
windows and extracts icons from Windows executables (ADR-0230), the shell
launcher scans `.desktop` entries and plans their execution behind bounded
seams (ADR-0042, ADR-0062), and the operator's machine runs a real Lutris
whose library lives in a SQLite database at
`~/.local/share/lutris/pga.db`.

None of Steam, Lutris, Wine or Proton is guaranteed to exist on a given
machine, and every one of their data sources is user-owned, mutable, and
authored outside this repository — the same hostile-input posture as
ADR-0230's parsers. The launcher also asked for two things Lutris itself
does not give him: per-game launch options that persist (gamemode, MangoHud,
extra environment), and an explicit choice of which display a game opens on
in a multi-display setup.

## Decision

**QindaLutris** (`gui-apps/qindalutris`, sources `src/apps/qindalutris/`,
binary `qindalutris`) is the desktop's game library. The retired draft name
"QindaPlay" must not appear anywhere.

**A pure model first.** The model merges four independent, individually
optional sources into one sorted list of games — stable id, title, source,
install path, cover path, and what it takes to launch:

1. **Steam** — injected candidate install roots; each root's
   `steamapps/libraryfolders.vdf` declares library roots whose
   `steamapps/appmanifest_*.acf` become games. The VDF/ACF parsing is
   **linked, not copied**, from the compositor's public bounded readers
   (`QindaQt::CompositorShellActions`, ADR-0230); this adapter owns only the
   capped filesystem walk. The appid comes from the manifest filename. The
   exact per-game install directory and `SizeOnDisk` live behind manifest
   keys no shared parser exposes today, so Steam games honestly report the
   library folder as location and "not tracked" as size rather than
   re-parsing — a parser addition belongs to the compositor module.
2. **Lutris** — `pga.db` opened through SQLite's `immutable=1` read-only URI
   plus `QSQLITE_OPEN_READONLY`: never a write, never journal recovery,
   never a `-wal`/`-shm` file, never the `lutris` binary or its Python.
   Only `installed = 1` rows. The schema is gated on the exact column set
   first; a missing file, locked file, unrecognised schema, or null-name row
   degrades to fewer games and a status-bar note. Cover art comes from the
   database's sibling `coverart/` and `banners/` directories by fixed
   `<slug>.jpg` name only.
3. **Native Linux games** — the `Categories=Game` slice of the shared
   apps-side `QindaQt::ApplicationCatalog` scan (injected XDG roots, the
   documented apps ← shell-launcher-L0 direction). Launch argv is planned
   from the retained, validated document text by the same catalog's
   `planApplicationLaunch`; terminal-only and D-Bus-only entries are
   reported not-launchable with a reason.
4. **Hand-added Wine/Proton titles** — a `.exe`, a prefix, and a runner,
   persisted app-locally (below). The cover is extracted by the compositor's
   public bounded PE-icon parser, again linked; these are the only games
   whose size is honestly knowable (the executable's byte size), so they are
   the only ones that show one.

**De-duplication:** a Lutris row whose normalized title (case-folded,
punctuation-stripped) matches a Steam game's folds into the **Steam**
record — it carries the honest install anchor and needs no extra runner.
Two distinct Steam appids sharing a title both survive; only the Lutris
copy ever drops.

**Launching never touches a shell.** A plan is one program, one argv vector,
one set of typed environment overlays: `steam steam://rungameid/<id>`,
`lutris lutris:rungameid/<id>`, the catalog-planned desktop argv, or
`wine`/`proton run` for manual entries. Proton requires a prefix and gets
`STEAM_COMPAT_DATA_PATH`; Wine gets `WINEPREFIX`. Process start sits behind
a `GameProcessLauncher` seam in the posture of ADR-0062 — tests inject a
recording fake, production uses `QProcess::startDetached` with the session
environment plus overlays. Unavailable ingredients degrade to a typed
reason on the Play button or a status-bar note, never a guess and never a
dialog.

**Per-game options persist app-locally**, in `launch-options-v1.json` and
`wine-entries-v1.json` under `$XDG_CONFIG_HOME/qindaqt/qindalutris`, on the
ADR-0198 precedent: the key space is open-ended (one entry per discovered
game id), and Settings1 rejects a whole snapshot on one unknown key — a
fragility per-game tuning must not share. Both documents are exact-schema,
size-bounded, symlink-refusing, atomically committed, and refused **whole**
on any out-of-set value, leaving defaults standing.

**The target display is an explicit per-game choice.** Outputs are
enumerated through `QScreen` — the public platform seam an application may
use; the compositor is never reached. The choice persists as the connector
name and is applied at launch as `SDL_VIDEO_FULLSCREEN_DISPLAY=<index>`.
That is advisory, SDL-flavoured, and honoured by Steam/Lutris child
processes through environment inheritance; Wayland has no mandatory
protocol for placing a foreign window on an output, so a saved-but-absent
display degrades to a note and the compositor default.

**The UI is QML on QindaTK**, the System Monitor's contract (ADR-0220):
search field, source chips, a cover grid, a detail panel with an honest
Play button, and `Tk.EmptyState` for both "no games" and "no match". One
exception is recorded in code: `Tk.Thumbnail` exists only in the toolkit's
source tree, not in the installed `dev-libs/qindatk-0.1.0-r4`, so the cover
tile is a local part on `Tk.Theme` tokens implementing the same contract,
to be swapped back when the toolkit ships it.

## Consequences

- Focused rows prove the contract against synthetic fixtures: two-root and
  malformed Steam manifests, a test-built `pga.db` (installed filter, null
  name, missing column, non-database), `Categories=Game` desktop entries,
  the de-dup rule, the store's round-trips and whole-refusals, and the
  launch planner's argv/env shapes including every degradation. A QML test
  constructs the page over a stub model with a window parent chain and
  positive size, and a `--grab` row renders the real application offscreen.
- The application reads but never writes Steam's and Lutris's data; the
  operator's real Lutris keeps working exactly as it does now.
- The Steam manifest parser addition that would unlock per-game install
  directories and sizes is flagged to the compositor module owner instead
  of being duplicated here.
- A shared atomic app-local state primitive (today each app writes its own;
  File Manager's `StateFile` is module-private) is flagged as a candidate
  future extraction rather than copied.

## Revisit when

- QindaTK ships `Tk.Thumbnail` (and its theme roles) in an installed
  package — swap the local cover tile back to the toolkit control.
- The compositor module grows a parser for a manifest's `installdir` /
  `SizeOnDisk` — consume it and drop the honest "not tracked".
- A mandatory per-output window-placement protocol for foreign windows
  exists (Wayland or desktop-wide) — replace the SDL advisory pin.
