# ADR-0386: Supervise the compositor-owned locker and capture broker

- **Status:** Accepted — fork source candidate; installed delivery pending
- **Date:** 2026-10-16
- **Owners:** Compositor fork, native lock, portal capture
- **Amends:** [ADR-0299](0299-native-locker-private-launch-and-authentication.md), [ADR-0324](0324-native-portal-capture-boundaries.md)
- **Superseded by:** None

## Context

The compositor launches two protected children of its own: the native locker
(ADR-0299) and the capture-only portal broker (ADR-0324). The locker was
launched once per lock request; if it exited while the session was locked
(a QML failure, a configure it could not handle, an output change) nothing
relaunched it and the fenced screen stayed black until a manual `RequestLock`
(findings item 7). The broker was launched at most three times per compositor
lifetime and the counter never reset. Locking the screen ends a broker whose
admission had admitted content, so a few lock cycles or a portal restart at
login used up the budget; ScreenCast then had no sources for the rest of the
session (findings item 8). Both teardowns also waited up to a second on the
compositor thread, and the broker's own stderr was read by nobody.

## Decision

Each child is kept running by one supervisor while it is needed:

- **Locker:** needed while the session is locked. A locker that exits, crashes
  or fails to start is relaunched after 0.5 s, doubling to 8 s, without a limit.
  The fence and black presentation hold throughout, the relaunched locker gets
  the same private socketpair trust, and an unlock ends supervision.
- **Broker:** needed while this compositor owns `org.qindaqt.KWin`. A broker
  that exits, is retired or cannot start is relaunched after 2 s, doubling to
  60 s, without a limit. The portal frontend gaining an owner, a foreign owner
  releasing the backend name, and this compositor becoming selected relaunch
  it at once.
- The delay returns to its first step once a child stayed healthy (locker
  holding the lock, broker published) for 10 s, so a child that becomes
  healthy and dies at once keeps backing off instead of looping.
- An explicit lock request launches at once, even inside a delay.
- Teardown kills a child and reaps it asynchronously; the compositor thread
  never waits. Every exit, refusal and scheduled delay is logged, and both
  children's stderr is forwarded to the compositor's. The broker states the
  reason for every exit there.

Relaunching never confers authority: every relaunched child passes the same
launch policy, credential and Ready checks as the first one.

## Consequences

- A locker crash costs a pause of at most 8 s of black screen per attempt
  instead of the session; screen capture survives lock cycles and portal
  restarts.
- A permanently broken child is retried forever at the cap (one launch and one
  log line every 8 s or 60 s), which is cheap and keeps recovery automatic.
- Fixture compositors keep their crash-to-black rows unless a test opts in to
  supervision, and never start the installed locker.
- Covered by the fork's helper tests `qindaqt.restart-backoff`,
  `qindaqt.locker-relaunch` and `qindaqt.capture-broker-supervision`, the native
  lock row `lockerRelaunchedAfterExitWhileLocked`, and the broker exit-reason
  rows in `qindaqt.portal-capture-startup`.

## Revisit when

A child needs a different notion of health, or field logs show the caps are
too short (repeated relaunch storms) or too long (slow recovery).
