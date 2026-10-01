# ADR-0325: Add bounded native sleep modes to Sleep1

- Status: Proposed (candidate implementation; independent review pending)
- Date: 2026-10-01
- Extends: [ADR-0321](0321-supervisor-owned-native-sleep-admission.md)
- Related: [Native session locking](../architecture/native-session-lock.md),
  [Native power preferences](../architecture/power-policy.md)

## Context

Settings1 already names suspend, hybrid-sleep, suspend-then-hibernate and
hibernate actions. Sleep1 only dispatched suspend, leaving later native policy
without a public protected boundary for the other modes. The existing Sleep1
wire has no numeric version property; SessionActions uses its fixed zero-input,
boolean-output CanSuspend/Suspend methods and exact Session1 owner join.

Installed systemd 261.2's `src/login/logind-dbus.c` declares four corresponding
Can methods (no inputs, string output), and four actions (boolean interactive
input, no outputs). Its capability evaluation includes the actual system-bus
sender's UID, policy authorization and mode support. See the
[primary systemd implementation](https://github.com/systemd/systemd/blob/v261.2/src/login/logind-dbus.c).

## Decision

Keep the versionless `org.qindaqt.Sleep1` name, path and existing signatures.
Add zero-input/boolean-output CanHibernate/Hibernate,
CanHybridSleep/HybridSleep, and CanSuspendThenHibernate/SuspendThenHibernate.
A bounded public C++ SleepMode enum selects only those four fixed pairs; no
caller-selected method string reaches logind. Existing requestSuspend APIs
remain compatible wrappers. Existing completion signals describe the sole
serialized sleep operation, irrespective of mode.

Every Can method resolves the actual session-bus caller UID and requires the
supervisor's current Session1 ownership. The coordinator then queries the
corresponding logind Can method through its admitted selected-session system-bus
connection. Thus logind/polkit sees the supervisor's actual process credentials;
Sleep1 does not impersonate the client's PID or forward a privileged identity.
Only same-UID callers are admitted. PID-specific polkit grants to another process
are not delegated through this boundary. Can is a current hint, not a reservation
or proof that a locker has protected presentation. Unlocked state may query it;
Unknown/Locking, loss, pending mutation, no delay FD and any answer except exact
string `yes` return false. Readonly work is bounded to 16 concurrent queries,
250ms credential and 250ms capability bus calls, with existing bounded owner
checks; stop replies false and fences late callbacks.

All four actions use the same owned delay descriptor, native request/epoch
correlation and current targeted Locked/Protected receipt gate. After physical
protection, repeat the exact mode's Can method, recheck protection and selected
authority, then dispatch only that mode with interactive=false. Challenge,
unsupported, absent, malformed or contradictory capability answers refuse.
One mutation is pending at a time. Cancellation retires its exact transport
serial before another explicit request can begin, preventing old Can/action
replies from dispatching or completing a different mode in the same logind epoch.
A dispatched operation with an error, malformed/lost reply or cancellation is
Uncertain and is never automatically replayed. No fallback to Suspend, direct
unprotected logind dispatch, alternate locker or shell command exists.

## Consequences and limits

Older clients retain the same Suspend ABI. Older Sleep1 services lack the added
methods; new consumers must handle UnknownMethod as unsupported. No invented
numeric version handshake or new required dependency/process is introduced.
Source-specific lid, critical-battery and idle policy remain separate work;
this prerequisite does not complete PF2 or advertise new Power1 scopes.
The finite external logind delay deadline, native resume rules, selected root
logind identity and ordinary descriptor lineage remain ADR-0321 contracts.

Disposable empty-activation brokers, an unavailable system bus and pipe-backed
FDs qualify the protocol without host power/lock/PAM operations. Hardware mode
support, hibernation images and real logind/polkit timing require later managed
installed/hardware qualification.

## Candidate verification

The immutable source passes strict focused compilation and the original seven
native sleep/SessionActions rows plus `qindaqt.sleep_modes`: 8/8 CTests,
143 Qt passes with 0 failures/skips. The [testing
harness](../development/testing-harness.md#native-sleep-mode-wire-gate) records
the required empty-activation namespace, actual caller/ordinary credentials,
initial broker setup failure and exact task-generated crash artifact handling.
Independent review and integrated rerun remain required before adoption.
