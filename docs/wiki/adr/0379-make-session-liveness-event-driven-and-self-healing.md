# ADR-0379: Make session liveness event-driven and self-healing

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Session supervision, native lock services and platform attachment
- **Supersedes:** The getter-time daemon recheck in [ADR-0305](0305-public-ordinary-compositor-attachment.md); the "starts a unit that has not yet run" rule in [ADR-0094](0094-refresh-resident-wayland-session-services.md)
- **Superseded by:** None
- **Related:** [ADR-0304](0304-native-lock-observation-and-service-policy.md), [ADR-0321](0321-supervisor-owned-native-sleep-admission.md), [Session resilience](../architecture/session-resilience.md)

## Context

On 2026-10-15 at 08:15 the owner's session lost its screen lock for the rest of
the day. The shared [compositor attachment](../architecture/compositor-attachment.md)
polled the session bus every 100 ms with about seven blocking calls bounded at
250 ms, and treated the first slow reply as permanent loss. Nothing ever
re-attached. Native lock state stayed Unknown, so `ScreenSaver.GetActive`
failed, `Sleep1.CanSuspend` was false and idle and manual lock were refused. The
logind sleep transport then revoked itself every 250 ms because its admission
read the dead attachment: each revoke dropped the PrepareForSleep, Lock and
Unlock subscriptions and the delay inhibitor, so lid suspends happened
unlocked, and it emitted `Sleep1.Changed` twice per tick. A 10-second sample of
the live session bus on 2026-10-16 counted 80 `Sleep1.Changed` signals; the
shell answered them with 40 `GetNameOwner` calls per second, the keyring made
54 daemon calls per second, and `qindaqt-session` 24.

The same pattern recurred elsewhere: the keyring polled its own name every
250 ms, Display1 exited with status 4 at every login because it was activated
before `qindaqt-session` published `WAYLAND_DISPLAY`, and the login refresh
restarted `xdg-desktop-portal` 3.5 s after it had started. Items 2, 12, 19 and
20 of the stability round's findings (`ops/team/specs/2026-10-16-stability/`)
record the evidence. KDE's KScreenLocker and GNOME
(gnome-session, gnome-shell, gnome-settings-daemon) solve the same supervision
problems with `NameOwnerChanged` watchers, a logind delay inhibitor and
PrepareForSleep handling; QindaQt follows those patterns with its own code and
no dependency on either desktop.

## Decision

1. **One bounded recovery primitive.** The LGPL platform target
   `QindaQt::RecoveryBackoff` provides an injectable one-shot timer port and a
   bounded exponential schedule (0.25 s doubling to 30 s by default). A
   component that attaches to a peer, holds a subscription or waits for a
   session resource retries through it. Tests inject manual timers.
2. **Attachment liveness is event-driven.** `CompositorAttachment` getters check
   the kernel PIDFD and probe socket and the consumer's admission synchronously,
   and use compositor ownership cached from `NameOwnerChanged`. They never call
   the bus daemon, and no timer polls it. PIDFD and probe notifiers publish
   kernel loss without any getter. Admission callbacks must be cheap and make no
   bus call. Every daemon query has an explicit per-call bound; no component
   changes a connection's shared interface timeout.
3. **Genuine loss re-attaches.** Peer exit, probe hang-up, loss of the admitted
   compositor owner or pathname replacement emits `revoked()`. The retained
   selection then re-runs the full proof asynchronously, including any
   independently expected peer, retrying slow or failed replies with backoff,
   and emits `attached()` when a valid owner appears. Explicit `revoke()`, a new
   `attach()`, disconnection of the selected session owner, refused admission or
   the death of an expected peer retire the selection. Admission withdrawal is
   published by event as well: whenever the selected session owner gives up a
   well-known name (a `NameOwnerChanged` match on its old owner), admission is
   re-checked and a refusal retires the selection with `revoked()`.
4. **Lock state recovers.** `NativeLockStateMonitor` publishes Unknown on every
   failed, timed-out or refused probe or query and retries with the bounded
   schedule while started. Only a fresh nonce receipt supplies state.
   `refresh()` re-probes at once. The supervisor's lock assembly refreshes the
   monitor and idle observation on every attachment change and, when its first
   assembly fails, retries it with backoff instead of leaving the login unlocked.
5. **Logind binding persists.** `LogindSleepTransport` caches the logind owner
   from its watcher. A transient error retries the binding or the delay
   inhibitor with backoff; a logind restart rebinds; a validation mismatch is
   refused until the owner changes. Admission is re-evaluated only when its
   owner reports a change; losing admission releases the delay inhibitor but
   keeps the subscriptions. `availabilityChanged()` and therefore
   `Sleep1.Changed` fire only when availability really changes; the coordinator
   publishes only `canSuspend()` flips.
6. **Clients use watchers.** The shell-side session-actions client caches every
   service owner from watchers plus one asynchronous lookup at start, coalesces
   bursts into one running refresh and one follow-up, and never queries an
   absent ScreenPower1. The keyring ends on name or bus loss through events.
7. **Login refresh restarts only stale units.** A refresh-listed unit restarts
   only when its running instance predates this login (started before the
   witnessed compositor) or runs a replaced executable. A unit started during
   this login, or not running, is left to D-Bus activation. The retired Plasma
   portal backend leaves the list.
8. **Startup races wait.** Display1 waits, bounded inside its unit start
   timeout, for the session's Wayland socket, reading the user manager's
   published environment when its own lacks `WAYLAND_DISPLAY`. It tolerates a
   new suspend before its delay inhibitor returns and retries a failed Inhibit.
   NightLight logs the real registration error and retries. Settings1 and
   NightLight run in their own systemd user units instead of dbus.service's
   cgroup.

Cached ownership is safe because every call and signal check names a unique
connection, and a bus daemon never reuses a unique name: a stale cache can only
fail a call, never redirect authority. Messages on one connection arrive in
order, so a reply from a replaced owner cannot overtake its NameOwnerChanged.

## Consequences

- No owned path has a sub-second timer that makes synchronous bus calls. The
  remaining periodic timers are listed on the
  [session resilience page](../architecture/session-resilience.md).
- An owner change is now published when the watcher delivers it rather than at
  the next getter. Kernel lifetime and consumer admission still fail getters
  immediately. Two consumer tests outside this decision's owners assert closure
  with no event-loop turn after a same-PID compositor name replacement
  (clipboard native startup, shell notification lock observer); they must
  assert delivery instead.
- Probe loss from a compositor that stays live and keeps its name now cancels
  in-flight work and then re-attaches a new lineage to the same compositor.
  The keyring prompt-display test that treated such loss as permanent must
  assert cancellation and re-attachment instead.
- A lost compositor or logind binding retries for the life of the process at
  most every 30 s. A permanently refused owner costs nothing until it changes.
- The shell notification and clipboard observers still retire on revocation by
  their own policy; they are outside this decision's owners and may follow
  `attached()` later.
- Deterministic tests cover each retry schedule with manual timers, plus a
  private-bus integration in which the real supervisor lock assembly keeps
  answering `GetActive` through a frozen bus daemon and recovers after genuine
  compositor loss without a new login.

## Revisit when

A session component needs a liveness signal the bus daemon or kernel cannot
deliver as an event, or logind gains a supported way to keep a delay inhibitor
across its own restart.
