# Native portal capture candidate contracts

Screenshot and ScreenCast are source candidates. The selector continues to route
both to KDE until actual native frontend, consent, pixels, PipeWire and privacy
gates pass. This first slice does not complete PF19: useful window/multiple/cursor
capabilities and shared revocable restore permissions remain a successor before
program closure. See [ADR-0324](../adr/0324-native-portal-capture-boundaries.md),
[portal foundation](../architecture/portal-foundation.md),
[portal service](../architecture/portal-service.md) and the public
[compositor capture boundary](../architecture/compositor-capture.md).

## Primary wire

The installed xdg-desktop-portal1.20.4 XML and primary frontend were inspected:
[Screenshot backend XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.Screenshot.xml),
[ScreenCast backend XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.ScreenCast.xml),
[Session backend XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.Session.xml),
[Screenshot frontend](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/src/screenshot.c),
[ScreenCast frontend](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/src/screen-cast.c).

Candidate adaptors at `/org/freedesktop/portal/desktop` provide:

| Interface/version | Method | Inputs | Output |
| --- | --- | --- | --- |
| Screenshot2 | Screenshot, PickColor | `o,s,s,a{sv}` request, app, parent, options | delayed `u,a{sv}` |
| ScreenCast2 | CreateSession, SelectSources | `o,o,s,a{sv}` request, session, app, options | `u,a{sv}` |
| ScreenCast2 | Start | `o,o,s,s,a{sv}` request, session, app, parent, options | delayed `u,a{sv}` |
| Session1 | Close | none, at the supplied session path | none; Closed signal |

Response0 means a validated result,1 means cancellation and2 means failure;
backend result maps are empty for cancellation/failure. Screenshot success has
canonical fully encoded local `uri=s`; PickColor has `color=(ddd)`, finite RGB in
[0,1]. Start has `streams=a(ua{sv})` containing exactly one actual node with
`position=(ii)` and `size=(ii)`. The real frontend implements OpenPipeWireRemote
and restricts the returned connection to the node set; the backend neither
exports that method nor invents a PipeWire FD.

ScreenCast capabilities are monitor1 and Hidden cursor1. SelectSources explicitly
rejects window/virtual source types, multiple=true, unsupported cursor modes,
restore tokens and persistence options. No unsupported version4/5 token or mapping
claims are made. Native source selection lists actual compositor monitors, with
no default selection; a user must select an offered screen and press Share.
Screenshot requires Allow; PickColor then requires a real pixel selection. A
mapped Stop sharing action remains while a stream runs. Standard frontend
PermissionStore hints never substitute for native consent/authority.

## Authority and lifetime

The resident exports actor-fenced standard Request and Session handles through
public RequestRegistry/PortalSessionBinding. A session binds actual frontend
unique owner, caller unique name encoded in standard handles, app ID and caller
PIDFD. Calls cannot mutate or close another app/caller's session. Bounds include
8 sessions,16 process jobs,16KiB helper frames,16 offered monitors and128MiB result
files. Pre-start sessions expire after60 seconds; screenshot files live at most
5 minutes in owned private temporary directories.

The resident supplies two ordinary FDs from the admitted selected compositor:
Qt GUI and generated capture protocol. Package-owned desktop entries authorize
only the needed restricted interfaces. The native helper independently verifies
ordinary peer credentials, PIDFD, current native bus owner and nonce-authenticated
read-only NativeLockStateMonitor receipts. Unknown/locked authority disables
consent and prevents operation/publication. Empty parents are explicitly
unparented; nonempty native foreign parents must import before Allow and their
loss withdraws the request/stream.

Request Close, Session Close, frontend/caller/supervisor/compositor/parent loss,
native lock uncertainty and teardown retire pending operations, discard late
output, kill children, end streams and delete retained screenshot files. Result
publication rechecks current admission before delivering a canonical private
file URI or actual node. Values already copied by a client and already consumed
stream buffers cannot be recalled; no physical-frame privacy claim follows from
queued signals alone.

## Evidence still required

Focused `qindaqt.portal-capture-policy` and `portal-capture-requests` rows cover wire shape, bounds, caller/app/session fencing, Close, late replies and owner loss. The source `qindaqt.portal-native-capture` runner provides two fresh private compositor groups, including irreversible native Locked retirement. It uses zero-activation-directory custom D-Bus, explicit nonexistent system bus, a private minimal PipeWire core and policy-only WirePlumber with hardware monitors absent. Qt input links unchanged helper sources; a changing fullscreen ordinary test client supplies known pixels. The consumer connects only to the actual frontend-returned remote FD and checks node visibility, decoded frames and cessation after Close/native lock. All these new runtime rows remain uncompiled/unexecuted at this source checkpoint. Actual
frontend methods and mapped ordinary Qt input must produce real captures and
private PipeWire frames; cancellation, Close, caller/dependency/parent loss and
native lock/uncertainty must withdraw results/streams. Staged helper permission
entries, metadata/selection/poison gates, existing accepted portal/Screenshot
regressions, strict docs and source shape must pass before routing changes. No
actual runtime or installed full-portal completion is asserted by this page.
