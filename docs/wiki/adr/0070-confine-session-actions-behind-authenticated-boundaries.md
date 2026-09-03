# ADR-0070: Confine session actions behind authenticated boundaries

- Status: Accepted
- Date: 2026-09-03
- Deciders: QindaQt architecture group
- Scope: shell logout, lock, and machine power actions

## Context

Power1 owns battery, profile, and brightness truth, not desktop-session
lifetime. The session supervisor already owns the essential shell/notification
processes and the shell PID, while ScreenSaver and login1 remain the standard
lock and machine-power authorities. Letting either Power presentation surface
call those buses directly would duplicate admission, pending, and replay
policy.

## Decision

The supervisor publishes the minimal `org.qindaqt.Session1` logout interface
and authenticates each call by resolving the caller PID and comparing it with
the live supervised shell PID. Accepted logout stops essential children in a
fixed order and exits so KWin's existing session coupling ends the compositor.

A separate `src/services/session_actions` client owns all Session1,
ScreenSaver, and login1 calls through injected bus connections. It exposes
typed availability, permits one bounded request, rechecks `CanLogout` or the
exact login1 `Can* == "yes"` fact immediately before destructive dispatch,
pins the unique owner, and never replays an uncertain mutation. Power applet
and Settings QML receive only this public client surface. Power1 is unchanged.

The optional Network secret agent remains a supervisor child because its
lifetime is session-wide, but it is non-essential, readiness-independent, and
restarted at most once.

## Consequences

- Direct login1 and ScreenSaver symbols outside the client are boundary-test
  failures.
- Only the supervised shell can log out. A separate Settings process shows
  logout unavailable rather than gaining a proxy or weaker trust rule.
- Lock and Suspend may dispatch without confirmation; Logout, Reboot, and
  PowerOff require confirmation in both presentation surfaces.
- Meta+L uses the existing audited shortcut registrar and the same client
  request; no new shortcut subsystem is introduced.
- Bus loss, owner replacement, or deadline after mutation dispatch produces
  uncertain no-replay feedback.

## Alternatives rejected

Adding actions to Power1 mixes machine/session policy with power telemetry and
reverses ADR-0023/0060. Direct QML D-Bus calls cannot enforce a shared bounded
lifecycle. Trusting a well-known bus name or cached caller identity would
authorize a replacement process after shell restart.
