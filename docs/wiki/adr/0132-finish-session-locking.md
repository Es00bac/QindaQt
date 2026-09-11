# ADR-0132: Finish session locking on KWin's locker and PowerDevil's actions

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Session PowerDevil adapters, Settings Power route, Shell Power applet
- **Supersedes:** None
- **Superseded by:** None
- **Builds on:** [ADR-0011](0011-gate-notifications-on-authenticated-lock-state.md),
  [ADR-0091](0091-configure-kscreenlocker-preferences-through-settings.md),
  [ADR-0105](0105-delegate-idle-display-off-to-powerdevil.md),
  [ADR-0070](0070-confine-session-actions-behind-authenticated-boundaries.md)

## Context

Locking already has one owner end to end: KWin 6.6.6 embeds KScreenLocker
(`kwin_wayland` links `libKScreenLocker.so.6`, greeter
`/usr/libexec/kscreenlocker_greet`, PAM service `/etc/pam.d/kde`) and owns
`org.freedesktop.ScreenSaver` and `org.kde.screensaver`. QindaQt keeps that
locker — the ADR-0011 notification privacy gate authenticates it through
`src/services/session_lock_state`, and every lock button goes through
`src/services/session_actions`. What is missing is the rest of a complete
locking surface:

- KScreenLocker's resume-lock and grace-period preferences
  (`kscreenlockerrc` `[Daemon] LockOnResume` and `LockGrace`) are not
  configurable anywhere in QindaQt, although the ADR-0091 adapter already
  writes the neighbouring `Autolock`/`Timeout` pair.
- Lid-close and power-button behaviour live in PowerDevil's
  `[<profile>][SuspendAndShutdown]` profile keys and have no QindaQt
  configuration path at all.
- `Meta+L` has two registrants: KWin's `ksmserver` shortcut component owns
  the active "Lock Session" binding, and the shell registers a second,
  conflicting `qindaqt_lock_session` action that loses the conflict and does
  nothing — an advertised-but-dead shortcut.

## Decision

KWin's embedded KScreenLocker stays the only locker, and PowerDevil stays the
only interpreter of lid and power-button policy. QindaQt never locks, suspends,
or shuts down by itself; it only persists preferences and asks the owning
daemon to reload them.

1. **Resume-lock and grace join the ADR-0091 adapter.** The Power route's
   screen-lock adapter reads and writes two more `[Daemon]` keys:
   `LockOnResume` (bool, "Lock after waking from sleep") and `LockGrace`
   (seconds; offered as immediately = 0, 5, 30, 60, and 300). Its strictness
   is unchanged: only `Autolock`, `Timeout`, `LockOnResume`, and `LockGrace`
   are written; every other key and group is preserved; a mutation re-reads
   the stored state first; unchanged values are never written; `configure` is
   requested once per successful save; an unreadable config rejects the
   change. Upstream `settings/kscreenlockersettings.kcfg` at tag `v6.6.6`
   defines `LockGrace` as Int seconds (default 5) and `LockOnResume` as Bool
   (default true); the upstream KCM offers the same grace ladder. The
   upstream "-1 = never require password" option is deliberately not offered:
   it flips `RequirePassword`, which stays outside this adapter's four-key
   write set.
2. **A new `src/session/powerdevil_lid` adapter persists PowerDevil policy.**
   Mirroring `powerdevil_idle` (ADR-0105), the adapter opens `powerdevilrc`
   through KConfig, reparses, writes exactly
   `LidAction`, `InhibitLidActionWhenExternalMonitorPresent`, and
   `PowerButtonAction` in each of the `AC`, `Battery`, and `LowBattery`
   `[<profile>][SuspendAndShutdown]` groups, syncs, and then triggers the same
   daemon reload (`org.kde.Solid.PowerManagement.refreshStatus`). Offered
   choices are the supported `PowerDevil::PowerButtonAction` values cited
   below: do nothing (0), sleep (1), hibernate (2), shut down (8), lock
   screen (32), and turn off screen (64). `PromptLogoutDialog` (16) and
   `ToggleScreenOnOff` (128) exist upstream but are not offered: the logout
   dialog and screen-toggle flows are outside the requested surface and can
   be added by extending the same mapping. Availability, owner fencing, and
   error truth follow `powerdevil_idle` exactly.
3. **Lid presence is additive telemetry.** The Power page hides the two lid
   rows on machines without a lid (this development machine has none). The
   public Power1 wire gains the smallest additive field that expresses that
   fact (logind `LidIsPresent`), carried through `power_protocol`,
   `power_service`, and `power_client` with codec validation and a
   fail-closed default for malformed or older sources.
4. **Meta+L has exactly one owner: KWin's ksmserver component.** The shell
   stops registering `qindaqt_lock_session`; the Power applet and menu, the
   Power page, and any future QindaQt surface lock through
   `session_actions`, never through a global shortcut. KWin's "Lock Session"
   component is the shortcut authority and already dispatches into the same
   embedded locker.

### Cited values (PowerDevil and KScreenLocker 6.6.6)

Gentoo installs no PowerDevil headers or kcfg sources, so the numbers are
cited from the upstream `v6.6.6` tag, which matches the installed
`libpowerdevilcore.so.6.6.6` and `/usr/share/doc/powerdevil-6.6.6-r1`; the
moc enum tables inside the installed library and the
`powerdevil_handlebuttoneventsaction.so` D-Bus properties
(`lidAction`, `triggersLidAction`) corroborate both shape and version.

- `daemon/powerdevilenums.h` @ v6.6.6 — `PowerButtonAction`:
  `NoAction = 0`, `Sleep = 1`, `Hibernate = 2`, `Shutdown = 8`,
  `PromptLogoutDialog = 16`, `LockScreen = 32`, `TurnOffScreen = 64`,
  `ToggleScreenOnOff = 128`; `SleepMode`: `SuspendToRam = 1`.
- `PowerDevilProfileSettings.kcfg` @ v6.6.6 — per profile:
  `LidAction` UInt, `PowerButtonAction` UInt,
  `InhibitLidActionWhenExternalMonitorPresent` Bool (default `true`),
  `SleepMode` UInt (default `SuspendToRam`).
- `daemon/actions/bundled/handlebuttonevents.cpp` @ v6.6.6 (lines 203-206,
  172-187) — both config keys are cast to `PowerButtonAction`;
  `TurnOffScreen` dispatches `DPMSControl`/`TurnOff`, every other value
  dispatches `SuspendSession` with `Type` = the numeric value.
- `daemon/actions/bundled/suspendsession.cpp` @ v6.6.6 (lines 111-141) —
  `Sleep` suspends, `Hibernate` hibernates, `Shutdown` requests a session
  shutdown without confirmation, `LockScreen` locks through the session
  manager.
- `kscreenlocker/settings/kscreenlockersettings.kcfg` @ v6.6.6 —
  `kscreenlockerrc` `[Daemon]`: `Autolock` Bool (true), `Timeout` Double
  minutes (5), `LockGrace` Int seconds (5), `RequirePassword` Bool (true),
  `LockOnResume` Bool (true).

## Consequences

- The Power page presents one coherent lock story: idle lock (existing),
  resume lock and grace (ADR-0091 adapter), and lid/power-button policy
  (new adapter), each row icon-led and keyboard reachable.
- The screen-lock adapter stays the only Power-route component that names
  the locker; `powerdevil_lid` stays the only component that writes
  `powerdevilrc`; boundary checks keep both claims mechanical.
- `power_protocol`/`power_service`/`power_client` grow one additive,
  versioned-tolerant field; consumers of the existing wire are unaffected.
- Shell shortcut ownership shrinks: removing the dead second registration
  makes KWin's ksmserver "Lock Session" the sole `Meta+L` owner, and a
  regression check fails if the shell ever registers it again. The applet,
  menu, and page lock buttons keep working through `session_actions`.
- ADR-0070's "Meta+L uses the existing audited shortcut registrar" sentence
  describes the 2026-09-03 state and is history; this ADR supersedes that
  sentence. ADRs are never rewritten.

## What is proven, and what needs a lid

Proven on this machine (no lid, empty `/sys/class/power_supply`): the
extended screen-lock adapter round-trips `LockOnResume`/`LockGrace` with
unrelated keys preserved; the lid/power-button adapter round-trips the three
`SuspendAndShutdown` keys against temp configs and a fake PowerDevil on a
private bus; lid rows hide without lid presence; `Meta+L` locks through the
private-bus proof with the shell registration removed.

Requiring a machine with a physical lid (explicitly out of scope here):
live suspend-on-lid-close and resume-lock after a real sleep cycle, external
monitor inhibition on real hardware, and PowerDevil's actual execution of
each mapped action. The adapters write documented keys and are unit-proven;
behavioural lid outcomes are hardware qualification.

## Revisit when

KScreenLocker or PowerDevil changes the documented keys, the enum values, or
the `configure`/`refreshStatus` reload contracts.
