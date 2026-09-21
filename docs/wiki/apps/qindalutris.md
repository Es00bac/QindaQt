# QindaLutris

QindaLutris is the desktop's game library: every game on the machine in one
native QindaQt window, and the button that starts it. It belongs alongside
the bundled monitor, editor, terminal and file manager, and its decisions
are recorded in [ADR-0231](../adr/0231-qindalutris-the-game-library.md).

## Where the games come from

The library merges up to four sources. Every source is optional; a machine
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
  Wine, or any Proton installation discovered under your Steam roots. The
  game's cover is the icon extracted from the executable itself.

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
installed", "Proton needs a prefix directory" — instead of failing later.

## Launch options

Every game has its own options, kept between sessions: which **display**
the game opens on, **gamemode**, **MangoHud**, and **extra environment**
variables (`KEY=VALUE`, one per line; invalid lines are ignored on save).
Windows games additionally let you switch runner (Wine ↔ Proton) and edit
the prefix after the fact.

The display choice is explicit because the operator runs multiple displays:
pick a display and the game is asked to open there. This uses the SDL
display-index convention, which games and their launchers honour through
environment inheritance; Wayland has no mandatory protocol for placing a
window on a specific output, so when the saved display is unplugged the
game opens on the compositor's default and the status bar says so. gamemode
and MangoHud degrade the same honest way — requested but not installed
means the game still launches, with a note in the status bar.

Options are stored app-locally under
`~/.config/qindaqt/qindalutris/` (launch-options-v1.json and
wine-entries-v1.json), in the same exact-schema, atomically written style
as the File Manager's preferences. A corrupt or newer-version file is
refused whole and defaults apply; nothing half-read ever takes effect.

## What QindaLutris deliberately does not do

It does not install, configure, or write to Steam, Lutris, Wine or Proton,
and never touches their directories. It does not download anything — no
store, no artwork fetch, no account. It does not run games through a shell:
every launch is one program and an argument vector, planned the same
bounded way the shell launcher plans desktop entries. And it never turns a
missing game source into an error — an empty machine is a normal machine.
