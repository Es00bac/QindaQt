# ADR-0318: Compose native portal families through explicit lifetime ports

- Status: Accepted source design; routing and installed delivery remain gated
- Date: 2026-09-30

## Context

The Plasma-free roadmap needs native consent, notification, email and inhibitor
behavior while preserving the standard portal frontend and the existing Settings
and Secret contracts. A backend must not derive display authority from sandbox
payloads, invent success for unavailable native capabilities or enlarge Settings
presentation into a launcher/service framework.

## Decision

Compose separate bounded request transport, pure family policy, native platform
ports and a small ownership factory in `services/portal`. Use actual current
frontend unique-owner admission and standard Request.Close cancellation. The
frontend supplies case-sensitive app IDs, including empty host IDs where the
primary contracts allow them. Secret keeps its distinct empty-ID policy.

A public native Portal1 control retains the first admitted selected-session owner
and attaches through public CompositorAttachment. Access runs a separate QindaTK
consent helper on a newly owned ordinary FD. Confined xdg-foreign-v2 platform
integration must process parent import before enabling consent and retire on
parent loss. Native Unlocked receipts gate Access/Email and publication.

Notification bridges the production native Notifications host, preserving
application/id namespaces and targeted action results. Support bounded sealed
PNG/JPEG icons; reject unsupported data instead of promising rendering or sound.
Email uses a new independent ApplicationUriOpener service borrowing public
Default applications persistence/catalog. Extend the existing Launcher input
DTO append-only with encoded URLs; never invoke a shell. A small nonprivileged
one-use relay preserves the admitted FD across exec.
[Qt's QProcess contract](https://doc.qt.io/qt-6/qprocess.html#startDetached)
states that startDetached ignores childProcessModifier, so it cannot carry this
explicit descriptor inheritance boundary directly. The relay owns no resident
policy, service lookup or install authority.

Portal idle inhibition requires the complete public native idle-stage mask;
zero or partial capability fails. Session monitoring and unsupported flags fail
explicitly. The current real producer advertises zero scopes. Route Access, Notification and Email to the native backend through explicit
metadata and real-frontend qualification. The supervisor owns one retained
session connection plus optional backend child; backend replacement reattaches
the same selected ordinary display, while session loss revokes authority before
child teardown. Keep Inhibit on KDE until actual native consumers qualify its
complete scopes. No generic success stubs or restore tokens are permitted.

## Consequences

Settings appearance libraries and the process-free Default applications route
retain their boundaries. Standard Secret Service/Secret portal, power policy,
compositor implementation and supervisor wiring remain independently owned.
The consent helper adds Qt/QindaTK GUI runtime and the confined parent adapter
adds private Qt QPA plus generated primary xdg-foreign protocol integration;
those dependencies do not enter pure policy or the resident GUI stack.

Compatibility limits and exact request/FD/handoff lifetimes are recorded in the
[native portal foundation](../architecture/portal-foundation.md). URI handoff
cannot recall an independent application after successful exec. Nonempty Email
attachments, unsupported notification payloads, unavailable idle scopes and
session-monitor calls remain explicit failures. Private native tests cannot
establish installed routing, physical UI or PAM authorization. No new package
installation or resident KDE/GNOME runtime dependency is introduced here.
