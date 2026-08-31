# ADR-0053: Compose Display1 from authenticated runtime authorities

- **Status:** Accepted
- **Date:** 2026-08-31
- **Owners:** Display platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0016](0016-display1-transaction-authority.md) makes the resident Display1
process QindaQt's only display-transaction authority, while
[ADR-0050](0050-direct-kde-output-management-writer.md) and
[ADR-0051](0051-persist-display-journal-in-injected-state-root.md) deliberately
leave process composition, state-root selection, lock authentication, and
suspend coordination outside the D4 writer and D5 store. Starting those
adapters independently is unsafe: a rejected recovery file must prevent any
forward mutation connection, an unauthenticated unlocked signal must not admit
a preview, and sleep can begin while a forward result is still uncertain.

The accepted lock monitor authenticates three session-bus names against an
independently supplied KWin PID. Display1 is D-Bus activated rather than a
supervisor child, so it does not receive the shell's witnessed PID argument.
Trusting a PID obtained from one of the same bus names would make that check
circular. The direct D4 Wayland connection already has a Linux peer credential
for the exact compositor endpoint it can mutate.

## Decision

Add a focused `display_runtime` process-composition module. It owns no display
model, protocol mapping, persistence format, or compositor object. It composes
the existing D2 resident service, sole D4 writer, D5 store, authenticated
session-lock monitor, and one exact-owner systemd-logind sleep-delay adapter.
Those modules retain their separate public boundaries and lifetimes.

The process selects one concrete state root before constructing D5. An explicit
command-line path has priority, followed by systemd's single
`STATE_DIRECTORY`, `XDG_STATE_HOME/qindaqt`, and `HOME/.local/state/qindaqt`.
Every selected path must be absolute, clean, and an existing safe directory;
D5 remains the final ownership, symlink, and permission authority. The systemd
user unit creates its `qindaqt` state directory with mode `0700`. D5 loads
before the Wayland writer starts. Rejected or unsafe truth is retained and
startup stops without a mutation-capable connection.

On Linux, D4 reads `SO_PEERCRED` from its private Wayland socket and exposes
only the positive peer PID through its abstract port. D6 supplies that PID to
the existing three-name lock monitor. The monitor must still bind
`org.qindaqt.Compositor`, `org.freedesktop.ScreenSaver`, and
`org.kde.screensaver` to one exact unique owner whose bus PID equals the
Wayland peer. No environment PID or name-derived self-assertion is accepted.

The logind adapter binds `PrepareForSleep` and `Inhibit` to one exact unique
system-bus owner. It acquires a `sleep`/`delay` file descriptor before the
resident publishes mutation authority. Display1 is `Safe` only while all three
facts hold: the D4 output-management generation is available, the authenticated
lock state is `Unlocked`, and the current logind delay descriptor is held.
Unknown, locked, writer-loss, resume-before-reacquisition, and incomplete
startup truth are non-safe.

`PrepareForSleep(true)` revokes safety and routes the active D1 machine through
`prepareForSuspend`. The runtime keeps the delay descriptor while a forward
apply is in flight, then releases it as soon as no forward request remains.
Rollback may continue after release; its durable journal is the restart/resume
authority when logind's delay window is shorter than convergence. Resume must
reacquire the descriptor before safety can return. Logind owner replacement,
system-bus loss, malformed replies, or failed reacquisition are terminal for
that process; a fresh activation must establish new authority.

A valid startup journal is passed to D1 `recover` on the first complete D0
inventory rather than initialized away. If inventory becomes unavailable or
changes exact owner, the service captures any active journal before discarding
the old machine. A stale, contradictory, or malformed complete read from the
same owner is rejected while preserving the prior public truth and active
machine; its diagnostic cannot masquerade as an authority-loss edge. The next
complete lineage after actual loss recovers the journal under a new outer
lineage. Rejected journal truth is never cleared, and owner/epoch replacement
never authorizes forward replay. Late writer completions remain fenced by
D4's copied machine/token/request/owner tuple.

## Consequences

- D2 remains independent of files, Wayland, logind, and lock transport; D4 and
  D5 remain independently testable and have no environment lookup.
- The packaged writer is Linux/Wayland session composition. Failure to obtain
  a positive Wayland peer credential, a safe state root, a delay descriptor,
  or a complete inventory keeps Display1 unavailable or non-safe.
- Same-process D0 owner replacement retains recovery authority without keeping
  an old machine or accepting a late completion. Process restart repeats the
  same rule from D5's canonical bytes.
- Deterministic fake and private-bus tests can prove startup order, lock/logind
  fencing, suspend release, restart recovery, and zero-forward-apply behavior.
  They do not prove KWin convergence. The serialized contained nested D6 row
  remains mandatory before claiming operational compositor behavior.
- A compositor peer PID authenticates only this Display1 composition. It does
  not replace the supervisor-witnessed PID contract used by the shell in
  [ADR-0011](0011-gate-notifications-on-authenticated-lock-state.md).

## Revisit when

Revisit if Wayland exposes a portable authenticated peer identity, Display1
becomes a directly supervised child with an equivalent witnessed credential,
logind changes the delay-inhibitor contract, or the compositor provides one
public crash-safe preview/rollback transaction that supersedes Display1's
journal authority. Convenience activation or a bus-name-only PID is not enough.
