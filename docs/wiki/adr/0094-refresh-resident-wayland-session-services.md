# ADR-0094: Refresh resident Wayland-connected services at session entry

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Platform and session supervision
- **Supersedes:** None
- **Superseded by:** None

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
D-Bus-only activatable services (Settings1, Network1, Power1, Bluetooth1, the
portal) are unaffected: systemd starts them fresh, with the just-published
environment, the first time this session's shell talks to them.

## Decision

Immediately after `publishActivationEnvironment` and before any desktop
consumer starts, `qindaqt-session` calls a new, separate
`refreshResidentWaylandServices(bus, unitNames)` (in
`src/session_supervisor/src/resident_service_refresh.{h,cpp}`) with a fixed,
reviewed list of unit names returned by `residentWaylandServiceUnits()`:
`qindaqt-clipboard-host.service` and `qindaqt-display-service.service`.

For each named unit it calls `org.freedesktop.systemd1.Manager.RestartUnit(name,
"replace")` on the same session bus, bounded by a two-second call timeout. This
both restarts a unit that is already resident from a prior desktop and starts
one that has not yet been activated in this session; no separate
active-state query is needed. A restarted resident process reconnects with
`wl_display_connect` under the environment `SetEnvironment` just applied.

The list is closed and explicit. A service is added only when its own module
independently opens Wayland; ownership and review stay with that module's ADR.
No unit outside this fixed list is touched, so unrelated resident services
(Settings1, Network1, Power1, Bluetooth1, the portal) and their persisted
preferences are left running exactly as they were. A missing unit or transport
failure is logged and does not stop the desktop from starting, matching
ADR-0082's failure posture. This mechanism does not itself change what
`qindaqt-clipboard-host`'s ADR-0058 restart already implies (a new capture
epoch); it only makes that restart happen reliably at session entry instead of
requiring a manual `systemctl --user restart`.

This is not a login-time fix for every possible service-scaling failure; it
closes the one demonstrated lifecycle cause (a resident process holding a
stale Wayland connection past environment publication), not a general claim
that all activation-environment scaling problems are resolved.

## Consequences

- A resident Wayland-connected service picks up the current session's socket
  without a full logout, and without a manual operator restart.
- Session startup makes two additional bounded, best-effort D-Bus calls; a
  slow or unreachable user manager degrades to the same warning-and-continue
  behavior as the existing `SetEnvironment` call.
- The fixed list is a private-bus-tested contract
  (`qindaqt.session-resident-service-refresh`) covering: both configured units
  receive `RestartUnit` with mode `replace`; one unit failing to restart does
  not stop the request for the remaining units or fail session startup; and
  the production list itself.
- Adding a third resident Wayland consumer requires updating
  `residentWaylandServiceUnits()` and this ADR's list, not a structural change
  to the mechanism.

## Revisit when

A future resident service opens its own Wayland connection and needs the same
treatment, or systemd/D-Bus gains a native "reload environment into running
service" primitive that makes the explicit restart list unnecessary.
