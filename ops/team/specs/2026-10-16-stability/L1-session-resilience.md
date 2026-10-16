# L1 — Session resilience and bus discipline (container-wm) — Opus

Base: container-wm hub `main` at the commit named in the manager's assignment. Branch `stability/l1-session-20261016`.
Worktree: `~/work_SPaC3/container-wm.worktrees/stab-l1-session-20261016`. Voice seat: `claude-helper-one`.

## Why
The live session's lock and sleep wiring went permanently dead after one slow bus reply. Since 10-15 08:15
there has been no lock, `ScreenSaver.GetActive` fails with "Native lock state is unknown", `Sleep1.CanSuspend`
is false, lid suspends happen unlocked, notification content is hidden and clipboard history is withheld.
`qindaqt-session` emits `Sleep1.Changed` twice every 250 ms. The shell answers each emission with about 13
blocking `GetNameOwner` calls plus logind `Can*` checks, which polkit evaluates. Findings items 2 and 12, and
the session parts of items 19 and 20.

## Owned paths
- `src/platform/compositor_attachment/**` (shared by qindaqt-session and the keyring; one fix serves both)
- `src/session_supervisor/**`, `src/session/**` (native_sleep, native_lock_runtime, display_power, autostart)
- `src/services/session_lock_state/**`, `src/services/native_lock_service/**`, `src/services/session_actions/**`
- `src/services/keyring/app/**` (only the periodic liveness check), `src/services/night_light/**`
- `src/services/display_writer/**`, `src/services/display_runtime/**` (startup and sleep-authority handling only)
- `data/systemd/**` entries for the units you touch, plus new units for Settings1 and NightLight (see 8)
- the tests and wiki pages for these modules

## Outcomes
1. **CompositorAttachment is event-driven and self-healing.**
   - No periodic timer that makes synchronous bus calls. Detect loss with pidfd, socket HUP and a
     QDBusServiceWatcher on the compositor's well-known name.
   - A slow or failed call is retried with backoff and is never treated as permanent loss.
   - On genuine loss, re-attach automatically once a valid compositor owner appears again.
   - Consumers get "attached/detached" notifications and keep working after re-attach.
   - The keyring's own use of this library, and its separate 250 ms check in `keyring/app/main.cpp`, become
     event-driven too.
2. **Native lock recovers.** `native_lock_composition` and the lock-state monitor re-attach after an
   attachment loss and leave Unknown as soon as the compositor answers again. `ScreenSaver.GetActive` answers,
   and idle and manual lock work again without a re-login.
3. **The logind sleep transport stops churning.**
   - No unconditional `revoke()` from a timer.
   - The PrepareForSleep / Lock / Unlock subscriptions and the delay inhibitor are kept and re-established
     automatically after logind restarts or transient errors.
   - Lock-before-sleep works again.
   - `Sleep1.Changed` is emitted exactly once per real state change, and never otherwise.
4. **Session actions (shell side) stop polling.**
   - Cache service owners via QDBusServiceWatcher, never with a blocking `serviceOwner()` helper, and never
     change the shared interface timeout.
   - Re-query capabilities only on a real Changed signal, and coalesce bursts.
   - Don't poll `org.qindaqt.ScreenPower1` when it is absent. Find out why the session never registers
     ScreenPower1 (`src/session/display_power`) and fix it if it should exist.
5. **Login does not restart services started in the same login.**
   - `resident_service_refresh` restarts only units whose running instance predates this login or whose
     executable was replaced (`/proc/<pid>/exe` is "(deleted)").
   - Remove `plasma-xdg-desktop-portal-kde.service` from the list.
   - Restarting xdg-desktop-portal 3.5 s after it started is the likely trigger of the dead screen-capture
     broker.
6. **Display1 startup race.** At every login Display1 exits with status 4 because it connects to Wayland
   before qindaqt-session publishes the socket name. Wait or retry (bounded) for the session environment
   instead of exiting.
   - Also stop the self-exit "sleep-without-delay-authority" when a new suspend begins before the delay
     inhibitor is re-acquired: tolerate it and re-acquire.
7. **NightLight startup** logs the real error. It waits or retries for Settings1 and Display1 instead of
   failing with an empty message.
8. **Bus cgroup isolation.** Today Settings1 (`qindaqt-settings-service`) and NightLight are D-Bus-activated
   inside `dbus.service`'s cgroup, which has OOMPolicy=stop. An OOM kill there stops the whole session bus;
   that happened on 10-02.
   - Give them systemd user units and `SystemdService=` in their activation files. Use the same hardening
     style as the existing QindaQt units, without PrivateTmp; see the existing AGENT-NOTEs.
   - The manager ships the dbus.service OOMPolicy drop-in and the bus queue limits separately.

## Acceptance
- Deterministic unit tests with an injectable clock or fake bus. Required cases:
  - attachment survives N timeouts without detaching, then detaches on real loss and re-attaches when the
    owner returns;
  - lock-state leaves Unknown after re-attach;
  - `Sleep1.Changed` count equals real changes, with zero emissions over a simulated minute of idle;
  - the logind transport re-subscribes after a simulated logind restart;
  - resident refresh selects only stale units.
- A private-bus integration test showing that `GetActive` recovers after a simulated compositor-bus stall,
  without a new login.
- An audit with grep: no `QTimer` under 1 s in your owned paths drives synchronous bus calls. List the
  remaining periodic timers and explain why each is safe.
- Focused build and ctest for every touched module. Update the docs `native-session-lock.md`,
  `compositor-session.md` and the native sleep page, plus an ADR if a public contract changes. Run strict
  mkdocs and the link checker.

## Reference designs (study, don't depend)
- KDE KScreenLocker (ksld): logind PrepareForSleep, a delay inhibitor, and greeter supervision.
- gnome-shell's screen shield with gnome-session and gsd's logind handling: event-driven `NameOwnerChanged`
  and no polling.
