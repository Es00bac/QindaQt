# ADR-0025: Arbitrate session-bound Power1 activation

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Session supervisor and Power platform service

## Context

The backlight provider must connect only to QindaQt's child compositor, but
D-Bus and systemd user activation environments are shared across a user's
graphical sessions. Unconditional takeover lets two supervisors stop and
restart each other's service forever.

## Decision

The session supervisor publishes equal child-socket markers through systemd
`Manager.SetEnvironment(as)` and D-Bus
`UpdateActivationEnvironment(a{ss})`, awaits both replies, then activates
`Power1` with one initial attempt and at most two retries per generation.

One deterministic same-user graphical-session arbiter selects the active
session, breaking unresolved ties by the lowest active-or-online session ID.
Losers publish typed unavailable and perform no publication, activation,
retry, or stop. Only the winner may replace a foreign binding. `Power1` exits
before connecting unless both markers match an existing private socket.

## Consequences

- Activation cannot fall back to the host compositor.
- Simultaneous supervisors converge instead of entering a takeover loop.
- Publication failure consumes the same bounded retry budget.
- Multiple simultaneous same-user QindaQt providers remain unsupported in
  version 1 but fail deterministically and visibly.
