# ADR-0082: Publish the current session activation environment

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Platform and session supervision
- **Supersedes:** None
- **Superseded by:** None

## Context

The [session supervisor](../architecture/compositor-session.md) inherits KWin's
current Wayland socket, while D-Bus and the systemd user manager can retain an
older desktop's environment. Services activated by Settings consequently fail
to connect even though directly launched desktop applications work.

## Decision

After establishing its compositor parent and before launching desktop consumers,
`qindaqt-session` publishes the current desktop connection environment to both
D-Bus `UpdateActivationEnvironment` and the user manager's `SetEnvironment`.
Only present values of `DBUS_SESSION_BUS_ADDRESS`, `WAYLAND_DISPLAY`, `DISPLAY`,
`XDG_RUNTIME_DIR`, `XDG_SESSION_TYPE`, `XDG_CURRENT_DESKTOP`, and
`XDG_SESSION_DESKTOP` are copied. Each call has a two-second limit. Failure is
logged without preventing the desktop from starting; non-systemd private test
sessions remain supported.

## Consequences

D-Bus and systemd activation receive the same socket and desktop identity as the
supervised shell. No new service supervisor or dependency is introduced. The
contract is tested on a private bus with a fake user manager, including the
current socket, bus address, desktop identity, and exclusion of unrelated
variables. This does not restart already-running services or install missing
system services.

## Revisit when

Concurrent graphical sessions for the same user require separate activation
brokers or service instances rather than the existing shared user manager.
