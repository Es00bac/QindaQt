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
canonical fully encoded local `uri=s`, using QUrl::fromLocalFile serialization
and exact literal-path round-trip. Legal path sub-delimiters such as `$` can
remain literal; spaces, percent signs and backticks are encoded. This is URI
serialization, with no shell parsing or expansion. PickColor has `color=(ddd)`, finite RGB in
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

The capture-only broker exports actor-fenced standard Request and Session handles
through public RequestRegistry and the capture session policy. A session binds
the actual frontend unique owner, caller unique name encoded in standard handles,
app ID and caller PIDFD. Calls cannot mutate or close another caller's session.
Bounds include8 sessions,16 jobs,16KiB helper frames,16 offered monitors and128MiB
result files. Created/selected sessions expire after60 seconds; starting requests
have a bounded100-second registry budget for consent plus the protected operation.
Screenshot files live at most5 minutes in owned private temporary directories.

The compositor launches the fixed protected broker and helper. The broker owns
publication but never opens an ambient display or launches a helper. The helper
uses separate ordinary GUI/foreign-parent fd4 and tagged capture fd5; control fd3
joins actual kernel credentials, current native bus owner and PIDFD. Authenticated
read-only NativeLockStateMonitor receipts additionally gate consent, operation and
publication. Unknown/locked authority denies. A live monitor forwards denial to
its owner; admission destruction stops timers and disconnects that forwarding
before stopping the monitor, because the owner’s job storage may already be
unwinding. Teardown cannot emit a new owner-facing loss callback. Empty parents are explicitly
unparented; nonempty parents must import before Allow and their loss withdraws
requests/streams. The general supervisor-owned backend and Session1 remain intact.

Request Close, Session Close, frontend/caller/broker/compositor/parent loss,
native lock uncertainty and teardown retire pending operations, discard late
output, end streams and delete retained screenshot files. A surviving helper
also unlinks only its own created image when the broker pipe/control disappears.
Publication reconciles queued control revocations and rechecks current admission
before delivering a canonical private URI or actual node. Values already copied
by a client and consumed stream buffers cannot be recalled; no physical-frame
privacy claim follows from queued signals alone. Hard simultaneous destruction
of both processes does not imply universal cleanup of already materialized files.

## Preserved predecessor admission blocker

Fork690 resolves restricted-interface permission through `/proc/<pid>/exe`:
`utils/executable_path_proc.cpp` supplies the path used by DBus Screenshot2 and
Wayland restricted-global admission. The resident and capture helper both set
`PR_SET_DUMPABLE=0` before opening connections. Same-UID peers normally cannot
read that protected proc link. A task-owned non-dumpable child experiment
confirmed denied executable-link access (errno13), and private fork690 logs
reported that the protected ordinary Wayland peer could not be identified.
The e78bf9f4 diagnostic fixture combined protected resident and frontend caller,
so primary frontend registration failed to open the caller proc root. Its
successor separates the ordinary caller from a protected backend child hosting
unchanged public resident/composition, and supplies the installed primary client
context in private PipeWire configuration. At df68e649 both setups pass:
Screenshot maps the actual-input helper and returns frontend Response2/no URI;
the existing public capture failure signal reports the exact Screenshot2
`Error.NoAuthorized` mapping. This is an exercised restricted denial, not a
request to weaken protection or install another component. At e1b06401 the
fixture correctly asserts and reads the primary compatibility string
`session_handle`: ScreenCast CreateSession and SelectSources return Response0,
then actual Start returns Response2/empty streams. No mapped sharing dialog,
remote node or frame is reached. Fork logs separately report that the protected
resident ordinary Wayland peer cannot be identified; source denies its restricted
capture global when the executable identity is unavailable. Earlier failed
inputs and assertions remain preserved. Backend and helper retain protection
before Qt startup; the runner clears both inherited permission bypasses. A
dumpable backend or bypass cannot qualify production.

The predecessor runner accepts an optional `--case NAME` after its nine artifact paths for
bounded diagnostics using unchanged Qt assertions. Only the default three-group
command qualifies the full journey. A protected admission failure is expected to
produce a failed success assertion, not a capture-usability pass.

The missing public seam must authenticate both the resident Wayland capture
peer and child DBus screenshot caller without making either process dumpable.
It must bind the selected current compositor, authenticated UID/PID/PIDFD,
current frontend/requester/session/parent lineage and native privacy authority.
A grant must be interface-specific and revocable on owner, parent, request,
session or privacy loss; stale grants cannot authorize reconnection or late
publication. Self-reported executable paths, desktop app IDs and host/root
fallback are insufficient. Fork changes require separate ownership and review;
this candidate preserves process protection and existing permission checks.

## Evidence still required

Focused `qindaqt.portal-capture-policy` and `portal-capture-requests` rows cover wire shape, bounds, caller/app/session fencing, Close, late replies and owner loss. The source `qindaqt.portal-native-capture` runner provides three fresh private compositor groups, including irreversible native Locked retirement and actual compositor owner loss. It uses zero-activation-directory custom D-Bus, explicit nonexistent system bus, a private minimal PipeWire core and policy-only WirePlumber with hardware monitors absent. The predecessor direct fork receives no opt-in Xwayland, positional session/app, exit-with-session or input-method argument; its CLI has no launcher-specific empty-session switch. Every top-level broker/producer/controller uses a private process group and postexit audits require all descendants to disappear. Qt input links unchanged helper sources; a changing fullscreen ordinary test client supplies known pixels. The consumer connects only to the actual frontend-returned remote FD and checks node visibility, decoded frames and cessation after Close/native lock. The eleven named capture/resident/Screenshot targets build successfully at exact e78bf9f4. Earlier diagnostics stop in registration; corrected b3670dda observes Screenshot failure2 but no Start response. These failures do not qualify capture usability. At those predecessor commits, focused/adjacent runtime, complete native journey
and staged gates remained pending. Current protected consumer candidate evidence
is recorded below. Actual
frontend methods and mapped ordinary Qt input must produce real captures and
private PipeWire frames; cancellation, Close, caller/dependency/parent loss and
native lock/uncertainty must withdraw results/streams. Staged helper permission
entries, metadata/selection/poison gates, existing accepted portal/Screenshot
regressions, strict docs and source shape must pass before routing changes. No
successful capture-runtime qualification or installed full-portal completion is
asserted by this page.

## Protected authority source successor

The fork and consumer jointly froze wire version1 at fork
`91e1c20272b891add2455c648f49e097c9d8baaa`, then corrected only configured
package paths at `bd4eacd1eff51f81e4757ec3cd236015a0e73d7e`. Installed Qt-free
`launchpaths.h` beside the protocol header exports fixed executable/desktop
paths; consumer configure rejects destination mismatches. On qinda the actual
configured libexec directory is `/usr/lib64/libexec`. No runtime path discovery
or frame/FD/lifetime authority changed. Its public installed
`qindaqt-kwin/capture-authority/protocol.h` describes the fixed compositor-launched
`qindaqt-portal-capture-backend` and helper, with capture-only backend bus name
`org.freedesktop.impl.portal.desktop.qindaqt.capture`. It does not change the
general supervisor-owned ADR0318 portal backend or install activation for the
capture broker. Qualified header absence explicitly leaves this new backend
unavailable; no fork-private source include or legacy permission fallback exists.

The source consumer separates pure bounded packet codec, authenticated control
channel, broker-owned Request/Session/private-output lifetime and native helper
presentation. Control fd3 uses seqpacket32-byte headers and kernel credentials,
current compositor owner/PID/PIDFD, generation and role/state checks. JobStarted
passes only broker request-writer/result-reader ends; helper fd4 is ordinary Qt
Wayland/foreign-parent, fd5 tagged capture, fd6 request-read and fd7 result-write.
Captured result bytes cannot replace the authenticated channel or native privacy.

The broker owns bounded private files and does not launch helpers or open an
ambient display socket. The helper must send ParentReady after actual import and
ConsentGranted after actual Allow/Share, then wait CaptureReady. Screenshot has
one privileged invocation and hidden idle helper/control while its broker-owned
URI remains retained for at most300000ms after result. PickColor publishes before
intentional retirement. Streams follow live Session/actor/parent/native authority;
30s applies to Screenshot2 operation, not stream length.

The public KWin capture call now carries a pipe grace field defaulting to the
legacy10s; protected helpers select the frozen30s pending-call/total with zero
grace. A monotonic send-time deadline also rejects completion before queued timer
delivery, including during drain/decode. Legacy interactive120s remains unchanged.
These source changes are uncompiled/unexecuted until separately granted gates;
wire freeze and source presence do not qualify native capture usability. The helper source now follows fixed FD/control consent and retention.
Non-installable variants pass bounded per-job actual-input directives through
opaque pipes, removed before the strict production parser; no installed input
hook or permission bypass exists. Exact candidate7bc9e6a5 passes11 focused/adjacent
CTests (87 Qt passes, zero failures/skips), including packet/channel and live
native-admission teardown regressions on private buses; this is candidate evidence,
not integration or actual protected pixels/frames. Channel cases
include real private-bus owner replacement before queued reads, kernel sender
mismatch, consent ordering/stale generation, HUP, ancillary counts, FD ownership
and monotonic job replay rejection. No native authority usability is inferred.
The public transport fixture separately uses synthetic raw bytes on a zero-activation
private bus to test drain-before-metadata/EOF, early metadata with delayed pipe
completion beyond zero grace, event-loop delay, cancel/late reply suppression and
legacy interactive120s/default10s compatibility, plus native-owner replacement
while the old compatibility screenshot writer stays alive. This tests transport budgets,
not protected capture permission or actual pixels.

The new native runner takes eleven artifact paths plus an explicit matching native
plugin prefix. It selects a non-installable fork `testNativeCaptureAuthority` driver;
`QINDAQT_PRIVATE_CAPTURE_PLUGIN_PREFIX` must equal that driver's built bin directory
and contain its `qindaqt-kwin/plugins`. Missing/mismatched images or prefixes fail
configuration. The ordinary qualified fork executable supplies tool PATH only and
is never executed as the compositor. Native plugins come from the selected driver,
without inherited legacy prefixes or LD_LIBRARY_PATH substitutions. It invokes `serveNativeCapture` within its
single private bus/PipeWire environment, waits for the exact protected broker Ready
marker and closes stdin for normal shutdown. Driver configuration fixes test broker
and helper paths before compositor startup; production has no path override. The
ordinary caller starts the actual1.20.4 frontend and supplies bounded per-job test
input controls through opaque request pipes. Actual mapped Qt input audits fd4
ordinary registry denial of capture/lock/input-method/Xwayland/security-context
privileges and exact three-global fd5 announcements, with real public listener
forwarding. Ready/CaptureReady/revocation observations are test-only. The fork's
fd4 self-PID bypass repair and these observations require actual native gates.
The runner sets core limit0 before every child, clears permission bypasses and
preserves normal assertion failures. Fixture desktop entries grant no restricted
interface permission: successful pixels must use the protected control/consent lease.
The current `capture.audit` from the explicit helper audit path, completed
per-request fd4/fd5 history, producer logs and structured cleanup/core records
are retained under ignored `native-capture-evidence` beside the test caller before
temporary-directory deletion. Unexpected task-local cores remain opaque artifacts.
Actual groups require a task-only bwrap namespace with new proc and private dev,
no dri/input/snd nodes, isolated PID/network/IPC/UTS/user where supported, read-only
runtime dependencies and private0700 HOME/XDG/tmp/brokers/PipeWire. A harmless
namespace preflight must pass before native execution. Selected cases remain
diagnostics only.

Staging requires the separate broker/helper and exact selected public fixed paths
only when the qualified authority target exists. Otherwise capture artifacts are
absent and unavailable; no capture family may advertise or activate. Existing
positive/poison package and support-target guards remain unchanged. Metadata still
routes both families to KDE. Focused candidate unit/source gates pass; actual native/staged gates remain pending, and
prior exact denial evidence above stays preserved.
