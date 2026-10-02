# ADR-0342: Route native portals to their owning process

- **Status:** Proposed native delivery checkpoint; installed qualification pending
- **Date:** 2026-10-02
- **Owners:** Program manager and Platform workgroup
- **Supersedes:** ADR-0133 provider table and ADR-0088 KDE identity drop-in

## Context

The complete native families are integrated as separate modules. ScreenCast
and RemoteDesktop share session ownership through the existing source delegate.
The protected broker now composes both over AuthorityCapture (ADR-0341).
Keeping a resident copy or routing those two to different providers loses that
session relationship and leaves ordinary ProcessCapture without authority.

## Decision

Expose thirteen standard implementation interfaces on the ordinary resident:
Settings, Secret, Access, Notification, Email, FileChooser, AppChooser, Inhibit,
Print, GlobalShortcuts, Usb, Account and DynamicLauncher. Remove its capture and
remote-input adaptors and ordinary ProcessCapture. Retain existing constructor
signatures for source/link compatibility; their capture executable is ignored.

Advertise Screenshot, ScreenCast, RemoteDesktop, InputCapture and Clipboard
only through `qindaqt.capture.portal`, whose bus name is
`org.freedesktop.impl.portal.desktop.qindaqt.capture`. Install that declaration
only when the existing protected broker is built. All five selector rows use
`qindaqt.capture`; the remaining thirteen use `qindaqt`. Keep Wallpaper and
Background explicitly closed and `default=none` for every unlisted family.

Publish no capture activation descriptor. The compositor's existing exact
fixed path and inherited capability still launch and authorize capture; an
ordinary session-binding display never grants pixel authority. Native EIS uses
the exact current same-UID protected backend owner and its existing context
and native-lock checks. Remove installation of the KDE identity drop-in.

## Verification and consequences

Source/staged metadata checks pin both declarations and every selector row.
Mutation controls retain explicit interface/routing withdrawal; the real
frontend routing fixture owns a separate native capture name while competing
legacy declarations remain available, so selection must prefer only the native
owner. These routing fakes prove dispatch, not consent, pixels or input.

Coherent native gates separately require genuine combined sharing/input,
clipboard directions, barrier/release, Close, native lock and explicit
standalone remember/restore. Portage image/ownership/signature, dependency
closure and installed session checks remain required before release completion.
No completion follows from source routing alone.

See [Portal service](../architecture/portal-service.md),
[Portal foundation](../architecture/portal-foundation.md), and
[exact selection reference](../reference/portal-settings-backend-v1.md).
