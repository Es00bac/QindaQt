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
- **AC-vs-battery automatic profile switching: PowerDevil owns the key, and
  wiring it is lane O8-b.** An earlier version of this page said no
  AC/Battery-scoped key existed anywhere in PowerDevil. That was wrong: it was
  concluded from `strings` on `powerdevil_powerprofileaction.so`, which is a
  thin wrapper over `org.freedesktop.UPower.PowerProfiles` and genuinely has no
  such key — but the setting lives elsewhere.
  `kde-plasma/powerdevil-6.6.6-r1` carries it in
  `kcm_powerdevilprofilesconfig.so` and the `ProfileSettings`
  KConfigSkeleton (`PowerProfileChanged()`, `isPowerProfileImmutable`,
  a `PowerProfileModel`), and it is stored in `powerdevilrc` as
  `[AC][Performance] PowerProfile=`, `[Battery][Performance] …` and
  `[LowBattery][Performance] …`, taking the names
  power-profiles-daemon reports (`performance`, `balanced`, `power-saver`; all
  three exist on `qinda-top`). PowerDevil applies it on a power-source change,
  so this is a key to write the way
  [powerdevil-idle](../architecture/powerdevil-idle.md) writes the Display keys
  — reparse before write, bounded values, the same adoption path — not a policy
  QindaQt has to build. Live observation stands: `configuredProfile` is empty
  and `currentProfile` was `performance` while discharging at 82%, which is
  what an unwritten key looks like.

## Row 11 — Journal hygiene

- **Portal drop-in `BusName=` warning: fixed.** See
  [ADR-0192](../adr/0192-portal-dropin-busname-cannot-be-cleared.md).
- **All QindaQt D-Bus services under the user manager:** confirmed —
  `systemctl --user list-units 'qindaqt*'` shows all six (`audio`,
  `bluetooth`, `clipboard-host`, `display`, `network`, `power`) `loaded
  active running`, none `start-limit-hit` (the failure mode
  [ADR-0170](../adr/0170-survive-a-private-session-bus-for-dbus-units.md)
  recorded for `qindaqt-clipboard-host`/`qindaqt-display-service` on
  2026-09-16 is not currently reproducing).
- **Not a finding after all: those "QindaQt" coredumps are other lanes' test
  binaries.** An earlier version of this page reported `qindaqt_control`,
  `qindaqt_clipboa[rd]` and `qindaqt_status_*` coredumps in two early-session
  windows and hypothesised a race against `SessionBusBootstrap` (ADR-0170).
  The attribution was wrong and the hypothesis unsupported. `coredumpctl info`
  on the retained dumps resolves the executables to
  `…/work_space/smart_lights/build/dev/tests/…`: a deliberately-aborting
  negative fixture (`qindaqt_controls_font_fixture_negative_tests`, six of the
  eight), `qindaqt_status_notifier_applet_qml_tests`,
  `qindaqt_clipboard_applet_qml_tests` and `qmltestrunner`. The journal's
  15-character `comm` truncation is what made
  `qindaqt_controls_font_fixture…` read as `qindaqt_control`. In the first
  claimed window (2026-09-16 21:32–23:01) there are **zero** QindaQt coredumps
  at all; the dumps there are `xdg-desktop-por` (three, at portal restarts) and
  `ZCode`.

  No installed QindaQt process has been observed to crash on this machine, so
  row 11's "no QindaQt coredumps" half is in better shape than this page
  claimed. What remains for row 11 is the repeated-warnings half, measured
  against a fresh session. The PM's own probe of the current boot agrees: see
  the counted warning list on the board.
