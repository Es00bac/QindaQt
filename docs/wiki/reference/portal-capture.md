# Native portal capture contracts

The bounded Screenshot/PickColor and monitor ScreenCast source slice is
independently accepted at `d6002623` and integrated on the recovery branch.
Its independent eleven focused tests pass 87 Qt checks; the manager's coherent
exact-fork existing native matrix passes seven behaviors / thirteen Qt checks
with actual pixels and decoded frames. The installed selector continues to
route both families to KDE while additional privacy and package gates remain.
On combined integration `92f6888ad`, the strict 29-target affected consumer/plugin
build and 22 private CTests pass 200 Qt checks, including the eleven focused
capture/Screenshot tests, seven startup checks, five real decoder graph checks
and the existing power/action regressions. No row fails or skips; these focused
gates do not add a native GPU matrix or qualify the new privacy fixtures.
The successor source adds explicit multiple-monitor selection and all three
standard cursor modes using the existing producer. Its focused/native acceptance
remains pending; window/virtual sources and shared revocable restore permissions
remain open before PF19 closure. See [ADR-0324](../adr/0324-native-portal-capture-boundaries.md),
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
[0,1]. Start has `streams=a(ua{sv})` containing one to sixteen distinct actual nodes with
`position=(ii)` and `size=(ii)`. The real frontend implements OpenPipeWireRemote
and restricts the returned connection to the node set; the backend neither
exports that method nor invents a PipeWire FD.

ScreenCast source capabilities are monitor1 and Hidden1/Embedded2/Metadata4
cursor modes. Multiple selections are explicit and bounded to sixteen monitors;
the helper freezes the selected IDs before permission is granted and publishes
the batch only after every producer node is ready. Failure of one stream closes
the whole batch. SelectSources explicitly rejects window/virtual source types,
invalid or combined cursor-mode values,
restore tokens and persistence options. No unsupported version4/5 token or mapping
claims are made. Native source selection lists actual compositor monitors, with
no default selection; a user must select offered screens and press Share.
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
full native capability qualification or installed full-portal completion is
asserted by this page. The bounded current pixel/frame qualification is recorded
above and below; predecessor failures remain historical evidence.

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

The capture-only backend publishes its existing object/name only after its own
native observer has consumed the authenticated current-owner state receipt and
empty method reply. Initialized Unlocked, Locking, or Locked state can complete
startup; only current Unlocked state admits content. The name is registered
before starting the unchanged QCC1 channel because the compositor authenticates
that name when consuming Ready. The five-second startup budget includes native
initialization and Hello consumption. Owner loss before publication prevents
startup; all existing request-time identity/privacy checks and immediate job
revocation remain authoritative after publication. This corrects an ordering gap;
it does not establish the cause of the historical intermittent response2.
Focused checkpoint63e33ecd (production source7b79a7e9) passes the real-child
startup gate7/7 Qt and existing authority-channel gate13/13. The unchanged eager
backend fails the new ordering row because its public name is already present
while both native messages remain withheld. This is a genuine negative control
for startup ordering, separate from full native pixel/frame qualification.

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
privileges and exact scope-bounded fd5 announcements (two output globals for Screenshot,
three globals including screencast only for ScreenCast), with real public listener
forwarding. Ready/CaptureReady/revocation observations are test-only. The fork's
fd4 self-PID bypass repair and these observations require actual native gates.
The runner sets core limit0 before every child, clears permission bypasses and
preserves normal assertion failures. Fixture desktop entries grant no restricted
interface permission: successful pixels must use the protected control/consent lease.
The current `runtime/qindaqt-capture.audit` from the explicit helper audit path, completed
per-request fd4/fd5 history, producer logs and structured cleanup/core records
are retained under ignored `native-capture-evidence` beside the test caller before
temporary-directory deletion. Unexpected task-local cores remain opaque artifacts.
Actual groups require a task-only bwrap namespace with new proc and private dev,
only renderD128 AMD1002:731f plus read-only sysfs, no DRM card/input/snd nodes,
isolated PID/network/IPC/UTS/user where supported, read-only
runtime dependencies and private0700 HOME/XDG/tmp/brokers/PipeWire. A harmless
namespace preflight must pass before native execution. Software rendering requires
`LIBGL_ALWAYS_SOFTWARE=1`, `MESA_LOADER_DRIVER_OVERRIDE=swrast`, and
`GALLIUM_DRIVER=llvmpipe`. The frontend also selects the existing real foundation
Access composition: upstream1.20.4 creates Screenshot only if Access is selected.
Screenshot/ScreenCast still select the separate protected broker. Before requests,
the caller introspects both actual frontend interfaces, failing explicitly if absent.
Selected cases remain
diagnostics only.

Staging requires the separate broker/helper and exact selected public fixed paths
only when the qualified authority target exists. Otherwise capture artifacts are
absent and unavailable; no capture family may advertise or activate. Existing
positive/poison package and support-target guards remain unchanged. Metadata still
routes both families to KDE. Current focused and existing native matrix gates
pass; dynamic package delivery and helper-cohort/property privacy coverage remain
separate. Prior exact denial evidence above stays preserved.

The non-installable runner accepts `--renderer llvmpipe|render-node`, with
`llvmpipe` as the unchanged default. Explicit `render-node` mode verifies the
sole AMD1002:731f renderD128 character device, read-only sysfs and absent card/input/
sound nodes, clears swrast/llvmpipe overrides, and sets LIBGL_ALWAYS_SOFTWARE=0.
The caller authenticates the private compositor PID and queries its public
`supportInformation`; actual OpenGL vendor/renderer lines are retained. Software
requires reported llvmpipe; render-node requires AMD/Radeon/radeonsi and rejects
llvmpipe. Both modes keep every real pixel/frame/privacy assertion.

The retained exact447 software probes on scope-correct consumer97c63 return an
actual1100x820 fully transparent black Screenshot PNG and a real ScreenCast node/
remote FD that fails the required decoded-frame count. Earlier assertions did
not print the count; they do not establish zero frames. VirtualEglLayer allocates through EglSwapchain and the
GBM allocator, whose software allocation requires DRM_IOCTL_MODE_CREATE_DUMB;
render-only isolation denies that operation and the actual layer fails repeatedly.
These failures establish rendering limits, and do not qualify screenshots or
streams. Hardware render-node allocation uses GBM dma-bufs; explicit native
qualification remains separate, without a primary-card/host-display fallback.

The changing Qt widget producer now reports readiness only after an actual
public `wl_surface.frame` completion on its own painted/committed Qt surface.
The fixture follows the existing foreign-exporter native-surface access pattern;
it fabricates no Wayland buffer, image, or callback. The retained producer-frame
audit distinguishes local exposure/paint from compositor frame completion.
A missing callback fails the bounded initialization before Screenshot/ScreenCast.

Exact consumer3a8e48a1 with fork447 and matching driver/plugin/library images
passes actual AMD/radeonsi renderer provenance and producer callback readiness.
The retained producer audit records a real Qt paint and `wl_surface.frame`
completion before the request. Screenshot still returns Response0/local1100x820
PNG whose902000 pixels are entirely RGBA(0,0,0,0); the unchanged fixture-color
assertion fails. A separate ScreenCast diagnostic obtains the actual node and
frontend remote FD, then fails the unchanged requirement for more than3 decoded
frames: the diagnostic reports2 decoded frames,3 visible nodes and an empty
consumer error string. Neither probe has cleanup errors, owned survivors or
task-local core files. The completed callback proves frame callback readiness,
not presentation or useful captured scene content. The remaining bounded blocker
is actual captured scene pixels and continuing changed frames; cancellation,
PickColor, Close and the remaining native privacy matrix remain unqualified.
All earlier failure logs/commands/hashes and this actual PNG remain preserved
in ignored qinda evidence directories. An incorrect diagnostic case spelling
was rejected before any compositor launch and is retained separately.


Subsequent driver-only scene observations on exact447 show the synthetic pixel
window initially eligible, visible and backed by a real1100x820 buffer, then
closed before the capture helper maps. An unchanged-producer stderr probe
identifies Qt host-portal registration failing with an empty desktop app ID;
`QT_FATAL_WARNINGS=1` therefore terminates the producer after its first completed
frame. The fixture now supplies `org.test.CapturePixels` as its QApplication
desktop identity and a matching real task-only desktop entry. The real frontend
starts before that identified Qt producer because registration is immediate; an
earlier repair attempt retained a fatal ServiceUnknown failure from the opposite
startup order. It retains fatal
warnings, producer stderr/lifecycle evidence, and liveness checks alongside the
unchanged actual pixel, continuing-frame and privacy assertions. This bounded
fixture repair on consumerbf82b66b with immutable fork447 passes the original
Screenshot/pixel/PickColor/cancel and PipeWire/changed-frame/Session.Close/cancel
cases independently (3/3 Qt each, no skips). The actual Screenshot is1100x820,
with902000 opaque fixture-green pixels; producer stderr is empty. The unchanged
full matrix first group passes Screenshot, Close/requester loss, foreign-parent
loss and frontend/broker loss, but the later stream decoder fails after prior
stream retirement (0frames,2nodes, target not found; group6pass/1fail). The
remaining fresh lock/compositor-loss groups were not reached. The decoder puts
a returned node ID into PipeWire target.object, whose documented value is an
object serial or node name. The decoder fixture now resolves only the offered
node ID through the actual returned remote registry, waits for its bounded real
sync barrier, and connects by its authentic object serial using PW_ID_ANY. It
records each ID-to-serial pair and fails closed on missing/retired targets; there
is no default-node fallback. The repaired executable ca29910d passes the first
five-case compositor group (7/7 Qt results), including real returned node25 to
serial27 after retirement and continuous changed colored frames. The fresh
second compositor fails its initial Screenshot before any lock request, with
response2 and no helper audit; its lock assertions and the third compositor-loss
group remain unqualified. Source review identifies a separate asynchronous
native-state admission handshake beyond the fixture readiness checks; whether
that timing causes this denial remains to be traced. No admission guard is
relaxed and no delay or retry is substituted for a public readiness condition.
One unchanged fresh-lock probe with an observational private-bus monitor passes
its initial Screenshot and all three Qt results, but the runner correctly fails
because the compositor exits with SIGSEGV during RequestLockWithReceipt. The
trace shows the broker received the authentic unlocked state receipt before
Screenshot, then no lock-state or admission receipt before compositor loss.
Thus this probe does not reproduce the earlier initial denial, and does not
qualify native-lock retirement: compositor survival remains an explicit gate.
The separately owned fork repair68c4d74f (executable42604a8f) defers source QObject
destruction until synchronous scene-view snapshots finish, while revocation and
frame gating remain immediate. Its original447 driver plus repaired plugin
passes one unchanged monitored native-lock row (Qt3/3, runner0), including real
locked/protected transitions, stream/file retirement and compositor survival.
The same unmonitored row had failed initial Screenshot admission before lock;
the monitored success does not resolve that recurring prerequisite. Full matrix
and independent review remain held, and no routing or milestone is advanced.
All earlier blank PNG, two-frame and startup-order failures remain retained.
No installed routing or full PF19 completion is claimed.

### Decoder target retirement and no fallback

The test-owned PipeWire decoder resolves the exact frontend-offered node ID to
its authentic current object.serial after correlated remote registry sync. It
sets `node.dont-fallback=true` as well as the explicit target and DONT_RECONNECT:
that stream flag alone permits an initial default-target choice if the selected
node disappears before linking. Registry removal still clears the retained
serial, and no fallback/default node becomes a substitute capture result.

The separate `qindaqt.portal-decoder-target` transport gate uses an actual private
PipeWire/WirePlumber graph with two task-only real RGBA `pw_stream` sources,
with red offered pixels and blue default pixels. A test-only
linker wrapper retires the exact offered source after registry resolution and
before real pw_stream_connect while the distinct configured default stays live.
The same fixture can link a byte-identical ca299 decoder as the negative control;
no decoder API or production capture authorization seam is added. This graph
fixture does not qualify native compositor pixels or broaden portal remote node
permissions. Original native pixel/frame/privacy assertions remain required.

The bounded actual graph gate passes all five Qt results: both explicit source
rows deliver more than three frames with their exact links/colors; retiring
offered node19/serial41 leaves default21/serial43 available but yields zero frames
and `defined target not found`. The byte-identical ca299 negative instead decodes
two blue frames from unrelated default20 after offered18/serial18 retires, failing
the unchanged zero-frame assertion. Both private daemons survive until ordered
cleanup, with no survivors or cores. The earlier incompatible direct SPA-source
format/daemon-exit failure remains retained; it is not fallback evidence.
