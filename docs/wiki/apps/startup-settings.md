# QindaQt Settings — Startup applications route

`qindaqt-settings --page startup` chooses what launches at login: enable or
disable an installed application's autostart entry, add a custom command,
and remove a command this route itself added. The design and the write
boundary are [ADR-0193](../adr/0193-startup-applications-route.md).

## What the route shows

One list, merged from `$XDG_CONFIG_HOME/autostart` (usually
`~/.config/autostart`) and every `XDG_CONFIG_DIRS/autostart` directory
(typically `/etc/xdg/autostart`), by basename — a user entry shadows a
system entry of the same name, per the freedesktop.org Autostart
specification. Each row shows the entry's `Name=`, `Comment=` (when
present), whether it runs at login, and a Remove action that only appears
for an entry this route itself created.

A file that has no `[Desktop Entry]` group, or never declares
`Type=Application`, is not shown — this route does not guess.

## Authority and write boundary

There is no D-Bus service or resident daemon behind this route; the
authority is the filesystem itself, read and written directly by
`XdgAutostartStore`. Every mutation is synchronous and either succeeds and
refreshes the list, or fails and leaves the previous list showing with
`errorText` set.

- **Enable/disable** never edits a system (package-owned) `.desktop` file.
  Disabling one for the first time copies its exact text into the user
  directory with `Hidden=` added, preserving every other line. Toggling
  after that patches the user copy's `Hidden=` line in place.
- **Add a command** creates `~/.config/autostart/qindaqt-custom-<slug>.desktop`
  with `X-QindaQt-Custom=true`, disambiguating the filename with a numeric
  suffix if the slug is already taken.
- **Remove** only ever deletes a file carrying that marker. Every other
  entry — including one this route disabled — is never deleted, only ever
  hidden.

`Exec=` is stored and displayed verbatim; the route does not interpret,
shell-expand, or validate it beyond refusing an embedded newline.

## What this route does not claim

- It does not know whether a currently-running session already launched (or
  didn't launch) a given entry — autostart runs once at login, so a toggle's
  effect is only visible next session, same as every other desktop
  environment's autostart surface.
- Locale-suffixed keys (`Name[fr]=`), `TryExec=`, `OnlyShowIn=`/`NotShowIn=`,
  and `X-GNOME-Autostart-Phase=` are not read; an entry gated by one of
  those shows as if unconditional.
- No default applications, network locations, or notification schedule
  controls live here — see the other Settings routes and
  `docs/wiki/reference/settings-completeness.md` for what is and isn't
  covered elsewhere.

## Verification

    ctest --test-dir build/dev --output-on-failure \
      -R '^qindaqt\.settings-(startup-(store|model)|route-registry|navigation-controller)$'

- `qindaqt.settings-startup-store`: the merge-by-basename shadow rule, that
  disabling a system entry never touches the system file and preserves an
  unrelated key, that re-enabling after an override patches rather than
  restores-by-deletion, that a `Hidden=true` user-only entry patches in
  place, that a file without `Type=Application` is skipped, custom-entry
  creation (including id de-duplication and empty-field refusal), and that
  removal is refused for a non-custom entry.
- `qindaqt.settings-startup-model`: the QML-facing projection and that a
  failed mutation reports `errorText` without discarding the previously
  loaded list.
- `qindaqt.settings-route-registry` / `qindaqt.settings-navigation-controller`:
  the `startup` route's registration, position (13th and last built-in
  route), and keyboard/index navigation reaching it.

Not yet covered: an offscreen page-level test for `StartupPage.qml` itself
(the pattern other routes use, e.g. `qindaqt.settings-audio-page`) and a
Settings Center installed-route/boundary-poison pair (matching
`qindaqt.settings-audio-boundary`). Both are natural follow-ups within the
same shape other routes already use; not added in this slice for time.
