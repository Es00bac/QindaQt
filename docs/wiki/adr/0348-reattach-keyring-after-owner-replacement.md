# ADR-0348: Reattach keyring after native owner replacement

- **Status:** Accepted
- **Date:** 2026-10-04
- **Owners:** Platform session supervisor and native keyring
- **Supersedes:** None; extends the lifecycle in ADR-0296
- **Superseded by:** None

## Context

The [native keyring](../architecture/keyring-daemon.md) admits a selected
session's retained bus owner and ordinary compositor display. Its screen and
idle observers require that display attachment; ambient Wayland environment
alone does not select them. A daemon replaced during an otherwise retained
session loses that admission. Startup-only supervisor retries leave the new
daemon without observers, so enabled fail-closed policy can immediately relock
a collection after an accepted password. Restarting the desktop or disabling
policy is not a supported recovery contract for a daemon replacement.

## Decision

The [session supervisor](../architecture/compositor-session.md) retains its
existing dedicated unique bus connection and watches the native Keyring1 name.
Every different current owner starts a fresh bounded batch of at most thirty
asynchronous attachment attempts, with a 500 ms call timeout and one in-flight
request. The retained login display declaration is sent only to the exact
current same-UID unique owner without auto-start. Legacy AttachSession remains
compatible when login declared no display. The daemon's existing first-owner
and public ordinary display/peer/lifetime checks remain authoritative.

Ownership replacement clears previous admission and retires in-flight reply
generations. A successful reply is accepted only while its selected owner and
UID remain current. Queued obsolete loss/arrival signals query current ownership
instead of replaying an already admitted generation. Exhausted admission does
not retry indefinitely for one owner; a new owner receives its own bounded batch.

A confirmed missing name and a transport lookup failure are distinct. Unavailable
owner/UID metadata cannot admit attachment or Shutdown, but retains the bounded
retry window; confirmed absence retires the selected generation and a real UID
mismatch refuses it. An owner-arrival hint may start a fresh retry window when
metadata is unavailable, but every actual request still needs current owner and
same-UID lookup. A private-bus interruption/recovery test covers a timeout with
no later owner-change event.

Stop and destruction retire the watcher, timer and pending callbacks before
waiting for the optional child. Shutdown is sent only to the exact still-current
unique owner that accepted this session, never a well-known-name replacement
or an unadmitted daemon. The dedicated connection is then released, preserving
the daemon's session-owner-loss termination boundary from
[ADR-0296](0296-native-keyring-daemon-boundary.md).

## Consequences

- Replaced daemons reacquire ordinary display and observer admission while the
  original desktop and its lock/idle preferences remain intact.
- Admission still proves ordinary same-user identity and lifetime; it adds no
  executable attestation, password authority, policy bypass or new process.
- Focused private-bus tests must exercise replacement, bounded refusal, stale
  replies, stop races, stable ownership and retained caller disconnection.
  A software compositor fixture must verify actual resident observer recovery.
- Source tests prove reconnect and policy availability, not live credentials,
  authentication success, physical display behavior or installed adoption.
  An already running old supervisor is not changed by installing this source.

## Revisit when

The native session or keyring admission protocol changes its caller lifetime,
display declaration, daemon replacement or logout contracts.
