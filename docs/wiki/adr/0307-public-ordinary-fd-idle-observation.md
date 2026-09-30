# ADR-0307: Share readonly idle observation on an admitted ordinary display FD

- **Status:** Accepted; public extraction, native policy consumer migration separate
- **Date:** 2026-09-30
- **Owners:** native lock services and keyring consumers
- **Related:** [ADR-0305](0305-public-ordinary-compositor-attachment.md), [ADR-0304](0304-native-lock-observation-and-service-policy.md)

## Context

Native idle policy requires actual compositor idle signals. A local timer, service
name or standalone display-path connection cannot prove activity or the admitted
session's lineage. Reusing a daemon-private idle adapter couples unrelated policy.

## Decision

`QindaQt::IdleObservation` exposes thread-confined `IdleObservation` and
`WaylandIdleObservation` under `QindaQt::Platform::Idle`, with Qt Core public
headers and a private libwayland-client standard ext-idle-notify-v1 adapter.
Constructor-visible borrowed callbacks supply an admitted ordinary connected FD
and synchronous current attachment liveness. Captures outlive the observer.
The platform owns each supplied FD, including libwayland construction failure;
it never reconnects by pathname or receives a privileged locker descriptor.

Availability requires exactly one advertised wl_seat and one idle notifier.
Missing, duplicate, dynamically added ambiguity, bound-global removal, peer
failure or lost lineage revoke availability and idle immediately. A two-second
handshake deadline only revokes availability; no local elapsed time manufactures
idle. Timeout 0 disables; unsupported bounds fail closed. Reconnect follows an
explicit refresh or bounded listener-context rotation through the admitted
ordinary opener. No observation of an unselected or unknown seat is claimed.

Each timeout rearm advances a notification identity/generation. Old queued
notification events and reentrant timeout edits cannot contaminate the new
state. Invalidation precedes any deferred destruction during Wayland dispatch;
reentrant refresh remains a legitimate same-thread consumer operation and must
recover without stale callbacks cancelling it. Listener contexts are bounded per
connection; rotation at 64 prevents unbounded growth during repeated edits.

This port is readonly: policy, persistence, inhibitors and native lock/sleep
requests live in their owning consumers. Idle loss never unlocks or creates a
Protected claim. Manual locking and critical battery policy remain uninhibited
by ordinary idle inhibition, and native sleep ordering awaits admitted actual
Protected through the separate public observer.

## Consequences and qualification

Real private Wayland fixtures cover actual idled/resumed events, elapsed-time
non-authority, lineage revocation, missing and duplicate initial globals,
dynamically added second seat/notifier, global removal, peer loss, old queued
notification events, reentrant rearm/refresh and bounded context rotation.
They qualify protocol observation only, not native idle policy, logind inhibition,
physical hardware, deployed services or actual lock/sleep execution.

See [compositor attachment and idle observation](../architecture/compositor-attachment.md),
[module boundaries](../architecture/module-boundaries.md) and
[testing harness](../development/testing-harness.md).
