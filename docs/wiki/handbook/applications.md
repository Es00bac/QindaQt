# Applications

QindaQt ships a small, focused set of first-party applications — enough for
everyday text, files, and terminal work, each doing its job well rather than
a suite doing everything. They are ordinary desktop applications: they follow
your theme and fonts, share the same controls, and behave the same whether or
not their menus appear in the top panel.

## Welcome guide

The first time the session starts, the Welcome guide opens by itself: seven
short chapters on the desktop, the launcher and dock, grouping windows with
`Meta+Shift` drag, pages and splits, detaching and the group menu,
customization, and appearance. Buttons in the guide open the real Settings,
Text Editor, and File Manager so you can try things as you read.

Its **Show at next launch** checkbox is on by default; turn it off and it
won't open automatically again — you can always reopen it from the launcher.

## System Settings

Open System Settings from the system menu (the left button of the top panel)
or from the launcher. A sidebar groups the pages:

- **General**
  - *Notifications* — banner behavior and Do Not Disturb.
- **Personalization**
  - *Appearance* — themes, wallpaper, and fonts; see
    [Making it yours](customization.md).
  - *Customize* — panels, applets, and layout presets; see
    [Making it yours](customization.md).
  - *Clipboard* — turn clipboard history on or off, see how full it is, and
    clear it.
- **Hardware**
  - *Display* — screens, resolution, refresh rate, and scaling, with a live
    preview you confirm or cancel before anything changes.
  - *Network* — see available networks, connect, and manage known
    connections; password entry happens in its own secure prompt.
  - *Audio* — output and input devices, per-stream volume, mute, and default
    device.
  - *Bluetooth* — turn the adapter on, discover devices, pair (confirming
    the standard passkey prompt), connect or disconnect, trust, or forget.
  - *Power* — battery and supply state, power profile, and screen and
    keyboard brightness.
  - *Color* — color-profile management per screen.

Keyboard shortcuts: `Ctrl+1` through `Ctrl+0` jump straight to the pages in
order, the arrow keys move through them, and `Alt+Left` returns to the
previous page. When a service is missing or not running, its page says so
plainly instead of showing dead controls.

## Text Editor

A plain-text editor for local files, handling up to 32 documents in tabs. It
keeps each file's existing line endings and encoding intact, saves
atomically, and warns you if a file changes on disk underneath you — a
conflict never silently overwrites your work. Find and replace covers plain
text and regular expressions, with Replace All undoable as one step. If you
close with files open, the editor offers to remember *which* files to reopen
next time (never unsaved contents).

Line numbers, automatic syntax highlighting, and `Ctrl+G` to jump to a line
help with scripts and configuration files. Tab and Shift+Tab indent selected
lines; Enter carries indentation forward. The View menu gives each tab its own
word-wrap switch and text zoom (`Ctrl++`, `Ctrl+-`, and `Ctrl+0` to reset).

## File Manager

Browse folders with a path bar and back/forward history, open files with
their proper applications, and organize with rename, copy, move, and the
Trash. It works on local files; remote filesystems are not part of the
current version. Use `Ctrl+L` to type a path, `Ctrl+H` to show hidden files,
and `Ctrl+D` to bookmark a folder. List and grid views share your selection;
Ctrl-click picks individual files and Shift-click selects a range. Click a
column heading to sort. Copy, move, and Trash work on the selected files together.

## Terminal

A real terminal for your shell, with up to eight sessions in tabs, saved
profiles, searchable scrollback, and clickable links that open only after
you confirm them. Closing a tab keeps the exit status honest — a crashed
command reports the crash rather than vanishing.

New tabs open in the active shell's current folder. Adjust a tab's text size
from View, or use `Ctrl+Shift++` / `Ctrl+Shift+-`; `Ctrl+Shift+0` restores the
profile size. Theme changes preserve that zoom and the terminal font.

## Menus in the top panel

The Text Editor, Terminal, and File Manager put their menus in the top
panel while they are focused, so the window spends its space on your work.
Applications without that integration simply keep their own menu. See
[Using the desktop](desktop.md#the-application-menu).

## Not included (yet)

An image viewer, archive manager, system monitor, and software center appear
in the project's long-term plans. They are not shipped applications today —
use ordinary Linux applications for those needs. The honest per-feature
ledger is the [feature catalog](catalog/features.md).

Each application's full contract — limits, shortcuts, recovery behavior — is
documented on its own page: [Welcome](../apps/welcome.md), [Settings
Center](../apps/settings-center.md) and each route page, [Text
Editor](../apps/text-editor.md), [File Manager](../apps/file-manager.md),
and [Terminal](../apps/terminal.md). Return to the [handbook index](index.md).
