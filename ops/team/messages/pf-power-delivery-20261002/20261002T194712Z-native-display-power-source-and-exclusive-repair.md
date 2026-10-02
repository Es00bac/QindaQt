# Native DisplayPower fixture and exclusive-startup source checkpoint

- Timestamp: 2026-10-02T19:47:12Z
- Worker: pf-power-delivery-20261002 / pf_capture_privacy_coverage.
- Desktop base:71b458d5f (parent-composed final2944 plus historical review receipts), isolated worker/pf-power-exclusive-startup-repair-20261002.
- Fork base:0828296b373d3e8a05e3f4f439726c9807031789, isolated worker/pf-native-display-power-qualification-20261002.
- Fork candidate:10c634f06d6a179b3a27b1265683a9f0df419d44, pushed qinda hub;3 owned paths.
- Status: source checkpoint only. No compiler, native, host session or physical DPMS/sleep action; shared warm core untouched.

## Material source finding and repair

Final2944 suppresses its owned PowerDevil child before native startup and merely
logs failed native lock/power composition. Root confirmed this violates exclusive
prerequisite refusal and granted a minimal repair. Nonexclusive behavior is kept.

NativePowerComposition now distinguishes asynchronous start from readiness: the
current actual DisplayPower receipt/admission, confirmed settings/source, native
lock preferences and observation when automatic locking is configured, selected
logind delay/known native privacy authority, accepted full consumer registration
and current authenticated Power1 inhibition truth must all exist. It waits at
most6seconds using the Qt loop, corresponding to three existing2second wire
phases. There is no timeout restart, permission widening or fallback. Existing
exact sleep request tokens preserve declarations while an accepted owned sleep
temporarily makes canSuspend false. Failed exclusive composition unwinds owned
resources; main stops owned children and returns2. Nonexclusive degradation
still continues as before.

Actual CLI regression source launches the real qindaqt-session on a private bus
with task-only HOME/XDG/emptyPATH/dead system bus and explicit owned helpers.
It asserts missing compositor prerequisites yield exclusive exit2 and no power
child; off mode still starts an actual owned power probe and public Logout reaps
it. It does not claim positive complete composition or the6s branch has run.

## Actual native fixture source

Real KWinIntegrationTestFramework/Workspace/DisplayPower1; separate current
Session1-name owner and ordinary unique bus sender, no authorizer override.
11 authored behavior rows plus lifecycle (expected13Qt checks, not a result):
current inventory/epoch and unauthorized/stale mutation; stock virtual unsupported,
mixed capabilities, unexplained mixed/allOff and empty placeholder inventory;
legacy writer and topology withdrawal; own blank/release and same-mode external
Off; preexisting external Off; actual injected keyboard wake; expiry/owner loss.
Assertions check actual output modes and Workspace state, targeted lease receipts
and fresh request IDs. Production source remains byte-unchanged.

The private derived VirtualOutput uses protected BackendOutput state methods via
public Application::setOutputBackend before startup; only its supported rows
advertise DPMS. Stock inherited capabilities remain an explicit negative.
VirtualOutput symbols are not exported from core (read-only nm confirms), so the
target compiles unchanged upstream virtual_output.cpp into this noninstalled
fixture. Its real applyChanges/Workspace animation still route all state. This
is deterministic software nested routing, not physical renderer/DRM proof.

## Other exact source review

Root scanner4d7c4ce06ad19c444b916e6486bb3408ef7dd2e2 source ACCEPT: shortcut
fixture consumes the ext-session-lock generated files from the always-defined
native lock fixture under the same authorization guard/binary directory; only
keyboard-inhibit uses its separate scanner. No configure/runtime claim.

## Next executable gate

Different-worker source review of both exact candidates, then root-coordinated
warm-cache strict testNativeDisplayPower and affected desktop session/native
startup target build. Run the private nested fixture and actual CLI regression
once artifacts are coherent. Additional full supported native-exclusive startup
and shared idle/lid/source/inhibition require real composed private services and
are not implied by source, old7Qt input receipts or component tests. No lease
retained at this source checkpoint; available for reviewed repairs/exact gates.
