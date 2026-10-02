# Public compositor capture boundary

`src/services/compositor_capture` owns the pixel request, raw image decoder,
geometry and compositor transport shared by [Screenshot](../apps/screenshot.md)
and the [portal capture candidate](../reference/portal-capture.md). The process
choice is recorded in [ADR-0324](../adr/0324-native-portal-capture-boundaries.md).

## Public interfaces

`QindaQt::CompositorCapture` exports value-only request/geometry/decoder headers,
`CapturePort` and its restricted D-Bus `KWinCapturePort`. The decoder bounds raw
payloads before allocating/copying images. The port owns an asynchronous pipe,
expects a complete raw response and reports one decoded result or failure. It
has one operation at a time; cancel closes the operation and suppresses late
replies. Each call carries a default legacy10s pipe grace; protected authority
consumers explicitly choose the frozen30s pending-call/total with zero grace.
The monotonic send-time deadline is checked during drain, reply and decode,
so queued timer delivery cannot admit late bytes. Legacy interactive120s remains. All calls and signals stay on the creating Qt thread. Request policy
and transport do not grant application permission or show consent.

Screenshot's old private include paths remain thin compatibility wrappers. Its
command-line, save policy, capture flow, UI and recording/OBS boundary remain
app-owned. Consumers include public module headers, never app-private headers.

`QindaQt::WaylandScreenCast` owns a supplied native Wayland FD (ordinary or separately tagged) and generated
installed screencast/xdg-output protocol objects. It enumerates actual named
monitor sources and their logical position/size. It requires same-thread,
read-only borrowed callbacks for current compositor lineage and pixel permission;
they outlive the port. Enumeration requires lineage; creation, node publication
and an active stream additionally require pixel permission. No ambient display
name, executable lookup, authorization database or persistence is consulted.

The port accepts one to sixteen distinct IDs in the live offered monitor set
and one standard Hidden, Embedded or Metadata cursor mode. It validates the
whole selection before sending any producer request. Each stream publishes its
actual compositor-created PipeWire node; any member failure closes the whole
batch. The portal helper waits for every node before publishing the batch. The port never restarts or replays consent. Output removal, protocol
failure, deadline or authority loss invalidates getters and closes all owned streams;
teardown is deferred out of protocol listeners to preserve callback lifetime.
Enumeration and node results are bounded; late creation cannot resurrect a closed
port. The public port has no portal Session, app permission, GUI or PipeWire remote
policy. The standard portal frontend owns the remote and node permissions.

## Native authority and compatibility names

The qualified fork690 still exports its legacy ScreenShot2 interface and
`org.kde.KWin` owner alongside the native compositor identity. Public
`CompositorNames` constants describe those deliberate PF21 carve-outs. Screenshot
transport verifies that restricted screenshot and native compositor owners agree
both at send and before final decoding/publication; a replaced native owner
cannot publish an old process's still-live compatibility response;
there is no invented renamed interface or ambient legacy fallback.

The fork refuses screenshot and stream creation while native lock is active,
refuses screenshot pixel production while locked and closes existing streams
synchronously on `screenAboutToLock`. Direct stream sources black-fill if locked.
Portal consumers additionally recheck nonce-authenticated native receipts and
selected peer/owner/PIDFD before exposing results. Those complementary checks do
not recall pixels a consumer already copied or a stream buffer already received.

## Qualification boundary

Extraction keeps existing Screenshot focused tests applicable through the public
boundary. The new portal first slice requires independent wire/actor tests plus
actual mapped native GUI input, private EGL-capable compositor, real frontend and
private PipeWire node/frame/lifetime/lock gates. Source presence or mock policy
results do not establish that qualification. Installation, ordinary desktop and
sandbox delivery remain separate manager gates.

## Native privileged identity

[ADR-0340](../adr/0340-use-native-privileged-compositor-identities.md) coordinates
ScreenShot2 with the fork and screenshot/helper desktop entries. `CompositorNames`
provides the service, object, interface and error names. The source cutover must
ship with the native backend routing and matching compositor; installed
qualification remains a separate release gate.

The coordinated [protected input/capture composition](../adr/0341-unify-protected-capture-and-input-sessions.md)
adds only native input-family adaptors to the fixed broker. PortalCapture1
ordinary attachment is independent of resident Portal1 and grants no pixel
capability; inherited QCC1 and protected capture consent remain mandatory.
This candidate requires genuine combined/restore runtime before route delivery.
