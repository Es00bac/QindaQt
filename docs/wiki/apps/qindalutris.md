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
  executable itself. The entry always records one Proton build — its name
  and its version — chosen in the dialog; the list starts with
  **Default (<build>)**, preselected (see [Proton builds](#proton-builds)).
  A Proton entry needs a prefix folder (umu creates it on first run if it
  does not exist yet), and cannot be added when no pinnable build is
  installed. Entries saved by older versions of QindaLutris that used
  "Any discovered Proton", or that recorded a build without its version,
  are pinned once to the build they would have used, and the status bar
  says so ("Old Game is now pinned to GE-Proton11-6 (GE-Proton11-6)"). A
  Wine entry switched to Proton in its launch options is pinned the same
  way.
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
build for this game.", "Proton 9.0 has changed since this game was set up
(was proton-9.0-2, now proton-9.0-4). Confirm the new version in
QindaLutris before playing." — instead of failing later.

(The source chips still label the `installed` source "Wine": the chip text
is decided in `LibraryPage.qml`, which has not been updated yet. The
detail panel and list model say "Installed".)

## How Windows games run

Every Windows game that uses Proton — a hand-added entry with the Proton
runner, or an installed title — runs through `umu-run` with:

- `PROTONPATH` set to the **absolute directory of the game's own recorded
  build**, for example
  `/usr/share/steam/compatibilitytools.d/GE-Proton11-6-x86_64`, after
  checking at launch that the build's `proton` file is still there.
  Floating names umu would otherwise accept — `GE-Proton`, `GE-Latest`,
  `UMU-Latest`, `UMU-Proton`, `latest`, or nothing — are refused, never
  passed through, and so are Steam's rolling channels.
- `WINEPREFIX` set to the game's prefix, `GAMEID` to its umu id (else
  `umu-0`), `STORE` to its store (else `none`), and
  `UMU_RUNTIME_UPDATE=0` so the Steam Runtime never updates itself on
  launch.
- The working directory set to the executable's folder.

- `UMU_NO_PROTON`, `RUNTIMEPATH` and `PROTON_VERB` removed from the
  environment the game inherits, because each lets umu run something
  other than the recorded build; and `LD_PRELOAD`, `LD_LIBRARY_PATH`,
  `LD_AUDIT` and every `PYTHON…` variable removed too, because `umu-run`
  is a Python program and those steer its interpreter before anything
  else starts. (If you turn on gamemode, `gamemoderun` still adds its own
  library, as it always does.)

**A game never moves to another build by itself.** If its build is no
longer installed, Play is refused with a sentence naming the build; the
app does not quietly pick a newer one. If the build is still there but its
`version` file changed — Steam updating "Proton 9.0" in place, or a user
replacing a directory — Play is refused with both versions named until
you confirm the new one for that game. That silent move is exactly what
broke World of Warcraft on 2026-09-25 (ADR-0275). Neither a title's saved
environment, your own extra environment, nor the session environment can
set or keep any of these variables; an attempt from the game's settings is
ignored and noted in the status bar, and a saved title that sets one is
refused.

`umu-run` is found in `/usr/bin` first (the `games-util/umu-launcher`
package), then on `PATH`, then in `~/.local/bin`. The plain **Wine** runner
for hand-added entries is unchanged: the Wine loader with `WINEPREFIX`.

## Proton builds

QindaLutris lists every usable Proton build in exactly these directories,
in this order:

1. `/usr/share/steam/compatibilitytools.d` — builds installed by Portage
   (`app-emulation/ge-proton-bin`). The app never removes these.
2. Builds you installed yourself, in each `compatibilitytools.d` of:
   `$XDG_DATA_HOME/Steam` (normally `~/.local/share/Steam`),
   `~/.steam/root`, and the Flatpak Steam's
   `~/.var/app/com.valvesoftware.Steam/.local/share/Steam` and
   `~/.var/app/com.valvesoftware.Steam/data/Steam`.
3. Valve Proton — directories named `Proton*` in `steamapps/common` of
   `~/.steam/root`, `~/.local/share/Steam`, and the two Flatpak Steam
   directories above. (Extra library folders declared in Steam's
   `libraryfolders.vdf` are not searched for Proton yet.)

A build is a directory holding an executable `proton` file. **Its identity
is its directory name plus the first line of its `version` file**
(ADR-0275 section 2), for example `GE-Proton11-6-x86_64` with
`1756415527 GE-Proton11-6`; the `compatibilitytool.vdf` display name is
shown alongside. Every copy is listed with where it came from, even when
two directories share a name; when a Portage copy and a user copy have the
same name *and* version, the Portage copy is the one used.

Some builds are known to the app but can never be recorded for a game
(the add dialog leaves them out):

- Steam's rolling channels — Steam-installed builds whose names contain
  "Experimental", "Hotfix" or "Next", such as `Proton - Experimental` —
  shown as "Updated by Steam — not pinnable";
- builds without a `version` file, shown as "No version file — not
  pinnable".

Not listed at all: a **symlinked** build directory or `proton` file, a
directory named like a floating alias (`GE-Proton`), and any directory
whose name starts with `.` — such as the `.qindalutris-staging-*` and
`.qindalutris-trash` folders a download uses while it runs. A link can be
retargeted, which would move every game pinned to it.

The **default build** for new entries is a preferred build when one is set,
installed and pinnable; else the newest Portage build; else the newest
build you installed yourself; else there is none. Steam's own builds are
never picked as a default. The Proton manager page (choosing the default,
downloading and removing GE builds), the compatibility database that
recommends a build per game, the button that confirms a changed build, and
the "move this game to another build" action are later work; the
controller already offers the confirm step (`confirmProtonBuildForSelected`).

## Installed titles

`~/.config/qindaqt/qindalutris/titles-v1.json` records each title the app
installed or adopted: its store, prefix, **pinned Proton build**, umu id and
store value, executable and arguments, environment, the launcher it is
started through, the winetricks verbs already applied, and the install
date. The pinned build and its version are both required, and the build
must name one concrete build.
Like the other QindaLutris files, the document has an exact schema, is
written atomically, and is refused whole when anything in it is unknown,
out of range, symlinked, too large or from a newer version — the library
then shows no installed titles rather than a guessed subset, and leaves the
file untouched for repair. None of QindaLutris's files is ever written
through a symlink.

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

## Installing games and Proton builds

[ADR-0275](../adr/0275-qindalutris-installs-games-and-manages-pinned-proton.md)
makes QindaLutris an installer as well as a library: one-click store
launchers, any Windows setup file, and GE-Proton builds downloaded and
removed the way Steam manages them, each title pinned to one exact build.
The job machinery, download allowlist, store recipes and failure messages
are described in [QindaLutris installs and Proton builds](qindalutris-installs.md).
Until those jobs are wired into the window, the boundary below still
describes what the shipped application does.

## What QindaLutris deliberately does not do

It does not configure or write to Steam or Lutris, and never touches their
directories. ADR-0275 lets it install Windows games, download GE-Proton
releases and refresh its compatibility database; none of those flows is
built yet, so today it still downloads nothing. It never runs as root and
never invokes Portage. It does not run games through a shell:
every launch is one program and an argument vector, planned the same
bounded way the shell launcher plans desktop entries. And it never turns a
missing game source into an error — an empty machine is a normal machine.
