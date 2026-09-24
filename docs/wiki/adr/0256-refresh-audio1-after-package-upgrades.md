# ADR-0256: Refresh Audio1 after package upgrades at session entry

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Platform and session supervision; Audio service
- **Supersedes:** None (extends ADR-0094's inclusion rule for Audio1 only)
- **Superseded by:** None

## Context

ADR-0094 refreshes resident processes that retain stale Wayland connections or
cache desktop routing across login. Its original rule excluded ordinary
D-Bus-only services because activation starts a process from the installed
binary when a client first requests it.

That assumption does not hold when the systemd user manager and the Audio1
process persist across logout. An upgrade can replace the executable on disk
while the resident process continues serving its old D-Bus ABI. The observed
upgrade from source `8a86b748` to `397216bc` changed the Audio1
`VbanStream` D-Bus structure from seven fields to eight. Settings on the
upgraded desktop then reported audio unavailable while talking to the old
owner; a fresh boot started the new process and `GetSnapshot` succeeded.

This is a specific installed-package compatibility failure. It does not show
that other D-Bus-only Settings-facing services need a session-start restart.

## Decision

Extend ADR-0094's reviewed unit list with
`qindaqt-audio-service.service`. `qindaqt-session` requests its restart after
publishing the current activation environment and before starting shell
consumers.

Because `RestartUnit` only enqueues a systemd job, the session supervisor first
records the current unique owner of `org.qindaqt.Audio1`, requests the restart,
then waits up to two seconds for that owner to disappear or change. This
prevents later Settings consumers from addressing the old process when the
manager successfully retires it. A missing initial owner needs no retirement
wait. An owner-query failure, restart failure, or timeout is logged and
reported by the helper; session startup continues so Audio1 cannot block the
desktop.

The existing `Type=dbus` unit and private-bus activation behavior remain
unchanged. ADR-0170 documents that on a private QindaQt session bus, systemd
may not observe Audio1's name acquisition while D-Bus activation leaves an
unmanaged process running. In that case a restart request may not retire the
old owner. The bounded wait reports this condition, but the session continues
and Settings may still encounter that owner. The change does not claim to fix
this private-bus limitation.

Settings1, Network1, Power1, and Bluetooth1 remain outside the refresh list.
They have no demonstrated Audio1-style package-upgrade ABI failure in this
incident.

## Consequences

- Session entry makes one additional bounded RestartUnit request and checks
  Audio1 owner retirement before launching desktop consumers.
- If the old owner is retired, Settings activates or addresses the current
  installed Audio1 process. If retirement cannot be established within two
  seconds, the desktop remains available and logs the unresolved condition.
- The focused refresh test covers delayed owner retirement and the bounded
  case where a fake manager accepts the restart request but the old owner
  remains. It does not simulate systemd killing a real Type=dbus service on
  a private bus.

## Revisit when

The user manager reliably retires Audio1 on every supported session-bus
configuration, or a package-upgrade compatibility handshake allows Settings to
reject an incompatible owner before making Audio1 calls.
