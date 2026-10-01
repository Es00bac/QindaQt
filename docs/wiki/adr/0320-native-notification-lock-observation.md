# ADR-0320: Gate notification disclosure on native compositor receipts

- **Status:** Accepted
- **Date:** 2026-10-01
- **Amends:** [ADR-0011](0011-gate-notifications-on-authenticated-lock-state.md) for the native-lock production consumer

## Context

The native compositor owns NativeLock1 state receipts, while qindaqt-session owns the compatibility ScreenSaver facades. The legacy notification monitor required both facades and the compositor to share one D-Bus owner. That quorum cannot admit an unlocked native session.

## Decision

The shell privately composes the public ordinary compositor attachment with the public native lock transport and monitor. Its independently supervisor-provisioned compositor PID, actual compositor unique bus owner, selected Session1 owner and kernel ordinary socket peer must agree on the same actual bus daemon. The selected Session1 owner is pinned for this composition's lifetime; ordinary attachment establishes identity and lifetime, not executable attestation or locker authority. Only an authenticated native Unlocked receipt opens notification disclosure. The ScreenSaver facade's owner is irrelevant to this proof.

Unknown, Locking, Locked, malformed or missing receipt, ordinary peer loss, selected session loss and compositor replacement deny disclosure. Revocation refreshes the monitor and getters recheck the public attachment before returning Unlocked. No legacy fallback or silent replacement owner is selected. A new compositor needs an explicitly constructed observer with its supervisor-provisioned PID. Failed observation does not terminate the shell; notification privacy remains closed.

The observer owns its attachment, transport and monitor, uses the shell thread, stops the monitor before releasing borrowed dependencies, and exposes only the notification admission Boolean. It does not acquire a privileged display, request lock/unlock, process credentials, read service storage or link a resident service implementation.

Production NotificationPrivacyPolicy borrows the observer through a constructor-visible,
read-only admission callback. Its cached signal decision alone cannot authorize a
disclosure: every policy read rechecks live native admission. The controller composes
that public decision into each active/popup/history model's read boundary, including
retained model pointers and indices, and gates center/operation admission. Unknown
authority returns no role data or entries before queued watchers clear stored models.
Callbacks do not dispatch events or mutate consumers; storage clearing remains the
normal denial signal's responsibility, avoiding model reset inside a role read.
An admission exception fails closed. These public source interfaces require consumers
to rebuild; their existing signal-only constructors remain available to deterministic
or independently scoped consumers.

## Consequences

The native supervisor and compositor may retain separate owners without suppressing all notification presentation. Standalone or unauthenticated shells still cannot disclose. The legacy monitor remains available to independently scoped legacy consumers during migration.

## Verification

Focused private-bus fixtures exercise actual targeted native receipts and kernel ordinary socket peers with separate compositor and supervisor owners, including Locking/Locked transitions, trusted-PID mismatch, initial missing receipt, selected Session1 replacement, same-PID compositor owner replacement and stop invalidation. Selected-owner replacement must deny the production policy and retained model role before an event-loop turn. Focused controller coverage checks all retained projections, center and operation admission before queued denial, plus normal invalidation. Adjacent public attachment/native monitor and notification gates remain required. Physical installed-session privacy is a release qualification; these fixtures do not claim it.

## References

- [Notification presentation](../shell/notification-presentation.md)
- [Module boundaries](../architecture/module-boundaries.md)
- [Public ordinary attachment ADR-0305](0305-public-ordinary-compositor-attachment.md)
- [Native receipt authentication ADR-0314](0314-native-lock-receipt-authentication.md)
