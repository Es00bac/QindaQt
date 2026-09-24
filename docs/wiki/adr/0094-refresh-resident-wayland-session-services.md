# ADR-0094: Refresh resident Wayland-connected services at session entry

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Platform and session supervision
- **Supersedes:** None
- **Superseded by:** None
- **Extended by:** [ADR-0256](0256-refresh-audio1-after-package-upgrades.md)

## Context

[ADR-0082](0082-publish-session-activation-environment.md) republishes the
current desktop connection environment to D-Bus and the systemd user manager
so future activations receive KWin's current Wayland socket. Its own
Consequences section records what it deliberately does not do: "This does not
restart already-running services." That gap produced an observed failure. A
prior desktop's `qindaqt-display-service` (unit PID 1780106) stayed resident
across a later QindaQt login. `SetEnvironment` correctly updated the manager's
activation environment, but the already-running process never reread it, so
the service kept its original `wl_display_connect` from the retired socket.
Its D-Bus-facing behavior looked fine; only its output-management calls into
the stale compositor failed. A manual `systemctl --user restart
qindaqt-display-service` was required to recover it.

The same class of bug applies to any resident systemd user service that opens
its own direct Wayland connection rather than only using D-Bus: today that is
`qindaqt-clipboard-host` ([ADR-0058](0058-isolate-clipboard-capture-in-a-volatile-host.md),
`wl_display_connect` in the clipboard Wayland adapter) and
`qindaqt-display-service` ([ADR-0053](0053-compose-display1-from-authenticated-runtime-authorities.md),
`wl_display_connect` in the display-writer output-management port). Ordinary
D-Bus-only activatable services (Settings1, Network1, Power1, Bluetooth1,
and Audio1) were outside this refresh list at acceptance: systemd starts them
fresh with the just-published environment when this session first talks to
them. ADR-0256 records the later, evidence-backed Audio1 package-upgrade
exception; the other services remain outside this list.

A live physical-session recovery on 2026-09-06 found a second, related cause:
the `xdg-desktop-portal` frontend process (PID 1966668, started 19:33 under a
prior KDE desktop) stayed resident across a later QindaQt login (`qindaqt-0`,
20:17) and kept `WAYLAND_DISPLAY` and `XDG_CURRENT_DESKTOP=KDE` from that
prior desktop. `xdg-desktop-portal` selects and caches which backend it
routes to once, at its own startup, from `XDG_CURRENT_DESKTOP`; it does not
re-select per call. Its routed-to backend,
`plasma-xdg-desktop-portal-kde.service` ([ADR-0088](0088-enable-kde-remote-desktop-for-qindaqt.md)),
also stayed resident and is exposed to the same stale environment through the
KDE-only drop-in ADR-0088 installs; unlike the frontend, the KDE backend is
not D-Bus-only — it is itself a Qt Wayland client and a screencast/
remote-desktop consumer, so it independently holds a stale Wayland connection
in the same way `qindaqt-clipboard-host` and `qindaqt-display-service` do.
Both processes cache session identity/routing or connection state at startup
that `SetEnvironment` does not retroactively correct. Restarting the user
session's two portal units (not a host logout) recovered correct routing.
Other D-Bus-only activatable services have no such per-process startup cache
and remain unaffected by this stale-desktop-environment diagnosis. The later
Audio1 package-upgrade ABI exception is recorded separately in ADR-0256.

## Decision

Immediately after publishing the activation environment and before any
desktop consumer starts, qindaqt-session calls the separate
refreshResidentServices(bus, unitNames) helper in
src/session_supervisor/src/resident_service_refresh.{h,cpp} with the fixed,
reviewed list returned by residentServiceRefreshUnits(). At ADR-0094
acceptance, the list was qindaqt-clipboard-host.service,
qindaqt-display-service.service, plasma-xdg-desktop-portal-kde.service, and
xdg-desktop-portal.service — the portal backend before the frontend that routes
to it. The backend restart is requested first, but request order does not
guarantee completion order, since each RestartUnit call only enqueues a systemd
job.

For each named unit, in order, the supervisor calls
org.freedesktop.systemd1.Manager.RestartUnit(name, "replace") through the
systemd user-manager route: the private control socket when available, or the
session bus only when the manager owns that name there. This preserves the
private-bus routing contract in ADR-0170 and avoids activating a second,
unreachable user manager. The call itself is bounded by two seconds and
returns a job object path; it does not wait for the unit to finish restarting.
A unit already resident from a prior desktop is restarted, and an inactive
unit can be started without a separate active-state query. The Audio1-specific
old-owner wait and its bounded result are described in ADR-0256. Once a
Wayland consumer restarts, it reconnects under the environment just applied;
the portal frontend and backend re-select routing from the current
XDG_CURRENT_DESKTOP.

The list is closed and explicit. The original inclusion rule covers services
whose own module independently opens Wayland, is itself a Wayland client, or
caches desktop-scoped environment or routing at startup. ADR-0256 adds Audio1
only as a package-upgrade ABI exception, with its own evidence and bounded
owner-retirement contract. Ownership and review stay with each service's
module ADR. No unit outside the current five-unit list is touched; Settings1,
Network1, Power1, and Bluetooth1 remain outside it with their persisted
preferences unchanged. A missing unit or transport failure is logged and does
not stop the desktop from starting, matching ADR-0082's failure posture.

This is not a login-time fix for every possible service-scaling failure; it
closes the two demonstrated lifecycle causes (a resident process holding a
stale Wayland connection, or a resident process holding cached desktop
identity/routing, past environment publication), not a general claim that all
activation-environment scaling problems are resolved.

## Consequences

- At ADR-0094 acceptance, session startup made four bounded, best-effort D-Bus
  calls. ADR-0256 adds Audio1 as a fifth unit and waits at most two seconds for
  its prior unique bus owner to retire. Timeout or owner-query failure is
  logged, reported by the helper, and does not stop session startup.
- The private-bus-tested contract qindaqt.session-resident-service-refresh
  covers configured restart calls, continued requests after a unit error, the
  fixed production list, delayed Audio1-owner retirement, and the bounded
  stale-owner case.
- Any further resident-service addition requires updating the fixed unit list
  and the owning ADR; the refresh mechanism remains unchanged.

## Revisit when

A future resident service opens its own Wayland connection, independently
caches desktop-scoped environment or routing state at startup, or presents a
demonstrated incompatible package-upgrade ABI that requires the same
treatment; or systemd/D-Bus gains a native reload-environment-into-running-
service primitive that makes the explicit restart list unnecessary.
