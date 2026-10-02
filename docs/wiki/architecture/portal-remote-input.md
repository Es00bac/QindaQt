# Native remote-input portal

`src/services/portal/remote_input` implements the standard
`org.freedesktop.impl.portal.RemoteDesktop` version 2, `InputCapture`
version 1 and `Clipboard` version 1 backends over the selected QindaQt
compositor's EIS engine. The resident composes it beside the
[native foundation](portal-foundation.md) and reuses its PortalRequests,
selected-session attachment, native-lock monitor and QindaTK consent helper.
[ADR-0335](../adr/0335-native-remote-input-portal.md) records the boundary.

**Routing is unchanged.** The resident exports all three interfaces, but
`qindaqt.portal` does not advertise RemoteDesktop, InputCapture or Clipboard and the selector keeps them on `kde` with the
[ADR-0088](../adr/0088-enable-kde-remote-desktop-for-qindaqt.md) drop-in until the
real frontend/private-compositor gate below passes and the gaps are closed.

## Source inputs and credits

The session, device and EIS flow is adapted from xdg-desktop-portal-kde 6.6.6
(`9a5cc0e8`) `src/remotedesktop.{h,cpp}`, keeping the upstream
LGPL-2.0-or-later notices of Red Hat Inc, Jan Grulich and Harald Sitter, and
`src/inputcapture.{h,cpp}`/`inputcapturebarrier.cpp`, keeping David Redondo's
`LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL` notice. KWin
process lookup, KNotification, the tray item, fake-input, restore data and the
Plasma dialog are replaced by native ports. The fork's `src/plugins/eis` engine
(David Redondo) is the only input transport. No KDE portal, KGlobalAccel or
Plasma process is a runtime dependency of the new adapter.

## Components

| Component | Responsibility | Borrowed boundary |
| --- | --- | --- |
| CompositorEis | Asynchronous `connectToEIS(i)->(h,i)` and `disconnect(i)` against the exact selected compositor unique owner, 5 s bound, late-reply disconnect | QDBusConnection and an owner provider (`PortalSessionBinding::compositorOwner`) |
| RemoteSessions (private) | Standard Session objects, Close/Closed, actor/app/PIDFD liveness, 60 s unstarted expiry | RequestRegistry |
| RemoteDesktopAdaptor | Standard methods, device validation, consent question, phase machine and EIS publication | RequestRegistry, AccessConsent, CompositorEis |
| InputCaptureAdaptor | Consent before arming, compositor zones, barrier rule, enable/disable/release, targeted Disabled/Activated/Deactivated/ZonesChanged | RequestRegistry, AccessConsent, CompositorEis |
| ClipboardAdaptor (private) | RequestClipboard/SetSelection/SelectionWrite(Done)/SelectionRead and targeted owner/transfer signals for consented RemoteDesktop sessions | RemoteDesktop sessions, AccessConsent, CompositorEis |

All objects are confined to the constructing Qt thread. The adaptor is
destroyed before the borrowed ports; destruction closes sessions and
disconnects their compositor contexts.

## Wire and lifetime

`AvailableDeviceTypes` is 7 (keyboard, pointer, touchscreen), matching the fork
EIS capability bits. CreateSession binds the session to the current frontend
owner, the caller encoded in the standard handle (same UID, live unique name
and PIDFD) and the app ID; at most 8 sessions exist. SelectDevices accepts
`types` (default all) and `persist_mode` 0–2; restore data is never trusted
and nothing persists, so every Start asks again. Start requires a selected
session and asks the native consent helper with the requested device list
(plus the clipboard choice below); Allow returns `devices` and
`clipboard_enabled`, Deny/Close cancel and close the session.

ConnectToEIS is accepted once per granted session from the frontend. It
requests a compositor context limited to the granted devices from the
attachment-proven compositor owner, never a well-known name, frontend option
or application-supplied socket. Publication rechecks session liveness, phase,
frontend owner and native admission; a late or retired transport is
disconnected and never delivered.

Session Close, frontend or caller loss, request cancellation, native lock or
lock uncertainty and selected-session/compositor loss close the session,
emit `Closed` when the backend initiated it, cancel visible consent and send
`disconnect(cookie)`. Notify* methods fail with
`org.freedesktop.DBus.Error.NotSupported`: the backend has no second,
non-EIS injection path. The real frontend forwards Notify* without returning
backend errors, so legacy Notify clients keep the KDE route until a backend
EIS sender exists.

## Screen sharing in RemoteDesktop sessions

xdg-desktop-portal 1.20 picks the ScreenCast and RemoteDesktop backends
independently and sends `ScreenCast.SelectSources` for a RemoteDesktop session
to the ScreenCast backend with the RemoteDesktop handle (`screen-cast.c`); it
refuses `persist_mode`/`restore_token` for such sessions itself. Upstream KDE
shares one session registry between the two portals. QindaQt keeps the two
module-private registries and joins them through `ScreenCastSourceDelegate`
(`capture_ui.h`): ScreenCastAdaptor validates the standard options, refuses
persistence for these sessions, and forwards the monitor selection (multiple,
cursor mode) to the RemoteDesktop owner, which accepts it once, from the same
frontend, caller and app, before Start.

Start then asks the input consent (which says screens are chosen next) and,
after Allow and any clipboard grant, hands the session to the same protected
capture producer ScreenCast uses (`CaptureUI`, `ProcessCapture`): its own
monitor choice and consent, its frozen selection and its stream lifetime. The
Start request stays pending until every selected stream is ready and then
publishes `streams` with `devices` and `clipboard_enabled` once; a single
selection cannot publish two nodes. A refused, cancelled or failed share
fails Start and closes the session, so input is never granted without the
requested streams. Every retirement (Close, frontend or caller loss, request
cancellation, native lock, compositor/session-binding loss) stops the
session's streams and disconnects its EIS context; a producer that stops or
dies closes the RemoteDesktop session. The composition constructs
RemoteDesktop before ScreenCast so the borrowed seam outlives its user.

## InputCapture

CreateSession requires `capabilities` (nonzero subset of 7) and asks native
consent before the compositor creates anything; Allow calls
`addInputCapture(u)` on the selected owner and returns `capabilities`. Zones
come from the compositor's `zones()` (`a(iiii)` logical output rectangles,
replicated outputs merged), the same coordinates its barrier spy enforces;
no display connection or ambient screen list is used. SetPointerBarriers
requires the session's latest `zone_set`, accepts at most 64 barriers and
keeps only those covering one full outer zone edge (upstream rule); the rest
return in `failed_barriers`. New barriers disarm an enabled capture.
Enable/Disable/Release and ConnectToEIS (once, while disabled) act only on the
session's own compositor capture object. Compositor `disabled`, `activated`
and `deactivated` signals are accepted only from the selected owner's capture
path and forwarded as targeted signals to the session's frontend; the
standard signals are declared for introspection but never broadcast. A
compositor zone change disarms every capture, clears barriers and zones and
emits ZonesChanged with a new `zone_set`. Session close, actor loss, native
lock or attachment loss call `removeInputCapture`. The compositor's
Meta+Shift+Escape escape hatch (KGlobalAccel-configurable inside the fork)
is unchanged.

## Compositor contract (fork `src/plugins/eis`)

- `connectToEIS` and `addInputCapture` admit only the current same-UID unique
  owner of `org.freedesktop.impl.portal.desktop.qindaqt`, or of the KDE
  backend name while the selector still routes these families to `kde`.
- `disconnect(cookie)`, `removeInputCapture` and per-capture
  connect/enable/disable/release accept only the creating sender.
- No transport is issued and no capture is armed or activated while natively
  locked; `screenAboutToLock` destroys RemoteDesktop contexts and disables
  every input capture.
- `InputCaptureManager.zones()` (portal backend only) returns logical output
  rectangles; `zonesChanged` follows output add/remove/geometry changes.
- `connectClipboard()`, `disconnectClipboard(i)`, `setSelection(i,as)` and
  `readSelection(i,s) -> h` (portal backend only, creator-only handles, at
  most 16) publish an `AbstractDataSource` as the seat selection. Paste FDs and
  owner changes travel only in targeted `selectionTransfer(i,s,h)` and
  `selectionChanged(i,as,b)` signals; owner loss and `screenAboutToLock` drop
  handles and withdraw their selection.

## Verification

`qindaqt.portal-remote-desktop` runs the adaptor on a private dbus-daemon with
a synthetic compositor object: grant/connect/Close with disconnect, denial and
Request.Close cancellation, foreign and unstarted refusal, invalid devices,
Notify refusal, native authority loss, frontend owner loss, late transport
after Close, compositor replacement, and Clipboard: explicit choice, owner
change, SetSelection, a real paste FD written through SelectionWrite, one-shot
serials, SelectionRead bytes, release on Close, declined choice and late
RequestClipboard refusal, and authority loss closing an unclaimed paste FD. `qindaqt.portal-input-capture` uses a
synthetic manager/capture pair: grant, zones, barrier acceptance/failure,
single EIS receiver, enable, targeted activation, release, deactivation and
Close removal; denial; zone change disarming; native authority loss; and a
forged activation from another peer. `qindaqt.portal-remote-screencast`
drives the frontend's combined sequence over both adaptors with a fake capture
port: consent before producer, frozen request (session, caller, multiple,
cursor mode), atomic `streams`/`devices`/`clipboard_enabled` publication,
stream stop and EIS disconnect on Close, cancelled and over-full shares
closing the session, owner/app/once/persistence fencing of the selection, and
producer close, capture authority loss and caller loss ending the session.
They prove wire and lifetime, not physical input or real PipeWire nodes.

Candidate evidence (strict `-Werror` dev build, configured `-j24 -l24`): both
focused rows pass (Qt 12/0 and 7/0, also with `QT_FATAL_WARNINGS=1`) beside
portal-access, portal-service, portal-process-lifecycle and both
source-boundary rows. The real resident on a zero-activation private bus
exports all three interfaces, and all 37 standard members match the installed
backend XML wire signatures. The fork EIS edits pass `-fsyntax-only` with the
configured fork flags; the plugin itself has not been built or run.

Still required before routing changes: the fork plugin build plus a private
native compositor row where a real frontend session receives an EIS FD,
injects observable input, and loses it on Close and native lock; staged
metadata, and a real frontend RemoteDesktop+ScreenCast session receiving
producer nodes. Remaining source gaps: a backend EIS sender for Notify* and
RemoteDesktop persistence/restore tokens.

## Clipboard

The standard `Clipboard` version 1 adjunct is a private adaptor owned by
RemoteDesktopAdaptor on the same host, sharing its sessions; the clipboard
method/session rules are adapted from upstream `clipboard.{h,cpp}` (David
Redondo's LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
notice kept). `KSystemClipboard`/`QMimeData` copies are replaced by a
compositor-owned clipboard handle, because the protected resident cannot be
admitted to `ext-data-control-v1` (restricted globals resolve through
`/proc/<pid>/exe`, as recorded for [capture](../reference/portal-capture.md))
and the native [clipboard service](clipboard-service.md) exposes no payloads.

RequestClipboard is accepted only before Start from the session's frontend.
Start then adds a separate clipboard choice, off by default, to the native
consent; only an explicit `true` opens a compositor handle, and
`clipboard_enabled` is published after that handle exists. SetSelection makes
the session's MIME list (at most 64, each 1–255 characters) the seat
selection. A Wayland paste reaches the backend as the requester's own write
FD; the backend keeps at most 16 unanswered FDs per session, emits
SelectionTransfer with a new serial, and SelectionWrite hands that one-shot FD
to the writer, so no clipboard bytes pass through the resident.
SelectionWriteDone or retirement closes an unclaimed FD (the requester reads
EOF). SelectionRead returns a pipe the compositor fills from the current seat
selection, rechecked against the live grant at publication.
SelectionOwnerChanged carries `mime_types` and `session_is_owner`. Signals are
targeted to the session's frontend. Session close, actor loss, native lock or
attachment loss release the handle and withdraw the session's selection.
