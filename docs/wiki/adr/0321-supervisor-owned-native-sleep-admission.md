# ADR-0321: Supervisor-owned native sleep admission

- Status: Accepted
- Date: 2026-10-01
- Supersedes: the pending suspend composition consequence of ADR-0304
- Related: [ADR-0294](0294-native-session-lock-authority.md),
  [ADR-0314](0314-native-lock-receipt-authentication.md),
  [Native session locking](../architecture/native-session-lock.md),
  [Power service](../architecture/power-service.md)

## Context

The native runtime already waits for authenticated physical protection, but the
shell's SessionActions client sent `login1.Manager.Suspend(false)` directly.
Power1's read-only sleep projection cannot own a delay inhibitor or authenticate
selected-session logind Lock/Unlock signals. A compositor lock admission receipt
is insufficient: the protected presentation receipt is the sleep boundary.

## Decision

The supervisor composes the public `QindaQt::NativeSleep` target. A narrow
`LogindSleepTransport` owns system-bus adaptation and one CLOEXEC logind sleep
**delay** inhibitor. A separate `SleepCoordinator` borrows the native runtime and
readonly monitor. A `SleepService` exports `org.qindaqt.Sleep1` at
`/org/qindaqt/Sleep1`; SessionActions uses this manual suspend handoff. Policy,
logind IPC, facade residency and composition remain separate files.

The production composition explicitly selects `XDG_SESSION_ID` and the actual
supervisor PID/real UID. No absent-ID fallback is allowed. Bounded asynchronous
bus-daemon queries resolve the exact root-owned logind unique owner. Its
`GetSession(id)` path must be a login1 session object and equal
`GetSessionByPID(supervisorPid)`. `Id` and the actual `(uo)` `User` property must
match the selected ID, real UID and canonical user object path. Production
expects logind UID zero; private tests inject their fixture UID through the
constructor, never through an environment switch. The readonly same-thread
admission callback joins the already accepted ordinary compositor attachment
and live Session1 supervisor owner. Loss revokes authority and closes the FD.
Late replies and descriptors cannot enter a retired generation.

Exact-owner selected-session `Lock` requests native manual admission. `Unlock`
only refreshes observation and never authenticates or calls native unlock.
`SetLockedHint(true)` follows authenticated native Locked/Protected state;
`false` follows authenticated Unlocked. Unknown and Locking never synthesize an
unlocked hint. Neither hint supplies native protection authority.

Manual Sleep1 `Suspend() -> bool` admits the actual daemon-resolved same-UID
caller, serializes one request, and returns true only after the bounded logind
Suspend reply. `CanSuspend() -> bool` is a presentation hint requiring an owned
inhibitor and current Unlocked or protected Locked state. Unknown/Locking refuses
manual dispatch. The coordinator asks the native runtime to protect an unlocked
session; logind CanSuspend and Suspend are reached only after the current native
protected receipt. Protection is rechecked after the asynchronous CanSuspend
reply. SessionActions joins Sleep1 and Session1 unique owners, repeats admission,
and never falls back to direct logind Suspend. Reboot/PowerOff retain their
existing independent authority.

Exact-owner `PrepareForSleep(true)` runs the same protection gate and releases
the delay FD only while the authenticated native presentation remains protected.
Failure, unknown state, incomplete locking or absent receipt retains the FD;
no timeout invents success. `false` feeds the runtime's confirmed lock-on-resume
policy, cancels stale sleep work and rearms a fresh delay FD. Stop, supervisor or
ordinary peer loss, bus loss and logind replacement close retained descriptors
and invalidate late replies.

## Consequences and limits

A logind delay inhibitor has a system-defined finite deadline. QindaQt cannot
veto a privileged external sleep request after that deadline. Retaining the FD
on a failed native lock expresses refusal within that delay; it is not a claim
that external suspension is impossible. No stock locker fallback exists.

This slice owns manual and actual system sleep ordering. It does not implement
source-specific idle-suspend/lid/power-button policy. Power1's advertised idle
inhibitor scopes remain zero until every required consumer is qualified.

Wire tests use disposable private brokers, actual pipe-backed Unix descriptors,
a real ordinary socket/PIDFD attachment and production targeted native nonce
receipt transport/request ports. They never connect to the host system bus,
lock the host or invoke its power operations. These are process/wire gates;
physical output protection, real logind timing, PAM and hardware resume still
require the separately managed nested/installed/hardware qualification.
