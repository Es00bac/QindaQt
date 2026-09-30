# ADR-0314: Authenticate native lock state through targeted receipts

- **Status:** Accepted
- **Date:** 2026-09-30
- **Owners:** Native lock services and qindaqt-kwin
- **Supersedes:** The GetAll success-reply trust mechanism in ADR-0304
- **Superseded by:** None

## Context

The native lock client needs a current authoritative snapshot and a request
admission result from the compositor that owns NativeLock1. Qt 6.11.1 exposes an
empty QDBusMessage service field for method replies. The session bus also permits
unrequested replies under its eavesdrop policy. A method reply's apparent
destination, signature or success therefore cannot bind the result to the
compositor owner.

The existing public ordinary compositor attachment remains the independent
admission boundary for selected session, actual socket/PIDFD identity, exact
unique owner and daemon-resolved process ID. Native lock payloads cannot replace
that admission. See [native session lock](../architecture/native-session-lock.md)
and [ordinary compositor attachment](../architecture/compositor-attachment.md).

## Decision

Keep legacy RequestLock, Locked/Protected properties and change signals for
compatibility. Add RequestLockWithReceipt(nonce), which returns no value and
sends lockAdmissionReceipt(nonce, admitted) as a D-Bus signal targeted to the
unique caller captured from the actual method-call context. Add
RequestStateWithReceipt(nonce), which returns no value and sends one targeted
stateReceipt(nonce, locked, protected) snapshot to that same caller. The producer
accepts only 32 lowercase hexadecimal nonce characters.

A consumer creates a fresh nonce, subscribes to the exact current compositor
unique owner before sending the call, and accepts only the matching signal from
that actual sender with the expected path, interface, member, signature and
nonce. It also requires the void method completion as transport progress. A
missing, late, duplicate, malformed or wrong-nonce receipt, owner replacement,
attachment change, bus loss or timeout yields uncertainty or Unknown. It never
replays a manual request. Ordinary attachment admission is rechecked at
submission and disclosure.

The producer uses a direct exported QObject with QDBusContext for these
methods. A QDBusAbstractAdaptor does not receive the actual caller context
needed to target the receipt. Receipt admission never means physical Protected;
only the authenticated state receipt can update the readonly snapshot, and only
an admitted attachment can disclose it.

## Consequences

The native session observer and manual request port use one nonce-correlated
transport rule while legacy clients retain their existing interface. The
compositor gains no unlock or authentication-result method. Receipt generation
stays on the compositor thread and targets only the calling bus connection.
Private-bus tests cover valid, rejected, forged-sender, wrong-nonce, duplicate,
missing and late receipts, attachment and owner replacement, plus physical
Protected separate from request admission.

The consumer requires Qt Core/DBus only. This does not claim executable
attestation, independent supervisor-process identity, live host locking,
physical DRM, installed package readiness or sleep denial.

## Revisit when

Revisit if QtDBus supplies an independently verified actual method-reply sender
and the session bus no longer permits unrequested replies, or if a protocol-level
authenticated request/reply transport replaces targeted signals.
