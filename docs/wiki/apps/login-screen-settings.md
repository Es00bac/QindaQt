# QindaQt Settings — Login screen route

`qindaqt-settings --page login-screen` configures the SDDM greeter the whole
machine shows before anyone logs in: the login theme, the session automatic
login starts, automatic login itself, the numeric-lock state, and the cursor
theme. The write boundary and the polkit design are
[ADR-0225](../adr/0225-configure-the-login-screen.md).

## What the route shows

Every value on the page is what the merged SDDM configuration last said,
read the way `sddm.conf(5)` merges it: every file in
`/usr/lib/sddm/sddm.conf.d` and `/etc/sddm.conf.d` in lexical order, then
`/etc/sddm.conf`, later winning. Nothing on the page is an optimistic copy
of what the user just asked for — a choice is written, the file is re-read,
and only then does the control move.

- **Theme** lists what is actually installed under
  `/usr/share/sddm/themes`: directories with a `Main.qml`, showing the
  `metadata.desktop` name and a `preview.png`/`screenshot.png` thumbnail
  when the theme ships one. Themes carrying the `.qindaqt-sddm-theme` marker
  the QindaThemes repository ships sort first and are badged; the rest
  follow alphabetically.
- **Default session** offers every usable entry from
  `wayland-sessions`/`xsessions` (Wayland first, alphabetical inside each
  group), or "Remember each user's last session". SDDM has no machine-wide
  default-session key of its own — without automatic login it offers each
  user their last session — and the page says so in those words: the choice
  here is the session *automatic login* starts, written to
  `[Autologin] Session`. An entry that is `Hidden=true`, nameless, or has
  no `Exec=` is not offered.
- **Automatic login** is a switch plus a user picker (login users from
  `/etc/passwd`, uid ≥ 1000, real shells only). The page states plainly,
  next to the switch, that anyone who can reach the machine gets the session
  — no password is asked. Turning autologin on requires a session choice
  first; turning it off writes an empty `[Autologin] User` and leaves the
  pinned session in place, inert.
- **Numeric lock** and **cursor theme** are the `[General] Numlock`
  (`on`/`off`/`none`) and `[Theme] CursorTheme` keys SDDM documents — no
  invented configuration.

When a file that sorts after the route's own drop-in also sets one of these
keys, the page says which keys lose and which file wins, because the write
"succeeded" yet SDDM will use someone else's value.

## Authority and write boundary

Settings runs as the user and never opens `/etc` for writing. Each change is
validated client-side (a theme that is not installed, or a session that does
not exist, is refused by name without a password prompt), serialized as a
strict INI change-set of the five owned keys, and piped to
`pkexec qindaqt-sddm-config-helper`, gated by the polkit action
`org.qindaqt.settings.loginscreen.configure`. The helper's target is a
compile-time constant — `/etc/sddm.conf.d/zzz-qindaqt-settings.conf` — and
it re-scans and re-validates everything after elevating. The drop-in is
patched line-wise so comments, blank lines, foreign keys, and drop-ins such
as `01gentoo.conf` survive verbatim; `/etc/sddm.conf` is never written.

At open, the route asks polkit whether this process may use the action and
renders read-only with the plain reason until the answer is yes — no pkexec,
no helper, an unregistered action, or an unreachable system bus each get
their own sentence, and an authentication that would prompt counts as
writable with the prompt deferred to save time.

## What this route does not claim

- It does not change the running greeter: SDDM reads these files at greeter
  start, so a change is visible from the next login screen onward.
- It does not read or write the theme's own `theme.conf` user copies,
  `[Users]` ranges, X11/Wayland server options, or any other key — the five
  owned keys are the whole write set, and the helper enforces the same list.
- It does not write `/var/lib/sddm/state.conf` (the greeter's last-session
  memory): that is state, not configuration, and it sits outside this
  route's write boundary.
