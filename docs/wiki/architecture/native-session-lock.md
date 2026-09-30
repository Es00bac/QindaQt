# Native session locking

[ADR-0294](../adr/0294-native-session-lock-authority.md) defines the target native
lock boundary. Candidate server evidence differs from integrated session and
package readiness. The released session retains its existing services until the
manager integrates native server, greeter and service candidates.

The QindaQt fork implements the standard Wayland `ext-session-lock-v1`
protocol. Protocol resources validate surface roles, configure acknowledgements
and output-sized buffers. A native lock window is an explicit role; other
windows belonging to the same client gain no privilege. The controller owns
main-thread lock state and a generation-tagged physical output presentation
barrier. Native role loss removes graphics, not authority.

The fixed installed `/usr/bin/qindaqt-lock` must be root-owned and not writable
by the session user, including its parent directory hierarchy. Its desktop
entry requests `ext_session_lock_manager_v1` using the public interface
permission field. The PF7 private launcher additionally restricts the global to
the compositor-created connection: peer UID or executable inode alone cannot
authorize a self-launched client. Debug permission bypass, executable names and
sandbox app IDs cannot grant access. Unlock revalidates the current connection.

The compositor trusts that executable to authenticate through PAM and account
validation before `unlock_and_destroy`. PF7 supplies the executable, PAM and
desktop permission entry. PF8 adapts idle/inhibitors, Lock1/ScreenSaver, session
supervision and consumers. A request to lock confers no unlock authority.

Every enabled desktop physical output presents a protected frame before
`locked`, including mirrored outputs. Black fallback is allowed by the standard.
No timeout manufactures success. Crash and output changes remain fenced; an
authorized replacement can recover. Ordinary windows and desktop chrome cannot
appear or receive input while locked. The compositor-owned input method is
permitted only while a current explicit lock surface has keyboard focus. Native
role graphics and input remain suppressed until the protected presentation
barrier has acknowledged the lock; an early green buffer cannot satisfy the
black-fallback barrier.

The focused proof runs on private virtual outputs and a disposable D-Bus bus,
with temporary HOME/XDG roots. Selected fixture socketpair peers receive access
only in a non-installable compile-time test build; production contains no
fixture authorization. The fork README records commands and matrix coverage.
The [testing harness](../development/testing-harness.md) records consumer gates.
Physical DRM, greeter/PAM, service policy and live rollout remain later
qualification boundaries.

The PF6 candidate also fences capture. Screenshot APIs reject locked requests;
screencast APIs reject new requests, close existing streams on acquisition and
stop recording. Direct window/output/region sources render black to CPU or GL
buffers while locked. The effects paint chain cannot replay cached ordinary
window content over the protected scene. This policy protects ordinary content
and does not export the greeter's authentication graphics.

The fork's production build no longer discovers or links KScreenLocker. Its
vendored public ScreenSaver XML retains the PF8 service boundary. This source
change alone does not remove released package dependencies: the manager owns
the coherent final source pin, greeter and service integration.

## Private launch and authentication candidate

[ADR-0299](../adr/0299-native-locker-private-launch-and-authentication.md)
supersedes executable-inode-only binding for the PF7 candidate. The compositor
creates the private locker connection and launches its fixed native executable
with a whitelist environment. An independently launched copy cannot bind the
lock global, including one with actual loader injection. Kernel ptrace protection
and non-dumpable/core-off authority processes protect the inherited transport.
The separate native PAM worker owns authentication and account approval, using
a bounded private inherited socket and the real UID account. Each frame carries
the current lock epoch/request; caller-selected identity/service, malformed
frames, cancellation and EOF fail closed. The executable worker fixture uses
only a temporary PAM configuration and synthetic module. Only the native
controller can consume current epoch/request approval. QML
provides prompt/response/cancel presentation and cannot manufacture success.

The backend's `org.qindaqt.KWin.NativeLock1` at
`/org/qindaqt/KWin/NativeLock` admits `RequestLock`, exports `Locked` and
`Protected`, and offers no unlock/authentication method. Protected means actual
current-generation physical presentation, including clientless black fallback.
The prior PF5/PF6 integrated server and this candidate are separate executable
boundaries until manager integration. Native greeter/UI/PAM/service delivery is
still qualified independently.

The Qt shell integration assigns the immutable standard lock role during the
native pre-show resource check. It suppresses Qt's usual initial shell commit
until configure is acknowledged, applies the latest output size, and exposes
only the configured surface. One role per real output is enforced; placeholder
screens have no lock role. The native protocol boundary is never exposed to QML
and waits for server sync after authenticated unlock. The private real-window
probe proves actual compositor pixels at normal and 150% scale, output add/remove
and crash-black recovery on the compositor-created connection.

The owned native worker client bounds framing, handles partial writes, accepts
only its current token/result plus normal worker exit, and kills its own child
at the whole-attempt deadline even inside a blocked PAM module. Cancellation
invalidates the controller token before child termination. After authentication
and account approval, the worker invokes the optional handle-bound keyring PAM
session notification, closes it and ends PAM while its conversation is alive,
then reports approval. Keyring failure permits its later prompt fallback; it
never supplies unlock authority. The final `qindaqt-lock` service stack must not
open a second login/logind session. Its distro/package delivery remains held.


## Native prompt and scene composition

The candidate `src/lock_greeter` composition root constructs a real QtQuick
view for each physical output. Each view obtains the native standard lock role
before loading/showing its compiled QML. A role or QML load failure exits the
locker and preserves the compositor's black/input/capture fence; there is no
ordinary window or layer-shell fallback. Output removal deletes its view before
Qt can move it to another output. The QindaTK prompt displays a clock, real-UID
account name, bounded PAM prompt/status and password field, plus the compositor's
actual current XKB layout name. All views clear credential fields together.

The native controller receives lock state solely from its borrowed protocol
client and authentication solely from its owned worker. Its QML-facing methods
are begin, respond and cancel; protocol/worker objects are never exposed to QML.
The only authenticated-unlock call site consumes the coordinator's current
epoch/request approval. Server sync completes before the locker quits.

The public pure `qindaqt_osk_core` model supplies touch-keyboard geometry,
shift/page state and bounded key intent inside the nondumpable locker process.
A locker-owned KeyEmitter edits only its own credential field. It opens no
additional privileged Wayland connection and synthesizes no credential
keystrokes to another application. Printable labels and the layout indicator
come from the compositor's bounded standard wl_keyboard XKB keymap/group.
The compositor-owned input method remains restricted to current lock surfaces.

A purpose-scoped public Settings1/screensaver preference provider selects only
the two fixed compiled Patrol/Reef wrappers. The imported public scenes disable
metrics; unknown/absent preferences and failed optional scene loads retain the
plain background. No preference supplies an executable or QML URL. QML imports
and Qt plugins are restricted to root-managed installed directories in
production. CircuitReef's Qt6.11 prerequisite renames its incompatible QString
`palette` embedding property to `reefPalette`; CLI `--palette` is unchanged.
Its suite-source candidate and final Portage pin require separate integration.

The native worker sets a kernel parent-death signal and verifies its inherited
socketpair's creation credentials against the launching parent. Greeter crash
therefore terminates even a PAM module blocked outside conversation callbacks,
while the compositor remains locked. These controls do not modify kernel policy.


Gathered window chrome remains attached to ordinary window scene ownership.
The private combined fixture proves both icon-chip and shaded-strip coverage by
the real lock view, black screenshot/screencast source policy, whole-output black
fallback after locker death, and restoration after current native authentication.
Geometry inventory may persist during lock and is not content visibility proof.

The PAM process client's outbound queue checks channel/queue availability before
encoding another plaintext copy and wipes its owned response frame on every
branch. This complements field clearing and bounded native response buffers; it
does not imply that Qt implicit-sharing allocator history is a secure-memory API.

## Native readonly observation

[ADR-0304](../adr/0304-native-lock-observation-and-service-policy.md) defines the
PF8 observation boundary. The public `QindaQt::SessionLockState` target exports
`QtNativeLockTransport` and `NativeLockStateMonitor`, requiring only Qt Core/DBus.
The transport resolves the fork's NativeLock1 unique owner and process ID from
the bus daemon, subscribes before querying, and requests state with a fresh
32-character lowercase hexadecimal nonce. Only the targeted
`stateReceipt(nonce, locked, protected)` signal from the exact current unique
owner, with a matching void method completion, supplies a snapshot. Method
success replies and legacy properties do not supply state authority. Actual
signal message sender, signature, nonce, generation and request serial fence
queued state receipts. The same rule validates targeted lock-admission receipts.

The monitor requires a constructor-injected synchronous readonly admission
callback. It must consult an independently accepted live compositor attachment,
matching both exact unique owner and daemon-resolved PID. A supplied PID, same
UID or same process behind another unique owner is insufficient. Callback
captures and the borrowed same-thread transport must outlive the monitor.
Admission is rechecked before queries, replies, public state signals and getters,
including immediate revocation before the queued owner watcher runs.

Only an admitted `Locked=false, Protected=false` receipt permits ordinary
content. `Locked=true, Protected=false` means Locking; both true means physically
protected Locked. An inconsistent combination, malformed receipt, denied owner,
owner replacement, bus loss or failed receipt yields Unknown and suppresses
content/protection. Startup object absence receives a bounded retry only while
the exact attachment remains admitted. Legacy properties remain compatible but
do not update this monitor. There is no legacy fallback, RequestLock, unlock,
authentication result or locker launch on this readonly port.

This first executable slice is additive. Existing legacy quorum consumers are
still a separate migration boundary; PF8 lock request, ScreenSaver compatibility,
sleep policy and Settings migration are not completed by the observer alone.

## Native preferences and one-time import

[ADR-0308](../adr/0308-native-lock-preferences-and-atomic-import.md) gives
Settings1 durable native lock preferences. Schema v2 stores automatic enable,
idle timeout (60–14400 seconds, default 300), lock on resume, and idle grace
(0–300 seconds, default 5); both booleans default true. The public Core-compatible
`QindaQt::LockPreferences` provider borrows a same-thread SettingsClient and
returns values only for its ready, exact-current-owner, nonempty-epoch snapshot.
It contains no timers, lock capability or storage. Invalid/missing/replaced state
provides no preferences; consumers must choose an explicit fallback policy.

After owning Settings1, the resident service reads at most one MiB of valid UTF-8
from injected `kscreenlockerrc`, parsing a private bounded snapshot. Only Daemon
Autolock, integer Timeout minutes, LockOnResume and integer LockGrace seconds
are admitted. Explicit native user overrides win independently; a completion
marker and imported values share one atomic repository commit. Malformed or
unsupported source and failed persistence remain retryable. The source is never
written, and password bypass keys are never mapped. Completed migration is
idempotent across startup even when the legacy source changes.

Public [ordinary attachment and idle observation](compositor-attachment.md)
([ADR-0305](../adr/0305-public-ordinary-compositor-attachment.md),
[ADR-0307](../adr/0307-public-ordinary-fd-idle-observation.md)) provide selected
session identity/lifetime for native observation admission and true single-seat
idle signals. Admission joins explicitly accepted session selection, exact bus
owner/PID and actual ordinary socket/PIDFD; this is not executable or independent
supervisor-process attestation. NativeLock property payloads supply no attachment
authority. Existing consumer migration and native runtime policy remain separate
from the public extraction gates.

## Native Lock1 and ScreenSaver facade library

`QindaQt::NativeLockService` composes a borrowed readonly native monitor and a
separate manual `NativeLockRequest` port. The Qt request port uses the public
[ordinary compositor attachment](compositor-attachment.md): actual daemon ID,
unique compositor owner/PID and live socket/PIDFD admission must still match at
submission and completion. Bus identity queries have explicit 250 ms timeouts; one pending native request
uses a fresh 32-character lowercase hexadecimal nonce and a 1500 ms deadline.
The transport subscribes to the exact compositor owner before sending
`RequestLockWithReceipt(nonce)`. It accepts only the matching targeted
`lockAdmissionReceipt(nonce, admitted)` from that owner after the void method
completion. QtDBus success-reply metadata is not an identity proof. Cancellation,
malformed, missing, late or duplicate receipts, authority loss and timeout
produce an uncertain outcome without replay. Admission means **request
accepted**, never physical protection.

`ResidentLockService` claims `org.qindaqt.Lock1`,
`org.freedesktop.ScreenSaver` and `org.kde.screensaver`; native object path is
`/org/qindaqt/Lock1`, compatibility paths are `/ScreenSaver` and
`/org/freedesktop/ScreenSaver`. Partial registration rolls back only objects and
names this instance claimed. Same-thread borrowed ports must outlive the
facade; the composition owner controls monitor/admission lifecycle. Actual
D-Bus sender UID is resolved through the daemon for method admission. There is
one pending manual call across every facade endpoint, and no queue.

| Native Lock1 operation | Contract |
| --- | --- |
| `GetState() -> a{sv}` | Version 1; opaque epoch, unsigned 64-bit revision, state `unknown/unlocked/locking/locked`, bool `available` and bool `protected`. Unknown means unavailable and unprotected. |
| `RequestLock() -> bool` | True only for fresh native admission; false for explicit rejection; uncertainty is an error. |
| `Changed(epoch, revision)` | Invalidation; clients fetch a fresh state. Epoch/revision never provide authority by themselves. |

ScreenSaver `Lock()` and `SetActive(true)` use the same manual admission path;
`SetActive(true)` retains its standard bool reply. `GetActive()` returns false
only for admitted Unlocked, true for Locking/Locked, and an error for Unknown.
`ActiveChanged(false)` is never synthesized from admission loss. Legacy
ScreenSaver has no Unknown signal, so its cached bool is insufficient for
content disclosure or suspend ordering; those consumers need the admitted
native readonly observer and actual Protected value.

`SetActive(false)`, `SimulateUserActivity`, activity-duration queries,
Throttle/UnThrottle and Inhibit/UnInhibit return Unsupported. In particular,
ScreenSaver's inhibition request covers all automatic-lock/display-off/idle-
suspend scopes atomically: accepting only one would misrepresent support. The
qualified Power1 scope consumer/registry must be composed before this facade
can accept that request. Manual locking is independent of idle inhibition.
There is no unlock/authentication-result or greeter-descriptor interface.

The PF8 source candidate now composes the public attachment, idle observer,
Settings1 lock provider, native request/state monitor, Lock1/ScreenSaver facade,
and Power1 read-only client in the session supervisor. Its runtime applies the
confirmed Settings1 idle timeout and cancellable grace, supports a manual lock
request independent of idle policy, and evaluates lock-on-resume only against
current authenticated native state. A resume observed while state is Unknown is
held until state resolves; Locked/Locking needs no extra request. The Power and
Screensaver Settings routes use lock.* Settings1 keys, show confirmed values,
and report saved only after matching snapshot readback.

The runtime now consumes PowerClient's current-owner validated idle-scope
snapshot and defers automatic locking while `AutomaticLock` is active. This
never blocks explicit/manual lock requests. Power1 still reports zero supported
scopes by default, so this client behavior does not enable leases or establish
that an inhibitor is active on a host. The production display-off and idle-
suspend consumers are still required before any of the all-or-nothing scope set
can be advertised.

The runtime has a protected-before-suspend callback seam, but no production
suspend dispatch currently consumes it; owned suspend ordering remains pending
PF2/PF3 Power1 wiring. It cannot prevent a privileged external logind caller
from suspending. ScreenSaver Inhibit remains Unsupported until the Power1
automatic-lock, display-off and idle-suspend scopes are all consumed together.
No real lock or sleep is exercised by these candidate tests.
