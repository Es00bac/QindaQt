# ADR-0225: Configure the login screen from Settings through a polkit-gated helper

- **Status:** Proposed
- **Date:** 2026-09-21
- **Owners:** Settings
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt ships SDDM themes and selects one
(`/etc/sddm.conf.d/zz-qindaqt-theme.conf` sets `Current=qinda-reclaimed`),
but the desktop offered no way to configure the login screen at all. The
operator's ask was blunt: there is no SDDM configurator, and that needs to
change.

SDDM configuration is root-owned. `sddm.conf(5)` merges every file in the
configuration directories in lexical order and then the legacy main file,
later winning — so the machine's effective settings are spread across
`/usr/lib/sddm/sddm.conf.d`, `/etc/sddm.conf.d` (including files the
operator wrote by hand, such as `01gentoo.conf`) and `/etc/sddm.conf`.
Settings runs as the logged-in user: it must not be setuid, must not shell
out to `sudo`, and must never open `/etc` for writing itself.

## Decision

A **Login screen** route (`login-screen`) in the Settings Center, registered
last so no existing route index, shortcut, or traversal position moves
([ADR-0128](0128-accessibility-settings-route.md) appended-route rule). The route owns exactly five
keys and nothing else:

| Key | Meaning |
| --- | --- |
| `[Theme] Current` | the login theme, chosen from what is installed under `/usr/share/sddm/themes` |
| `[Theme] CursorTheme` | the pointer theme on the greeter |
| `[General] Numlock` | `on` / `off` / `none` |
| `[Autologin] User` | the account logged in automatically; empty means off |
| `[Autologin] Session` | the session automatic login starts |

- **The privileged write is a helper, not the app.** Settings pipes a strict
  INI change-set payload to `pkexec qindaqt-sddm-config-helper`, gated by
  the polkit action `org.qindaqt.settings.loginscreen.configure`. The
  helper's target path is a compile-time constant —
  `/etc/sddm.conf.d/zzz-qindaqt-settings.conf` — so it can never be steered
  into writing `/etc/sddm.conf` or another drop-in. It re-scans installed
  themes, sessions, and login users and re-validates the payload after
  elevating, because the caller is untrusted; a key outside the owned set or
  a value that is not installed is rejected by name.
- **Merging preserves what the route does not own.** The drop-in is patched
  line-wise: an existing owned line is replaced in place (the *last*
  duplicate, which is the one SDDM honors), a missing owned key is appended
  to its section, and every other line — comments, blanks, foreign keys — is
  kept verbatim. `01gentoo.conf` and hand-written drop-ins are never
  touched.
- **The page authorizes before it offers.** At open, the route asks polkit's
  authority (`CheckAuthorization`, no interaction flag) whether this process
  may use the action. Until the answer is yes, the page renders read-only
  with the plain reason — no pkexec, no helper, a refused action, or a dead
  system bus each produce their own sentence — rather than letting the user
  edit and fail at save time. A `challenge` answer counts as writable: the
  prompt then happens once, at save time, where the user expects it.
  - The D-Bus call is the full documented signature
    `((sa{sv})sa{ss}us)`: a `unix-process` subject carrying this process's
    pid, start time (raw `/proc/self/stat` field 22, clock ticks since boot
    — converting to epoch seconds makes the daemon answer "process has been
    replaced"), and uid. Current polkit rejects the call when any of the
    three is missing. Each requirement was confirmed against the live
    daemon, not inferred from older documentation.
- **Published values are file truth, never intent.** Every control binds to
  what the merged configuration last said. A choice validates, serializes
  through one outstanding write at a time, re-reads the file after the
  helper finishes, and abandons queued intents when a write is refused. A
  control that moved optimistically would claim a polkit-gated write
  succeeded when it had not.
- **Overrides are stated, not hidden.** When a file that sorts after the
  route's own drop-in (or `/etc/sddm.conf`) also sets an owned key, the page
  says which keys lose and which file wins. A configurator that silently
  loses is worse than one that cannot write at all.
- **Autologin is labeled for what it is.** The page says plainly that anyone
  who can reach the machine gets the session, no password asked. Turning
  autologin on requires a chosen session first; off writes an empty
  `Autologin/User` and leaves the pinned session in place, inert.
- **No invented configuration.** SDDM has no machine-wide default-session
  key: without autologin the greeter offers each user their last session.
  The page says so in those words and presents its session picker as the one
  automatic login starts, instead of fabricating a setting SDDM does not
  read. (Writing `/var/lib/sddm/state.conf` was considered and rejected: it
  is state, not configuration, and it sits outside the `/etc/sddm.conf.d`
  write boundary.)

## Consequences

- The route page, the pure model, and the discovery/merge/validation logic
  live in `src/apps/settings/login_screen/` with focused tests; the model
  test runs with no bus at all, so the read-only projection is proven
  without polkit.
- Installing the route requires the helper and the policy file
  (`org.qindaqt.settings.loginscreen.policy`); without them the page is
  honest read-only, which is the intended degraded state on a machine where
  the package is not fully installed.
- QindaQt themes sort first in the theme list via the
  `.qindaqt-sddm-theme` marker the QindaThemes repository ships; every other
  installed theme follows alphabetically, with `preview.png`/`screenshot.png`
  shown when present.
- Only the five owned keys are ever written anywhere. Anything else an
  operator wants — `MinimumUid`, X11/Wayland server options, theme
  configuration files — remains hand-edited, and the route neither offers
  nor disturbs it.
