# ADR-0358: Authenticate native clipboard privacy at use

- **Status:** Proposed
- **Date:** 2026-10-08
- **Owners:** Clipboard platform service
- **Supersedes:** Native-session lock-composition clause of [ADR-0058](0058-isolate-clipboard-capture-in-a-volatile-host.md), on acceptance
- **Superseded by:** None

## Context

Installed enabled clipboard history remained unavailable after an unlocked
native login. The old host composition required the compositor and both
ScreenSaver facades to share one unique bus owner. The native compositor and
session supervisor deliberately own different names and processes. Trusting the
supervisor's unlocked payload, weakening the compositor PID check, or retaining
an old unlocked Boolean would break clipboard confidentiality.

The existing public ordinary CompositorAttachment and native lock receipt
monitor already provide separate live identity and lock proof. Clipboard needs
a private composition over those public boundaries, independent of Shell.
Signal delivery alone is insufficient: a retained host pointer can be called
after owner loss but before queued watchers run. Adapter cancellation and bus
notifications can also synchronously reenter the host.

## Decision

The production clipboard application privately owns NativeClipboardLockObserver.
The ordinary Wayland adapter supplies only its connection's kernel SO_PEERCRED
PID. Environment values select the runtime directory and socket basename;
neither supplies PID or unlocked authority. Normal login starts resident services before Session1 is published. Before
selecting any owner, startup observes initial name publication with one timer,
at most eleven synchronous lookup attempts, ten fixed delays and a 30-second
monotonic observation window. Repeated start cannot reset that window or add
pending work. No name lookup activates an owner. Privacy/capture stay closed.
The first resolved pair pins one Session1 unique owner and native compositor
owner before independent socket/receipt admission. SessionAdmission
re-resolves Session1 to that exact owner on the constructing bus. Public
CompositorAttachment validates runtime/socket access, actual kernel peer,
PIDFD/socket lifetime, same bus daemon and current compositor owner/PID.

The public native monitor separately requires targeted nonce-correlated state
receipts and matching authenticated empty method replies from that exact
compositor. ScreenSaver/Lock1 payloads, UID alone, executable paths and
environment-supplied PIDs grant no authority. Initial absent owners remain closed while that bounded observation is pending.
Exhaustion or explicit stop retires it, so an owner published after timeout
cannot revive history. Invalid local/socket/PID proof and post-selection owner
loss are terminal. A late native interface can recover only through the existing bounded
native receipt retry. Start means observation installed, never unlocked.
Attachment revoke or explicit stop retires the composition and its receipts.
There is no replacement-owner rebind, legacy fallback, service activation,
lock command or Wayland graph mutation in the observer.

Production ResidentClipboardService and ClipboardHost receive a nonempty
borrowed PrivacyAdmission function in their constructors. Captured dependencies
outlive the service/host on the same Qt thread. It is synchronous, read-only and
non-reentrant; absence, exception, recursive evaluation or live denial closes
privacy. Original source-compatible constructors remain for explicit test or
legacy compositions. Native production always selects the strict overload;
this is not a production recovery fallback. Clipboard1 version 1 stays unchanged.
The rebuilt static SDK adds constructor symbols; no binary ABI promise is made.

A cohesive host-private state owns history and gates. Every snapshot disclosure,
submit entry, capture admission and Copy publication rechecks live privacy.
A const snapshot legally reconciles through that owned state; it does not return
a pre-revocation encoded copy. Denial records the closed gate and generation
purge before cancelling capture or notifying Changed. The capture argument may
alias cancelled adapter transfer storage, so a denied capture returns immediately
without reading it. No entry or payload reference into history survives an
external admission, adapter call or signal.

Calls recheck after adapter calls and Changed notifications. If a Copy was sent
before authority loss is observed, its result is Uncertain; no second publish is
attempted. Pure mutations whose completion is revoked during notification also
return Uncertain. Previously cached operation results remain receipts of their
original operation, not authority to publish again. Getters cannot revive cached
unlocked state after observed denial; a fresh authenticated observer event is
required. Explicit Settings1 user-overrides consent stays independent.

## Consequences

- Enabled history can be available on the native split-owner session without
  weakening legacy monitor policy or granting Shell clipboard payload access.
- Initially missing owners remain unavailable until first admission within the
  bounded startup window. Timeout, invalid socket/PID proof or later authority
  loss requires a fresh resident composition via normal activation/restart;
  no observer silently chooses a replacement incarnation.
- A synchronous authority check may emit a content-free Changed signal, purge
  history and cancel an adapter transfer. Callers must respect same-thread
  lifetime and reentrancy contracts.
- Bus daemon resolution is bounded by the existing public attachment/transport
  timeouts. There is no parser allocation, timing or race-exclusion guarantee.
  Adapter cached availability/PID alone is not synchronous socket liveness.
- No persistence, clipboard-content logging, host clipboard probe, additional
  protocol fallback or automatic service/hardware operation is introduced.

## Verification

Separate fixtures exercise real private-bus owner/PID proof and UNIX socket
peers, targeted native receipts, old split-owner rejection, native unlocked
capture, lock/presentation transitions, missing/spoofed/duplicate receipts,
wrong kernel PID, owner replacement before an event-loop turn, delayed first Session1 publication
(including beyond the shorter native-interface retry budget), timeout followed
by late owner publication, stop during initial observation and late native objects. Synthetic adapter fixtures cover synchronous host purge, empty/throwing/
recursive admission, cancellation of aliased capture storage, changed consent,
pre-publish denial and post-publish/notification uncertainty.

The new Host fixture can be compiled against immutable old production/header
with QINDAQT_CLIPBOARD_LEGACY_PRIVACY for actual old-source failures. It does not
emulate the old policy with new objects. The installed-only consumer links both
new constructors and verifies empty admission denies without contacting a
host bus or clipboard.

Source and authored tests alone do not qualify native behavior, installed
startup or real selection capture. Exact old/fixed native gates, independent
trust review, integrated rerun and user-consented installed acceptance remain
separate. See [Clipboard service](../architecture/clipboard-service.md) and
[Clipboard C0/C1 proof](../development/testing-harness.md#clipboard-c0c1-service-proof).
