# QindaQt Settings — Startup applications route

`qindaqt-settings --page startup` chooses what launches at login: enable or
disable an installed application's autostart entry, add a custom command,
and remove a command this route itself added. The original user-file write
boundary is [ADR-0214](../adr/0214-startup-applications-route.md);
the session execution contract is [ADR-0247](../adr/0247-run-xdg-autostart-in-the-session-supervisor.md).

## What the route shows

One list, merged from `$XDG_CONFIG_HOME/autostart` (usually
`~/.config/autostart`) and every `XDG_CONFIG_DIRS/autostart` directory
(typically `/etc/xdg/autostart`), by basename — a user entry shadows a
system entry of the same name, per the freedesktop.org Autostart
specification. Each row shows the entry's `Name=`, `Comment=` (when
present), whether it is enabled for the next login, an explanation when it
cannot run, and a Remove action only for an entry this route created.

A file that has no `[Desktop Entry]` group, or never declares
`Type=Application`, is not shown — this route does not guess. The same
[shared catalog](../architecture/session-autostart.md) decides Settings
eligibility and what the session launches. Scalar desktop-entry escapes in
`Name=`, `Comment=`, and `Icon=` are decoded for display; malformed escapes
leave a visible but ineligible row.

## Authority and write boundary

There is no D-Bus service or resident daemon behind this route; the
authority is the filesystem itself, read and written directly by
`XdgAutostartStore`. Every mutation is synchronous and either succeeds and
refreshes the list, or fails and leaves the previous list showing with
`errorText` set.

- **Enable/disable** never edits a system (package-owned) `.desktop` file.
  Disabling one for the first time copies its exact text into the user
  directory with `Hidden=` added, preserving every other line. Toggling
  after that patches the user copy's `Hidden=` line in place. Re-enabling
  clears every recognized GNOME autostart disable flag too.
- **Add a command** creates `~/.config/autostart/qindaqt-custom-<slug>.desktop`
  with `X-QindaQt-Custom=true`, disambiguating the filename with a numeric
  suffix if the slug is already taken.
- **Remove** only ever deletes a file carrying that marker. Every other
  entry — including one this route disabled — is never deleted, only ever
  hidden.

`Exec=` is stored and displayed verbatim; the route does not interpret,
shell-expand, or validate it beyond refusing an embedded newline.

## Session execution and limits

The supervisor scans once after the session shell and optional children
start. Eligible commands run as bounded desktop-entry argv, never through a
shell. A user entry shadows a same-name system entry before conditions are
checked. OnlyShowIn, NotShowIn, TryExec, Hidden, and the recognized GNOME
disable flag affect eligibility. The switch shows the requested enabled
state for the next login; a reason below the row explains a missing
executable, desktop mismatch, unsupported activation or startup phase, or
other condition. Toggling does not start or stop a process mid-session.

D-Bus-activatable entries and non-Application GNOME startup phases are
currently diagnosed as unsupported. Locale-suffixed display names are not
selected, and descendants that daemonize away from a direct parent are not
adopted. See [session autostart](../architecture/session-autostart.md).

## Verification

    ctest --test-dir build/dev --output-on-failure \
      -R '^(qindaqt\.settings-(startup-(store|model|page)|route-registry|navigation-controller)|qindaqt\.session-autostart-(catalog|lifetime))$'

- `qindaqt.settings-startup-store`: the merge-by-basename shadow rule, that
  disabling a system entry never touches the system file and preserves an
  unrelated key, that re-enabling after an override patches rather than
  restores-by-deletion, that a `Hidden=true` user-only entry patches in
  place, that a file without `Type=Application` is skipped, custom-entry
  creation (including id de-duplication and empty-field refusal), and that
  removal is refused for a non-custom entry.
- `qindaqt.session-autostart-catalog` / `qindaqt.session-autostart-lifetime`:
  shared eligibility, a private login marker once across shell recovery,
  filtered entries never run, stop before the first event-loop turn cancels
  launch, and logout terminates the direct child.
- `qindaqt.settings-startup-page`: the real QML renders the ineligibility
  reason and dispatches a mouse switch to the model.
- `qindaqt.settings-startup-model`: the QML-facing projection and that a
  failed mutation reports `errorText` without discarding the previously
  loaded list.
- `qindaqt.settings-route-registry` / `qindaqt.settings-navigation-controller`:
  the `startup` route's registration, position (13th and last built-in
  route), and keyboard/index navigation reaching it.

The Settings Center offscreen route-construction and staged installed-route
checks now include Startup. A dedicated boundary-poison test remains open.
