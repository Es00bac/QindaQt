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
by the session user, including its parent directory hierarchy. Peer UID equals
compositor UID and executable inode/device matches the installed file. Its
desktop entry requests `ext_session_lock_manager_v1` using the public interface
permission field. Debug permission bypass, executable names and sandbox app IDs
cannot grant access. Unlock revalidates the current peer.

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
The planned separate native PAM worker owns both authentication and account
approval; the current executable slice validates its native libpam adapter and
request/epoch coordinator.
only the native controller can consume current epoch/request approval. QML
provides prompt/response/cancel presentation and cannot manufacture success.

The backend's `org.qindaqt.KWin.NativeLock1` at
`/org/qindaqt/KWin/NativeLock` admits `RequestLock`, exports `Locked` and
`Protected`, and offers no unlock/authentication method. Protected means actual
current-generation physical presentation, including clientless black fallback.
The prior PF5/PF6 integrated server and this candidate are separate executable
boundaries until manager integration. Native greeter/UI/PAM/service delivery is
still qualified independently.
