# Laptop readiness: `qinda-top` (Checkpoint L, O8)

Findings and verification for Checkpoint L rows 3, 4, 5, and 11 (TEAM.md §3),
gathered by read-only `ssh qinda-top` probes and `qinda-top`'s own installed
system KWin 6.6.6 / PowerDevil / UPower / systemd — never by installing a
package or touching the live session. Dates below are 2026-09-18 unless
stated otherwise.

## Row 3 — Touchpad

Fixed in `src/apps/settings/input/{kwin_pointer_device_port.cpp,pointer_device_selection.cpp}`;
see [Settings Input route](../apps/input-settings.md) for the property-name
root cause. The live touchpad (`event4`, `SYNA2BA6:00 06CB:CE2D Touchpad`)
already reports `tapToClick = true` and `tapToClickEnabledByDefault = true`
today — the "tap-to-click disabled" fact in `PLAN.md`'s diagnosis table no
longer holds (confirmed live; a value can change between the plan being
written and this lane starting). QindaQt does not need to seed a default:
the row-visibility bug, not a bad default, was hiding the Touchpad section.

## Row 4 — Lid close suspends; wake shows the lock screen

[ADR-0132](../adr/0132-finish-session-locking.md) already ships the adapter
and its acceptance evidence explicitly named live lid behavior "requiring a
machine with a physical lid (explicitly out of scope here)". This row closes
that gap on `qinda-top`, policy side only (the physical close/wake cycle is
still the Checkpoint L **(user)** proof):

- `org.kde.Solid.PowerManagement.Actions.HandleButtonEvents.lidAction` = `1`
  (`Sleep`), `triggersLidAction` = `true` — live, read over D-Bus.
- `~/.config/powerdevilrc` has no `[AC][SuspendAndShutdown]` /
  `[Battery][SuspendAndShutdown]` group at all, so this is PowerDevil's
  compiled default, not a QindaQt or user override.
- `~/.config/kscreenlockerrc` does not exist, so `LockOnResume` is
  KScreenLocker's compiled default (`true` per ADR-0132's cited kcfg).

No code change was needed for this row; both policies were already correct
by default. Still open: the physical lid-close → suspend → wake → lock-screen
cycle itself, which only a **(user)** pass on the real hardware can prove.

## Row 5 — Battery: percent/time, low/critical notifications, AC-vs-battery profile

- **Percent and time estimate**: already present in both the Settings Power
  route (`PowerSupplySection.qml`, `percentageText`/`timeText`) and the
  power applet; no gap found.
- **Low/critical notifications**: new, see
  [Battery notifications](../architecture/desktop-controls.md#battery-notifications).
  UPower's configured thresholds on `qinda-top`
  (`/etc/UPower/UPower.conf`): `PercentageLow=20.0`, `PercentageCritical=5.0`,
  `PercentageAction=2.0`, matching `PLAN.md`'s 20%/5%.
- **AC-vs-battery automatic profile switching: open, not implemented.**
  `PLAN.md` describes this as "PowerDevil per-group setting", but the
  installed `powerdevil_powerprofileaction.so` (`strings`-inspected) exposes
  only `configuredProfile`/`currentProfile`/`setProfile`/`holdProfile` — a
  thin wrapper over `org.freedesktop.UPower.PowerProfiles` with no
  AC/Battery-scoped key anywhere in the binary. Live: `configuredProfile`
  is empty and `currentProfile` is `performance` while the laptop is
  discharging on battery at 82%, confirming PowerDevil is not switching
  profiles by source today. Building this is a QindaQt policy (new Settings1
  preference for the AC/Battery profile choice, a session policy watching
  `PowerClient`'s `source.acPresent`/`onBattery` edge, a Settings row to
  configure it, and an applet quick switch) — a vertical slice on the order
  of the battery-notification one above, deliberately left for a follow-up
  pass rather than shipped half-built (a Settings1 schema addition with no
  route to configure it is worse than not adding it).

## Row 11 — Journal hygiene

- **Portal drop-in `BusName=` warning: fixed.** See
  [ADR-0191](../adr/0191-portal-dropin-busname-cannot-be-cleared.md).
- **All QindaQt D-Bus services under the user manager:** confirmed —
  `systemctl --user list-units 'qindaqt*'` shows all six (`audio`,
  `bluetooth`, `clipboard-host`, `display`, `network`, `power`) `loaded
  active running`, none `start-limit-hit` (the failure mode
  [ADR-0170](../adr/0170-survive-a-private-session-bus-for-dbus-units.md)
  recorded for `qindaqt-clipboard-host`/`qindaqt-display-service` on
  2026-09-16 is not currently reproducing).
- **Open finding, not fixed: early-session QindaQt coredumps.**
  `journalctl --user -b -p warning` on `qinda-top` shows `systemd-coredump`
  entries for `qindaqt_control`, `qindaqt_clipboa[rd]`, and
  `qindaqt_status_*` clustered in two early-boot/early-session windows
  (2026-09-16 21:32–23:01 and 2026-09-17 00:07–00:32), overlapping in time
  with repeated `pipewire ... spa.dbus: Failed to connect to session bus:
  ... no-session-bus: No such file or directory` lines — consistent with a
  race where these processes start before the session's private D-Bus bus
  (`SessionBusBootstrap`, ADR-0170) is fully up, though no causal link is
  proven. No coredump has recurred in the most recent several hours
  (`coredumpctl list --since '6 hours ago'` shows only unrelated `sdrangel`
  and `office_quick` test crashes), and `coredumpctl list
  xdg-desktop-portal-kde` / `qindaqt_control` return no retained dumps to
  inspect further (rotated out). Reproducing or fixing this needs either a
  fresh login cycle on the laptop (explicitly out of scope for an
  implementer: "never touch the live desktop or the laptop session") or the
  next natural session restart to be watched live. None of the three crashing
  binaries fall under O8's leased paths
  (`src/apps/settings/{input,power}`, `src/session/desktop_controls`,
  `src/shell/power_applet`, notification policy, the portal drop-in); this
  is flagged for the PM to route.
