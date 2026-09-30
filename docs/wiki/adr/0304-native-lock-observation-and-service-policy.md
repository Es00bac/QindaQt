# ADR-0304: native lock observation and service policy

- **Status:** Accepted; observer slice, service policy and migration have separate gates
- **Date:** 2026-09-30
- **Owners:** native lock services and session lock state
- **Related:** [ADR-0299](0299-native-locker-private-launch-and-authentication.md)

## Context

Native protocol state is the compositor's lock authority. Privacy consumers and
sleep ordering need trustworthy readonly state without gaining the locker
connection, PAM result or unlock capability. A same-UID process or caller-selected
PID cannot establish the active compositor lineage.

## Decision

A QCore-compatible public readonly transport resolves NativeLock1 through the
bus daemon, then queries and subscribes only to its exact unique owner.
Constructor-injected admission must match an independently accepted live
attachment's unique owner and daemon-resolved PID. Recheck it before every
disclosure-admitting query, reply, signal and getter. Never replace this contract
with a supplied PID or a legacy service quorum.

Signal values only invalidate the snapshot. Fresh GetAll requires both native
boolean properties. Owner/generation/request serial fencing rejects stale
unlocked replies and queued signals from prior subscriptions. Unknown, denied,
lost or inconsistent state suppresses both ordinary content and protected-state
claims. Protected represents current-generation physical black/lock presentation,
including clientless crash fallback, rather than protocol acknowledgement.

The readonly port cannot request lock, authenticate, launch a greeter or unlock.
Request admission, ScreenSaver compatibility, sleep inhibition/order and Settings
persistence belong to separate collaborators through public interfaces. Sleep
policy must await authenticated Protected before admitting sleep; failure must
refuse it. No such policy is inferred from completion of this observer slice.

Qt's [asynchronous D-Bus connection API](https://doc.qt.io/qt-6/qdbusconnection.html)
and [message-bearing slot contract](https://doc.qt.io/qt-6/qdbusdeclaringslots.html)
supply the concrete transport. Callback captures and the borrowed same-thread
transport outlive the monitor; stopping revokes state and cancels pending work.

## Consequences and qualification

Existing quorum consumers remain explicit migration work until switched to the
native port. No readonly consumer receives a private locker FD or PAM approval
transport. Focused pure monitor and real private-bus tests cover admission,
replacement, stale work, loss and protection semantics with fatal Qt warnings.
They do not authorize live lock/sleep, alter kernel/PAM settings or qualify a
released package revision.

See [native session lock](../architecture/native-session-lock.md),
[module boundaries](../architecture/module-boundaries.md) and
[testing harness](../development/testing-harness.md).

[ADR-0305](0305-public-ordinary-compositor-attachment.md) supplies the public
selected ordinary attachment identity/lifetime boundary. Its independent socket,
kernel PIDFD and daemon lineage join explicitly admitted session selection; it
does not claim executable or independent supervisor-process attestation. Native
lock payloads are never a substitute for that attachment admission.
