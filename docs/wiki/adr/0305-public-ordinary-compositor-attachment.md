# ADR-0305: Share ordinary compositor attachment identity and lifetime

- **Status:** Accepted; public extraction and consumer migration have separate gates
- **Date:** 2026-09-30
- **Owners:** native lock services and native keyring consumers
- **Related:** [ADR-0304](0304-native-lock-observation-and-service-policy.md), [ADR-0299](0299-native-locker-private-launch-and-authentication.md)

## Context

Keyring prompts, native lock observation and idle policy need the same explicitly
selected live compositor lineage. Sharing a daemon's private attachment header
breaks the module boundary; an executable or service name alone does not prove
the connected display's identity or continued life.

## Decision

`QindaQt::CompositorAttachment` is a public Qt Core/DBus platform target.
`CompositorAttachment` receives a constructor-visible readonly admission callback
selecting an already accepted session unique owner. Callback captures outlive the
same-thread QObject. The owning consumer keeps its existing first-owner session
policy and any legacy fallback; neither is invented or exported by the platform.

Attach validates the canonical native socket basename in an owner-only 0700
runtime directory, walks its directory hierarchy without following symlinks,
and opens the actual ordinary UNIX socket through the pinned directory FD. The
socket must belong to the session UID and its kernel SO_PEERCRED PID must match
the bus daemon's exact compositor unique owner/PID. SO_PEERPIDFD and the retained
ordinary probe socket fence process death and disconnect; bus ownership and
session liveness are rechecked synchronously before identity getters and new
connections. A caller-supplied independently accepted owner/PID expectation is
additionally enforced when available, never fabricated from an untrusted native
lock payload. Admission loss immediately makes getters/open unavailable before
the queued owner watcher or periodic check runs.

This is selected ordinary identity/lifetime proof, not executable attestation or
independent supervisor-process attestation. Same UID or a service name alone
cannot attach; explicit session admission, actual peer proof and live exact bus
lineage are joined. The accepted keyring session binding remains the policy root;
PAM system-owner authentication and private locker executable trust remain
separate stronger contracts. No privileged locker FD, authentication decision,
RequestLock or unlock capability crosses this public boundary.

`openConnection()` transfers one CLOEXEC ordinary connected FD or returns -1.
The caller consumes that descriptor and never reconnects by pathname. The
platform has no legacy sentinel or fallback. Attach/revoke replace the generation;
reentrant revocation cannot report a successful attachment.

## Consequences and qualification

Focused private-bus/socket fixtures cover explicit admission, optional expected
owner/PID, same-PID different bus owner, unsafe paths, symlink/canonical-name
rejection, same-UID different actual peer, pathname replacement, synchronous
revocation and actual kernel peer death. Consumers continue owning their policy
and migrate independently. This source boundary installs no daemon or system
configuration and does not qualify a live desktop, executable trust or sleep.

See [compositor attachment and idle observation](../architecture/compositor-attachment.md),
[module boundaries](../architecture/module-boundaries.md) and
[testing harness](../development/testing-harness.md).

Manual native request consumers also join actual bus daemon identity through
`sameBus(connection)` using bounded 250 ms GetId calls. Owner/PID names can
collide between independent daemons. A queued initial advertisement cannot
revoke the same owner already admitted by attach; actual retained-owner loss
still revokes even if it reclaims the name before queued delivery.
