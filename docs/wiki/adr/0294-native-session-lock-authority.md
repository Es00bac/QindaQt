# ADR-0294: native protocol state owns the session lock

- **Status:** Accepted, implementation candidates require separate evidence
- **Date:** 2026-09-29
- **Owners:** QindaQt compositor fork, native locker, session services
- **Supersedes:** ADR-0132 and ADR-0091 for lock authority and transport after native integration; their existing delivery history remains unchanged

## Context

Plasma-free QindaQt requires its own lock protocol server, greeter and service
policy. The standard Wayland `ext-session-lock-v1` protocol defines presentation
ordering and fail-closed resource semantics. An ordinary full-screen surface,
a client-provided app ID, or a timeout cannot establish that the session is safe.
The server can enforce the lock but cannot infer authentication performed in
another process.

## Decision

The fork implements the standard protocol and makes its native controller the
sole compositor lock authority. Only the fixed installed root-owned
`qindaqt-lock` executable, running as the session UID and requesting the public
Wayland interface permission, may see and bind the global. The server verifies
peer executable inode/device and the root-owned non-writable file hierarchy;
the general debug permission bypass and sandbox app IDs confer no access.
Unlock revalidates the current peer. It is accepted only from the current
protocol lock object after that object received `locked`.

The controller fences ordinary scene items and keyboard, pointer and touch
input immediately on acquisition. It sends `locked` only after actual protected
frame presentation on every enabled desktop physical output, including mirrored
outputs. Each acquisition/recovery carries an epoch; old presentation feedback
cannot satisfy a new fence. Black fallback is a valid protected presentation,
not an authentication result. Greeter loss removes greeter graphics and keeps
the fence. A newly authorized client may recover that locked session. Output
hotplug and input-method focus must preserve the same invariant.

The native greeter owns PAM authentication and account validation. Only their
success permits it to request protocol unlock. Authentication failure,
cancellation or crash never unlocks. The server never receives a client boolean
claiming that PAM succeeded, and makes no claim to independently authenticate
it. The trusted native client boundary is the cross-process contract.

## Consequences

PF5 is the executable protocol/server boundary. PF6 must pass private virtual
compositor input, protocol-error, crash and topology gates before the fork drops
its KScreenLocker build dependency. PF7 supplies the greeter/PAM and its
root-owned desktop permission entry; PF8 supplies Lock1, idle/inhibitor,
ScreenSaver and session-service adaptation. These separate boundaries require
separate evidence; none alone means full Plasma independence or live readiness.

Test authorization exists only in a build that refuses installation, restricted
to selected private socketpair peers. Its runner isolates HOME/XDG paths and
D-Bus. No production environment switch grants the protocol. Dummy clients do
not claim to test PAM.

See [Native session locking](../architecture/native-session-lock.md) and the
[testing harness](../development/testing-harness.md).
