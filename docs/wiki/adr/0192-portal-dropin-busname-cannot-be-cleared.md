# ADR-0192: the KDE portal drop-in must not try to clear `BusName=`

- **Status:** Accepted
- **Date:** 2026-09-18
- **Owners:** Platform (session, portal integration)
- **Supersedes:** the `BusName=` half of [ADR-0170](0170-survive-a-private-session-bus-for-dbus-units.md)'s decision; ADR-0170's `Type=exec` decision and every other consequence stand unchanged.
- **Superseded by:** None

## Context

ADR-0170 (2026-09-16) had `20-qindaqt-remotedesktop.conf` override the
packaged `plasma-xdg-desktop-portal-kde.service` to `Type=exec` and declare a
bare `BusName=` to "clear" the packaged unit's
`BusName=org.freedesktop.impl.portal.desktop.kde`, reasoning that an empty
override is the usual systemd convention for resetting a directive to its
default (as it is for list-type settings such as `ExecStart=`).

`journalctl --user -b -p warning` on `qinda-top` (systemd 261) shows this
never worked:

    systemd[924]: /usr/lib/systemd/user/plasma-xdg-desktop-portal-kde.service.d/20-qindaqt-remotedesktop.conf:28: Invalid bus name, ignoring:

logged on every (re)start of the unit since the drop-in shipped (repeated at
21:32, 21:46, 22:11, 22:56 on 2026-09-16 alone). `systemctl --user show
plasma-xdg-desktop-portal-kde.service -p BusName` confirms the value the ADR
meant to clear is still active:

    BusName=org.freedesktop.impl.portal.desktop.kde

`man systemd.service` states `BusName=` "is mandatory for services where
`Type=` is set to `dbus`" and is otherwise only a recommended hint used by
`systemctl service-log-level`/`service-log-target`. It does not document an
empty-value reset convention, and this systemd version enforces that: a bare
`BusName=` is parsed as an invalid D-Bus name and the assignment is dropped
with a warning, leaving the inherited value in place. The net effect ADR-0170
wanted (BusName no longer governs unit readiness) was already fully achieved
by `Type=exec` alone; the extra line bought nothing and cost one warning line
per start.

`Type=exec` itself is unaffected by this and continues to work: the unit has
been `active running` for 5+ hours straight on `qinda-top` at probe time.

## Decision

Drop the `BusName=` line from `20-qindaqt-remotedesktop.conf` entirely. The
drop-in overrides `Type=exec` and `Environment=XDG_CURRENT_DESKTOP=KDE` only;
`BusName=` keeps the packaged unit's inherited
`org.freedesktop.impl.portal.desktop.kde`, which is harmless and in fact more
correct under `Type=exec` (`systemctl service-log-level` still resolves the
unit by its real bus name).

The contract test `qindaqt.portal-kde-compat`
(`tests/services/portal/check_kde_portal_compat.cmake`) is inverted to match:
it now fails if the drop-in declares an empty `BusName=` line, instead of
requiring one.

## Consequences

- The laptop's user journal loses one recurring warning line per portal
  backend (re)start, part of Checkpoint L's "no repeated warnings" row.
- ADR-0170's screen-capture fix, its `Type=exec` reasoning, and its measured
  19-vs-11-interface table are unaffected; only the `BusName=` clearing claim
  in its Decision section is corrected here, per this project's "do not
  rewrite history, supersede" rule for ADRs.
- No functional change to what the KDE portal backend registers or how
  clients discover it; this is a warning-only fix.

## Revisit when

Any future systemd release documents an explicit reset syntax for
`BusName=`; there is no reason to keep the line even then, since `Type=exec`
already makes its value inert for this unit.
