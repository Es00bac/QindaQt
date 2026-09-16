# ADR-0170: judge the KDE portal backend's start by exec, not by a bus name

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (session, portal integration)
- **Supersedes:** None (extends ADR-0088's KDE compatibility identity)
- **Superseded by:** None

## Context

Screen capture on QindaQt did not work. OBS, browsers and Discord all obtain it
through `org.freedesktop.portal.ScreenCast`, and QindaQt routes that interface to
the KDE backend (`qindaqt-portals.conf`), which is a hard dependency it already
ships a compatibility drop-in for ([ADR-0088](0088-enable-kde-remote-desktop-for-qindaqt.md)).

On a live session the backend was running and had the bus name, but exposed only
eleven `org.freedesktop.impl.portal.*` interfaces — **no ScreenCast, Screenshot,
RemoteDesktop, InputCapture or GlobalShortcuts**. It had
`XDG_CURRENT_DESKTOP=QindaQt`, not the `KDE` the drop-in sets.

Measured directly, same binary and same compositor, on a private bus:

| `XDG_CURRENT_DESKTOP` | impl interfaces registered | ScreenCast | Name acquired |
| --- | --- | --- | --- |
| `KDE` | 19 | yes | 0.5 s |
| `QindaQt` | 11 | no | 0.5 s |

So the identity is decisive and the drop-in is correct — it simply was not
reaching the process that ended up owning the name. The session journal shows
why:

    18:30:45  Starting Xdg Desktop Portal For KDE...        (systemd, KDE identity)
    18:32:15  start operation timed out. Terminating.
    18:32:15  Failed to start Xdg Desktop Portal For KDE.
    18:33:39  /usr/libexec/xdg-desktop-portal-kde starts    (PPID 1, QindaQt identity)

The packaged unit is `Type=dbus` with
`BusName=org.freedesktop.impl.portal.desktop.kde` and a 90 s start timeout.
A QindaQt session may run on a **private** session bus that it bootstraps
itself: `SessionBusBootstrap` runs `dbus-run-session` whenever
`DBUS_SESSION_BUS_ADDRESS` is unset, which is every host with no user
`dbus.socket` — the reporting host has none, and its session bus is
`/tmp/dbus-…`. The systemd **user manager** holds no client connection to that
bus. It therefore can never observe the unit acquiring `BusName=`, kills a
working backend at `TimeoutStartSec`, and D-Bus activation respawns the binary
directly — without the unit's drop-in environment. The replacement is the one
that keeps the name.

This is not specific to the portal. Every unit `refreshResidentServices()`
restarts is `Type=dbus`, and all four failed on the observed session:
`qindaqt-clipboard-host` and `qindaqt-display-service` with `start-limit-hit`,
`plasma-xdg-desktop-portal-kde` and `xdg-desktop-portal` with `timeout`. Their
processes run anyway because D-Bus activation respawns them; what is lost is
any unit-scoped environment and any systemd-side ordering.

## Decision

QindaQt's KDE portal drop-in overrides the unit to `Type=exec` and clears
`BusName=`. systemd then judges the start by exec success, which is observable
on any bus, so the identity-carrying process is the one that keeps the name.

Client activation is unaffected: `dbus-daemon` waits for the name to appear on
the session bus through its own watch, independently of the unit's `Type`. The
unit keeps `Restart=no`, so nothing new is respawned on exit.

The contract test `qindaqt.portal-kde-compat` now requires both the identity and
the activation override, with the `Type=exec` assertion anchored to a line of its
own — an earlier version of it was satisfied by the drop-in's own explanatory
comment, which proved nothing.

## Consequences

- Screen capture works on a default install: OBS, browsers and Discord get a
  ScreenCast backend. Screenshot, RemoteDesktop, InputCapture and
  GlobalShortcuts come back with it.
- The fix is scoped to the one unit QindaQt already owns a drop-in for. The
  other three `Type=dbus` units still fail under systemd on a private-bus
  session; they work in practice through D-Bus activation because they need no
  unit-scoped environment, but the failures are real and are logged every
  session.
- QindaQt still does not implement ScreenCast itself. The dependency on
  `xdg-desktop-portal-kde` is explicit and now actually functional.

## Revisit when

- The session runs on the standard `$XDG_RUNTIME_DIR/bus`. With a
  systemd-managed session bus the manager can observe name acquisition,
  `Type=dbus` is correct again, and this override should be removed rather than
  kept as folklore. Enabling the user `dbus.socket` is host configuration, not a
  QindaQt code change, which is why this ADR does not simply mandate it.
- QindaQt owns activation on a bus it bootstrapped. If the session created the
  bus, it is the session — not systemd — that can guarantee which process owns
  a name on it; starting the backend as a session child with the right
  environment would remove the race entirely.
- A native QindaQt ScreenCast backend lands, at which point the routing in
  `qindaqt-portals.conf` changes and this compatibility path retires.
