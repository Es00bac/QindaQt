# Session resilience and bus discipline

[ADR-0379](../adr/0379-make-session-liveness-event-driven-and-self-healing.md)
makes every session-side attachment, subscription and startup wait in the
lock, sleep and session-supervision path event-driven and self-healing. This
page is the map of that contract: who detects loss, how each part recovers, and
which timers remain.

## Why

On 2026-10-15 the owner's session lost its screen lock for the rest of the day.
The shared [compositor attachment](compositor-attachment.md) polled the bus
daemon every 100 ms and treated one slow reply as permanent loss; nothing
re-attached, native lock state stayed Unknown and the logind sleep transport
revoked itself every 250 ms, dropping its PrepareForSleep subscription and
delay inhibitor. A 10-second sample of the live session bus on 2026-10-16
counted:

| Source | Calls or signals in 10 s |
| --- | --- |
| `Sleep1.Changed` (qindaqt-session) | 80 signals |
| qindaqt-shell `GetNameOwner` (session actions) | 400 |
| qindaqt-keyring (100 ms attachment poll, 250 ms name poll) | 544 daemon calls |
| qindaqt-session | 240 daemon calls |

With this contract an idle session sends none of these: no timer polls the
daemon, and `Sleep1.Changed` fires only when `canSuspend()` changes.

## Rules

1. **Events, not polls.** Loss is learned from `NameOwnerChanged`
   (QDBusServiceWatcher), the kernel (PIDFD readability, socket hang-up) or the
   bus's local `Disconnected` signal. Getters read cached state and the kernel;
   they never call the bus daemon.
2. **Slow is not lost.** A failed or timed-out reply is retried with the shared
   bounded schedule (`QindaQt::RecoveryBackoff`: 0.25 s doubling to 30 s),
   never treated as permanent loss. A validation mismatch (wrong UID, foreign
   session) is refused until the peer changes.
3. **Re-attach on return.** Genuine loss publishes Unavailable/Unknown at once
   and recovers automatically when a valid peer appears, without a new login.
4. **Explicit bounds.** Every daemon call names its own timeout. No component
   changes a connection's shared interface timeout, which other components in
   the same process rely on.
5. **Real changes only.** Change signals are emitted when the published value
   changes, and clients coalesce bursts.

## Components

| Component | Detects loss by | Recovers by |
| --- | --- | --- |
| [`CompositorAttachment`](compositor-attachment.md) (session, keyring, portal, clipboard, shell) | PIDFD and probe-socket notifiers; compositor-name watcher; session-owner watcher; admission re-check when the session owner gives up a name | Asynchronous full re-proof of the retained selection with backoff; `attached()` again |
| [`NativeLockStateMonitor`](native-session-lock.md#native-readonly-observation) | Failed, timed-out or refused probe/query | Unknown, then bounded re-probe/re-query; `refresh()` re-probes at once |
| Supervisor lock assembly (`NativeLockComposition`) | Attachment `revoked()`/`attached()` | Refreshes monitor and idle observation, re-evaluates sleep admission, resumes native power; a failed first assembly retries from 1 s doubling to 30 s |
| [`LogindSleepTransport`](native-session-lock.md#selected-logind-lifecycle-and-manual-sleep) | logind owner watcher; reply errors | Rebinds a new owner; retries binding and delay inhibitor with backoff; admission changes release or re-acquire the delay |
| `SleepCoordinator` / Sleep1 | Lock state, delay and manual-request changes | Emits `availabilityChanged`/`Sleep1.Changed` only on a `canSuspend()` flip |
| Shell `SessionActionsClient` | Service watchers | Cached owners; one running refresh plus one follow-up per burst; absent ScreenPower1 never queried |
| Keyring daemon authority | `org.freedesktop.secrets` watcher; local `Disconnected` | Exits so systemd/D-Bus activation restarts it |
| Keyring and portal session lifetimes | Keyring1/Portal1 watchers | Asynchronous owner/UID lookups and attach; at most 30 attempts per owner |
| [Login resident refresh](compositor-session.md) | Unit MainPID, `/proc` start time and executable link | Restarts only instances that predate this login or run a replaced executable |
| [Display1 startup](display-service.md) | Missing session socket; delay inhibitor errors | Waits for the published `WAYLAND_DISPLAY`; retries Inhibit; tolerates back-to-back suspends |
| [NightLight startup](night-light.md) | Name registration failure | Logs the real error and retries from 1 s |

Settings1 and NightLight run in their own systemd user units
(`qindaqt-settings-service.service`, `qindaqt-night-light-service.service`),
named by `SystemdService=` in their activation files, so an OOM kill in
`dbus.service`'s cgroup cannot take them, or the bus, down together.

## Remaining timers

The acceptance audit for ADR-0379 found no sub-second timer in these modules
that drives a synchronous bus call. The timers that remain:

| Timer | Period | Why it is safe |
| --- | --- | --- |
| `RecoveryBackoff` users (attachment, monitor, logind, Display1 inhibitor) | Single shot, 0.25 s to 30 s, armed only after a failure | Asynchronous calls; an idle session arms none |
| Keyring and portal session lifetime ticks | 100 ms while an owner is unadmitted | Asynchronous lookups; at most 30 attempts per owner; stopped on success; re-armed only by an owner change |
| Native lock assembly retry (`qindaqt-session`) | 1 s to 30 s after a failed first assembly | Blocking queries are bounded and never more often than once a second |
| NightLight registration retry | 1 s to 4 s, at most five tries | Startup only; inside the unit's 20 s start timeout |
| Display1 Wayland wait | 0.25 s to 2 s, at most ten tries | Asynchronous manager query; startup only |
| Session-actions refresh debounce | 0 ms single shot | Coalesces a burst; the refresh is asynchronous |
| Native power readiness | 25 ms inside a 6 s startup loop | Reads cached readiness only; native-exclusive startup only |
| `ScopedDisplayPower` renewal | 10 s | Asynchronous; native-exclusive only |
| NightLight frame refresh | 60 s | Sends schedule frames; makes no calls |
| Request deadlines (lock request 1.5 s, state receipt 1.5 s, manual sleep 17 s, suspend 15 s, session actions 3 s/20 s, display-power and NightLight receipts, display writer transactions) | Single shot | Timeouts only |
| Legacy ScreenSaver lock-state retry (`SessionLockStateMonitor`) | A fixed, bounded list of single shots | Only while the service object is not yet registered; asynchronous `GetActive` |
| Window-management reconfigure debounce | 200 ms single shot | Event-driven; asynchronous reconfigure |
| Screensaver launcher kill/relaunch | 2 s / 1 s | Child-process supervision, no bus calls |

## ScreenPower1

`org.qindaqt.ScreenPower1` exists only when `qindaqt-session` runs with
`--native-power exclusive` (`QINDAQT_NATIVE_POWER_EXCLUSIVE=ON`): the
[native power composition](power-service.md) registers it. The installed
`qindaqt-desktop` r29 build sets the option `OFF`, so the session never
registers it and the shell's lid screen-off action is unavailable. That is a
packaging decision, not a defect in `src/session/display_power`. In exclusive
mode the composition now resumes after the attachment re-attaches instead of
staying stopped, and the shell no longer queries ScreenPower1 while it is
absent.

## Tests

| Gate | Covers |
| --- | --- |
| `qindaqt.recovery-backoff` | Schedule growth, cap, reset and re-entrant rescheduling on manual timers |
| `qindaqt.compositor-attachment`, `qindaqt.compositor-attachment-recovery` | Getters stay live and non-blocking through a frozen daemon; genuine loss detaches and the returning owner re-attaches; failed and stalled re-proofs retry with growing delay; expected-peer and session-owner retirement; admission withdrawal published when the session owner gives up a name; kernel death published without polling |
| `qindaqt.native-lock-monitor`, `qindaqt.native-lock-qt-transport` | Every failure retries with backoff; refused admission re-probes; `refresh()` leaves Unknown at once |
| `qindaqt.session-native-lock-recovery` | The real supervisor assembly answers `ScreenSaver.GetActive` through a frozen bus and after genuine compositor loss, and manual lock works again, without a new login |
| `qindaqt.logind_sleep_transport`, `qindaqt.sleep_coordinator`, `qindaqt.sleep_modes` | logind restart rebinds; transient binding and Inhibit errors retry; admission events; `Sleep1.Changed` equals real changes and stays silent while idle |
| `qindaqt.session-actions-client`, `qindaqt.session-actions-bus-discipline` | A burst of 40 `Sleep1.Changed` produces at most two refreshes and no `GetNameOwner`, `NameHasOwner` or `CanScreenOff` calls |
| `keyring_secret_service` | No daemon calls from an idle keyring; bus loss still exits 1 |
| `qindaqt.session-resident-service-refresh` | Stale, fresh, stopped and missing units; only the stale one restarts |
| `qindaqt.display-runtime-wayland-display-wait`, `qindaqt.display-runtime-session-safety-recovery` | Bounded wait for the published display; back-to-back suspend and Inhibit retry |
| `qindaqt.night-light-schedule-service` | The real registration error is reported and a retry succeeds |

See also [compositor session](compositor-session.md),
[native session locking](native-session-lock.md) and
[module boundaries](module-boundaries.md).
