# ADR-0193: Startup applications route shadows, never edits, system entries

- **Status:** Accepted
- **Date:** 2026-09-18
- **Owners:** Settings Center, Startup applications route
- **Supersedes:** None
- **Superseded by:** None

## Context

PLAN.md's O13 slice asks for a Settings route covering XDG autostart entries
(freedesktop.org Desktop Entry Specification, Autostart appendix): enable or
disable what launches at login, and add a user-defined command. QindaQt ships
no autostart daemon of its own; the surface to manage is a merge of two kinds
of `.desktop` files:

- **System-provided**: `/etc/xdg/autostart` and similar `XDG_CONFIG_DIRS`
  entries, installed by packages QindaQt does not own.
- **User**: `$XDG_CONFIG_HOME/autostart` (usually `~/.config/autostart`).

A user directory entry shadows a system entry of the same basename entirely;
this is how every XDG-conformant desktop lets a user turn off a
package-installed autostart entry without needing write access to `/etc`.

## Decision

The route never writes to a system autostart directory. Three operations,
one store (`XdgAutostartStore`):

1. **List**: merge by basename, user directory first, so a user override is
   the one presented. An entry is enabled unless its `[Desktop Entry]` group
   has `Hidden=true` or `X-GNOME-Autostart-enabled=false` (both are checked
   for interoperability with entries another desktop environment's tools may
   have written). A file with no `[Desktop Entry]` group or no
   `Type=Application` is not shown at all -- fail closed rather than present
   a row for something not actually an autostart application.
2. **Enable/disable**: if a user override already exists, patch its
   `Hidden=` line in place. Otherwise, read the *system* file's raw text
   (whichever `XDG_CONFIG_DIRS` entry has it) and write it verbatim to the
   user directory with `Hidden=` added or replaced -- every other line,
   including one this route does not understand, survives untouched. There
   is no "restore to system default" action that removes the override; the
   override with `Hidden=false` is functionally equivalent and simpler to
   reason about (one write path, not two).
3. **Add a command / remove**: a route-created entry gets
   `X-QindaQt-Custom=true` in its file. Only an entry carrying that marker
   can be removed outright; every other entry (including one this route
   itself disabled) can only ever be hidden, never deleted, because deleting
   a real application's autostart file is not undone by reinstalling the
   application.

`Exec=` is stored and read back verbatim. This route neither shell-quotes,
shell-expands, nor validates the command beyond refusing embedded newlines
(which would corrupt the file's line structure) -- the same trust boundary
QindaQt already applies to a user's custom global-shortcut command
(`~/.local/share/kglobalaccel/qindaqt-custom-<name>.desktop`, see
[Settings Input route](../apps/input-settings.md)).

Desktop Entry files are parsed and patched with a bespoke line scanner
(`readDesktopEntry`/`withHiddenSetTo` in `xdg_autostart_store.cpp`), not
`QSettings::IniFormat`: this route only ever reads five keys and writes one,
and a full INI round-trip risks reformatting or reordering the rest of a
file it does not own the shape of. `screen_lock_settings.cpp`'s adapter and
`src/session/sessiondefaults.cpp`'s `seedMissingDirectoryHandler` establish
the same "read what you need, patch one line, preserve the rest verbatim"
pattern for other local `.desktop`/INI-shaped files this codebase touches.

## Consequences

- A user can turn off any autostart entry, including ones installed by
  packages, without QindaQt ever writing outside its own config directory.
- No live daemon is restarted or reloaded by this route: autostart entries
  are read once at the next login, matching every other desktop
  environment's autostart contract. The route's "enabled" truth may
  therefore lag what is actually running until the next session.
- Locale-suffixed keys (`Name[fr]=`, `Comment[de]=`, ...), `TryExec=`,
  `OnlyShowIn=`/`NotShowIn=`, and `X-GNOME-Autostart-Phase=` are not read or
  honored; an entry gated on one of those is shown as if unconditional. Out
  of scope for this slice.

## Revisit when

A future route needs to write more than one key to a `.desktop`-shaped file
this route also touches (risk of two independent line-patchers disagreeing
on formatting), or the locale/condition keys above become a real user
complaint.
