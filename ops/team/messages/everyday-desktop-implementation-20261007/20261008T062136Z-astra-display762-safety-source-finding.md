# Display native safety topology: bounded source finding
- Time: 2026-10-08T06:21:36Z
- Reviewer: ed-foreign-astra-20261007.
- Exact source: 762084b781d8611d1b0d7ade1dcb671e3d17e278.
- Checkout: review/everyday-display-safety-astra-20261008, explicit hub fetch.
- Scope: source-only finding and executable reproduction proposal, not repair.
- No product/test edits, compiler, private runtime, Preview, lock, sleep or host action.

## Finding and provenance
QtSessionSafetyPort::start constructs legacy QtSessionLockTransport and
SessionLockStateMonitor with expected compositor PID. ResidentDisplayRuntime
gets that PID from WriterTransactionPort; the public output adapter contract
derives it from SO_PEERCRED on its actual connected Wayland socket and exposes
it only while live. That independent PID is good evidence and must be retained.

Legacy SessionLockStateMonitor::commonOwner requires all three unique owners
(org.qindaqt.Compositor, org.freedesktop.ScreenSaver, org.kde.screensaver)
to match before the PID query/unlocked admission. Exact-owner mismatch yields
Unknown. Native topology reported by root has separate Compositor and Session
ScreenSaver owners. Therefore the current code cannot reach Safe via that
topology even when native lock receipts say unlocked and logind delay is held.
This is a source inference against root's owner observation, not a reproduced
installed Display failure. No inference from daemon presence or missing guessed
unit is involved.

The existing tst_session_safety_private_bus.cpp helper registerScreenSaverNames
explicitly registers all three names on one connection, then expects Safe.
Its wrong-PID negative is valuable but cannot cover this native positive.
Logind's ready notification means a delay FD is held, not Safe; runtime already
documents that distinction. Transaction Machine rejects preview when safety
is not Safe, so replacing a UI flag alone would be the wrong boundary.

## Smallest executable old-control reproduction
Add an owning private-bus test alongside the current safety fixture:
1. Preserve fake logind Inhibit and observer, with invalid external bus
   environment and private-only broker as existing harness.
2. Give Compositor and the two ScreenSaver facades DISTINCT real unique owners.
   The facades report inactive. On the Compositor connection publish the actual
   NativeLock interface and nonce-correlated ordinary targeted receipts using
   the public session_lock_state test protocol, not a Lock1/PID payload.
3. Supply independently fixture-observed expected Compositor PID; for the real
   public attachment qualification provide a private ordinary Unix socket with
   actual SO_PEERCRED/pidfd, same actual bus ID and pinned Session1 identity.
   A test-injected exact-owner/PID callback is a bounded port unit test only,
   not proof of the production attachment producer.
4. Wait for actual ready/inhibit evidence and a native unlocked receipt, then
   require currentSafety()==Safe. Against immutable762 legacy source the
   positive must fail as Unknown; no new API-only compile failure counts.
   Preserve the identical regression and original production-source digest.
   Current same-owner/wrong-PID tests remain as compatibility controls.
5. After the native composition repair, the identical positive must pass, plus
   unknown/locked/locking, wrong nonce/sender/PID, expired/revoked attachment,
   bus loss, Session1/compositor replacement, queued stale unlocked receipt,
   stop/late reply and logind owner/inhibitor/sleep-delay negatives. No physical
   display change is needed to qualify this port-level behavior.

Owning target/row already exists:
qindaqt_session_safety_private_bus_tests /
qindaqt.display-runtime-session-safety-private-bus.
Add one focused native topology fixture/row only if cleaner than extending it.
Resident transaction tests must retain Safe gating and suspend rollback/delay
release behavior; later installed Preview remains a manager-only gate.

## Public composition proposal, not implementation approval
Keep policy and logind-delay ownership in display_runtime. Add a native safety
composition over public QtNativeLockTransport + NativeLockStateMonitor; preserve
the legacy factory explicitly if compatibility consumers need it. Never weaken
the old quorum or equate an inactive ScreenSaver facade with native authority.

NativeLockAdmission must synchronously compare monitor's bus-daemon-resolved
unique owner/PID against an independently admitted LIVE ordinary compositor
attachment, same actual session bus, and the writer's current independently
observed peer PID/incarnation. Use public CompositorAttachment; do not import
Clipboard-private observer code. Pin first qualified Session1 unique owner on
that bus, continuously re-resolve that exact fixed service, and retire admission
on loss/replacement. Socket path/environment is only a locator, never authority.
CompositorAttachment verifies actual socket peer, daemon owner/PID and pidfd;
its identity getters recheck admission. Do not substitute same UID/PID alone
for this combined live proof.

The current writer public factory exposes only a live PID, not a captured
socket/attachment descriptor identity. If full exact-display association cannot
be supplied by composition from its actual connection, request a minimal public
writer identity/owned-FD seam rather than reach into private adapter members or
silently reconnect an ambient pathname. The safer cohesive future composition
can have the writer consume CompositorAttachment::openConnection()'s owned FD,
with explicit ownership/thread/error contract; that is an interface decision
requiring separate narrow path assignment and review, not a change made here.

Preserve initial Unknown and bounded initial owner publication handling; no
transparent rebind after a qualified generation is revoked. Native getters
recheck admission, so the safety port must not turn them into a permanently
cached Safe value. Before any new mutating transaction admission, recheck the
current live proof and preserve transaction rollback on loss. Existing
sessionSafetyReady, exact logind owner/delay FD, sleep prepare/rollback/delay
release, generation suppression and fatal loss semantics must remain separate
from native lock observation. Signal-only replacement needs explicit queued
revocation coverage rather than assumed safety.

No broad SDK dependency, global service locator or shell/Clipboard private
import is proposed. Root should assign the exact public factory/admission
packet only after this bounded reproduction is frozen. Media's harder module
provenance and ASan/Portage recipe review retains priority; reviewer available.
