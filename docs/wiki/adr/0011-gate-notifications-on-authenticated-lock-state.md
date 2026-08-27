# ADR-0011: Gate notifications on authenticated compositor lock state

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Session, shell, and notification presentation
- **Supersedes:** None
- **Superseded by:** None

## Context

The production shell receives full notification summaries, bodies, actions,
and application identities through an authenticated presenter channel. That
channel proves that the shell is the notification host's selected presenter;
it does not prove whether KWin is unlocked, whether a lock acquisition is in
progress, or whether a screen-locker signal came from the compositor process.
Do Not Disturb is likewise an interruption preference, not a privacy boundary:
its critical-urgency bypass is unacceptable while the screen is locked.

KWin 6.6.5 embeds KScreenLocker and owns the session-bus services
`org.freedesktop.ScreenSaver` and `org.kde.screensaver`. The QindaQt compositor
plugin independently owns `org.qindaqt.Compositor`. A same-UID process can race
to claim a well-known session-bus name, so observing any one name or trusting a
well-known sender without owner verification is insufficient. `GetActive`
becomes true only after the lock surface is active, while `AboutToLock` provides
the earlier signal needed to suppress private content during acquisition.

QindaQt does not yet have a separately authenticated, data-minimized
lock-screen notification presenter. Continuing to expose the ordinary shell's
full notification projections while state is unknown would turn startup,
service races, and owner replacement into privacy leaks.

## Decision

Add a focused `session_lock_state` service-client module. It owns asynchronous
session-bus observation and publishes only four platform-neutral states:
`Unknown`, `Unlocked`, `Locking`, and `Locked`. Content may be shown only in
`Unlocked`; construction, transport failure, owner churn, malformed replies,
timeouts, and incomplete authentication all remain fail closed.

The KWin process identity is provisioned independently of D-Bus names.
`qindaqt-wm` replaces itself with KWin, which starts `qindaqt-session` as its
direct `--exit-with-session` child. Before accepting its kernel parent PID, the
supervisor arms a Linux parent-death signal and rechecks the parent to close the
setup race. Each essential child independently arms the same race-closed
witness against the supervisor. KWin death therefore removes the supervisor,
host, and shell rather than leaving a token-authenticated orphan that could
later trust a reused numeric PID. The supervisor passes the non-secret witnessed
PID to the shell as `--compositor-pid` whenever it provisions the notification
presenter descriptor. The PID is not a token and does not replace presenter
authentication.

The lock-state monitor accepts a binding only after all of the following are
true:

1. `org.qindaqt.Compositor`, `org.freedesktop.ScreenSaver`, and
   `org.kde.screensaver` resolve to the same exact unique bus owner;
2. the bus daemon reports that unique owner's Unix PID as the supervisor-
   provisioned KWin PID; and
3. the initial active-state reply and all later lock signals belong to that
   bound unique owner and the current owner generation.

Signal observation is established before the active-state query. Generation
and request serials prevent stale owner, PID, or active replies from restoring
an older state. `AboutToLock` advances to `Locking` and fences a stale false
active reply; `ActiveChanged(true)` advances to `Locked`; a trusted terminal
`ActiveChanged(false)` may return to `Unlocked`. Initial authentication requires
two consecutive serial-fenced `GetActive(false)` replies before publishing
`Unlocked`; any owner event, lock signal, or request failure invalidates that
confirmation. Registration-before-object startup is handled with bounded
asynchronous retry. The module never performs a blocking D-Bus call and never
consults shell or notification objects.

The transport subscribes to the local D-Bus `Disconnected` signal before
watching service owners. Losing the session-bus daemon revokes the trusted
state immediately, fences pending callbacks, and returns the monitor to
`Unknown`.

Add a separate injected notification privacy policy. It outranks the
interruption policy and defaults to denied. While denied, the presentation
controller closes the center; clears Active, popup, Recent, error, busy, and
accessibility-facing projections; stops presentation timers; rejects new
dismiss/action requests without transport calls; and suppresses every popup,
including critical urgency. Notification windows remain hidden, the applet is
unavailable and cannot emit a toggle, and the already-registered global action
becomes a no-op without rewriting the user's KGlobalAccel binding.

The authenticated presentation client may continue tracking its private
snapshot and settling an operation that began before locking. Those results are
not projected while denied. Returning to `Unlocked` baselines the current
authoritative snapshot: currently active entries may reappear in Active, but
notifications received or removed during the private interval do not become
popup or Recent replay. Recent stays cleared across the privacy boundary.

This boundary deliberately does not show notification cards, counts, icons,
generic placeholders, or actions on the lock screen. A future lock-screen
experience requires a separately provisioned, audience-limited protocol and
credential; the ordinary full-content presenter token must not be handed to a
greeter.

## Consequences

- Missing or late KScreenLocker objects degrade notification presentation, not
  the panel session: the shell stays usable while notification content remains
  private.
- The presenter token, compositor owner/PID binding, and KScreenLocker/PAM user
  authentication remain three distinct trust boundaries. None substitutes for
  another.
- Linux parent-death witnessing is part of the PID trust chain; environments
  that insert a launcher process between KWin and `qindaqt-session` fail closed
  instead of treating that launcher's PID as compositor authority.
- A lock attempt that never produces a trustworthy terminal state remains
  suppressed until owner-bound reauthentication or session restart. This is an
  intentional fail-closed result.
- Do Not Disturb state may remain session-volatile across a lock, but it cannot
  weaken privacy. After unlock it again controls only popup interruption.
- Pure state-machine, private-D-Bus, presentation-model, facade, and offscreen
  window tests can qualify the boundary without locking the developer's real
  desktop or injecting input.
- Initially locked login, shell/compositor restart during acquisition,
  alternative lockers, multi-seat switching, suspend/resume, and live
  accessibility behavior remain separate qualification gates.

## Revisit when

Revisit the data contract only when QindaQt implements a dedicated lock-screen
presenter, supports non-KScreenLocker compositors, or qualifies multi-seat and
session-switching semantics. Any lock-screen presentation must begin with a
new least-authority protocol rather than broadening this private-shell channel.
