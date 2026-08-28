# ADR-0019: Restart the production shell once per compositor session

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Session and notification presentation
- **Supersedes:** None
- **Superseded by:** None

## Context

The production session supervisor owns a resident notification host and the
shell. The host owns the freedesktop notification service and its active
in-memory records, while the shell owns presentation history and authenticates
to that host with a supervisor-generated token plus the direct KWin PID.

Ending the complete compositor session on the first shell failure preserves a
simple lifetime but turns a recoverable presentation crash into desktop loss.
Restarting both children would discard active notifications and temporarily
vacate the standard service. An unbounded shell restart loop could consume
resources indefinitely, hide a deterministic startup fault, or repeatedly
exercise a compromised presentation path. Persisting the token to make an
external restart possible would enlarge the secret boundary.

The owning contracts are documented in
[Compositor and session integration](../architecture/compositor-session.md)
and [Notification service](../architecture/notifications-service.md).

## Decision

`qindaqt-session` retains the generated presentation token only in supervisor
memory for the lifetime of a healthy supervised session. The notification host
remains resident when the first shell process exits unexpectedly. The
supervisor consumes a one-restart budget and starts one replacement shell with:

- a newly created one-shot inherited token descriptor containing the same
  in-memory session token;
- the same validated direct KWin PID used by the original shell; and
- the same profile and theme arguments.

Ordinary production replacement argv remains identical. In an explicit
development session only, the supervisor also passes the exact predecessor
shell PID so ADR-0020's evidence-name registration can wait boundedly for that
specific owner to release without queuing or replacing another owner. The PID
is non-secret and grants no production presentation authority.

The host's existing unique-name disconnect handling releases the former
presenter binding before the replacement authenticates. The replacement must
therefore traverse the production presentation, Settings1, GlobalAccel, and
KScreenLocker authentication paths; no restart bypass is added.

Notification-host exit always ends the session. Failure to start the
replacement shell, exit of that replacement, explicit supervisor stop, or
supervisor/KWin death closes the token, stops the remaining child, and ends the
session. A shell exit observed after the host process has already reached
`NotRunning` cannot consume the restart budget or start a replacement; host
health is a prerequisite at the exact restart decision. The restart budget is
never replenished inside one compositor session.

## Consequences

Active notification records and the standard freedesktop service survive one
shell failure. Shell-local Recent history, popup timers, in-flight operations,
and transient UI state do not survive; the replacement consumes the current
host snapshot as a fresh no-replay baseline. Persisted Do Not Disturb state is
reloaded and confirmed through Settings1 rather than copied from dead shell
memory. Lock privacy remains denied until the replacement independently
authenticates all three exact KWin-owned names and confirms unlock.

Keeping the token resident in supervisor memory slightly lengthens its
lifetime, but does not put it in argv, environment, a file, signal, or
diagnostic. Each child still receives only its own bounded pipe record, and
each replacement gets a fresh descriptor.

Supervisor process tests must prove resident-host PID continuity, replacement
PID change, the one-restart ceiling, fresh launch failure teardown, explicit
stop behavior, host-exit restart suppression, secret-free arguments, and
parent-death coupling. Nested
qualification must additionally leave one host record resident across restart,
prove the new shell baselines it as Active without Popup/Recent replay, and
close that exact record through the standard host path.

## Revisit when

Revisit this decision if the shell can checkpoint all security-sensitive
presentation state transactionally, if an external service manager can receive
the token without persistence or broader authority, or if measured crash data
supports a different bounded restart policy with equally clear failure
semantics.
