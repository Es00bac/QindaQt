# QindaLutris

QindaLutris is the desktop's game library: every game on the machine in one
native QindaQt window, and the button that starts it. It belongs alongside
the bundled monitor, editor, terminal and file manager, and its decisions
are recorded in [ADR-0231](../adr/0231-qindalutris-the-game-library.md) and
[ADR-0275](../adr/0275-qindalutris-installs-games-and-manages-pinned-proton.md),
which makes every Windows game run on one exact, recorded Proton build.

## Where the games come from

The library merges up to five sources. Every source is optional; a machine
with none of them simply shows the empty state.

- **Steam.** Each Steam install's `steamapps/libraryfolders.vdf` declares
  its library folders, and every `appmanifest_*.acf` in them is one game.
  Steam games launch through `steam://rungameid/<id>`. Covers come from the
  install's own `appcache/librarycache/`.
- **Lutris.** The installed Lutris's own library database
  (`~/.local/share/lutris/pga.db`) is read **strictly read-only** — opened
  with SQLite's immutable flag so QindaLutris can never write to it, and
  Lutris keeps working exactly as it does now. Only games marked installed
  appear. They launch through `lutris:rungameid/<id>`, and their cover art
  comes from Lutris's own `coverart/` and `banners/` folders.
- **Native Linux games.** Installed `.desktop` entries whose categories
  include `Game`, found through the same bounded scanner the shell launcher
  uses.
- **Windows games you add yourself.** **File → Add Windows game**
  (`Ctrl+N`) takes a title, the `.exe`, a Wine prefix, and a runner —
  Wine or Proton. The game's cover is the icon extracted from the
  executable itself. The entry always records one Proton build by name:
  the one you picked, or else the current default build (see
  [Proton builds](#proton-builds)). If no Proton build is installed at all,
  a Proton entry cannot be added.
- **Installed Windows games.** Titles QindaLutris installed or adopted are
  kept in `titles-v1.json` (see [Installed titles](#installed-titles)) and
  appear under the source id `installed`. The install and adopt flows that
  write these records are not built yet; today the library lists and
  launches whatever records the file holds.

A game that both Steam and Lutris know appears once, as the Steam entry.

## The window

The cover grid is the library. Type in the search field to narrow by title,
or use the **All / Steam / Lutris / Native / Wine** chips to show one
source. Select a game for its detail panel: cover, source, location, and —
where it is honestly knowable — install size. (For Steam and Lutris games
that number is "not tracked": QindaLutris shows only sizes it can stand
behind, not directory-walk guesses.)

**Play** (or double-click a cover) launches the game. When Play is
disabled, the button and the text beneath it say why — "Steam is not
installed", "umu is not installed. Install games-util/umu-launcher.",
"GE-Proton11-6 is not installed. Reinstall it or choose another Proton
build for this game." — instead of failing later.

(The source chips still label the `installed` source "Wine": the chip text
is decided in QML, which this slice did not change. The detail panel and
list model say "Installed".)

## How Windows games run

Every Windows game that uses Proton — a hand-added entry with the Proton
runner, or an installed title — runs through `umu-run` with:

- `PROTONPATH` set to the **absolute directory of the game's own recorded
  build**, for example
  `/usr/share/steam/compatibilitytools.d/GE-Proton11-6-x86_64`. Floating
  names umu would otherwise accept — `GE-Proton`, `GE-Latest`,
  `UMU-Latest`, `UMU-Proton`, `latest`, or nothing — are refused, never
  passed through.
- `WINEPREFIX` set to the game's prefix, `GAMEID` to its umu id (else
  `umu-0`), `STORE` to its store (else `none`), and
  `UMU_RUNTIME_UPDATE=0` so the Steam Runtime never updates itself on
  launch.
- The working directory set to the executable's folder.

**A game never moves to another build by itself.** If its build is no
longer installed, Play is refused with a sentence naming the build; the
app does not quietly pick a newer one. That silent move is exactly what
broke World of Warcraft on 2026-09-25 (ADR-0275). Neither a title's saved
environment nor your own extra environment can override these five
variables; an attempt is ignored and noted in the status bar.

`umu-run` is found in `/usr/bin` first (the `games-util/umu-launcher`
package), then on `PATH`, then in `~/.local/bin`. The plain **Wine** runner
for hand-added entries is unchanged: the Wine loader with `WINEPREFIX`.

## Proton builds

QindaLutris lists every usable Proton build it finds, in this order:

1. `/usr/share/steam/compatibilitytools.d` — builds installed by Portage
   (`app-emulation/ge-proton-bin`). The app never removes these.
2. `$XDG_DATA_HOME/Steam/compatibilitytools.d` (normally
   `~/.local/share/Steam/…`) and `~/.steam/root/compatibilitytools.d` —
   builds you installed yourself.
3. Valve Proton (`steamapps/common/Proton*`) under your Steam libraries.

A build is a directory holding an executable `proton` file; its identity is
the directory name, with the `version` file and `compatibilitytool.vdf`
display name shown alongside. When two roots hold the same name, the
earlier root wins, so a Portage build beats a user copy. A **symlinked**
build directory, or one named like a floating alias (`GE-Proton`), is not
listed: a link can be retargeted, which would move every game pinned to it.

The **default build** for new entries is a preferred build when one is set
and installed, else the first Portage build, else the first build found.
The Proton manager page (choosing the default, downloading and removing GE
builds), the compatibility database that recommends a build per game, and
the "move this game to another build" action are later work.

## Installed titles

`~/.config/qindaqt/qindalutris/titles-v1.json` records each title the app
installed or adopted: its store, prefix, **pinned Proton build**, umu id and
store value, executable and arguments, environment, the launcher it is
started through, the winetricks verbs already applied, and the install
date. The pinned build is required and must name one concrete build.
Like the other QindaLutris files, the document has an exact schema, is
written atomically, and is refused whole when anything in it is unknown,
out of range, symlinked, too large or from a newer version — the library
then shows no installed titles rather than a guessed subset, and leaves the
file untouched for repair.

## Launch options

Every game has its own options, kept between sessions: which **display**
the game opens on, **gamemode**, **MangoHud**, and **extra environment**
variables (`KEY=VALUE`, one per line; invalid lines are ignored on save).
Hand-added Windows games additionally let you switch runner (Wine ↔
Proton) and edit the prefix after the fact; the recorded Proton build stays
the same. Installed titles keep their recorded prefix and build.

The display choice is explicit because the operator runs multiple displays:
pick a display and the game is asked to open there. This uses the SDL
display-index convention, which games and their launchers honour through
environment inheritance; Wayland has no mandatory protocol for placing a
window on a specific output, so when the saved display is unplugged the
game opens on the compositor's default and the status bar says so. gamemode
and MangoHud degrade the same honest way — requested but not installed
means the game still launches, with a note in the status bar.

Options are stored app-locally under
`~/.config/qindaqt/qindalutris/` (launch-options-v1.json,
wine-entries-v1.json and titles-v1.json), in the same exact-schema,
atomically written style
as the File Manager's preferences. A corrupt or newer-version file is
refused whole and defaults apply; nothing half-read ever takes effect.

## What QindaLutris deliberately does not do

It does not configure or write to Steam or Lutris, and never touches their
directories. ADR-0275 lets it install Windows games, download GE-Proton
releases and refresh its compatibility database; none of those flows is
built yet, so today it still downloads nothing. It never runs as root and
never invokes Portage. It does not run games through a shell:
every launch is one program and an argument vector, planned the same
bounded way the shell launcher plans desktop entries. And it never turns a
missing game source into an error — an empty machine is a normal machine.
