# Native power policy preferences

PF1 establishes the Settings1 authority used by the later Power1 and idle
policy work. It does not execute power actions. Power1 remains the sole
runtime power-state authority; the shell and logind keep their existing
session-action boundary in [Power and brightness](power-service.md).

## Settings contract

Schema v2 defines bounded `power.lid.*`, `power.idle.*`,
`power.critical.*`, `power.sleep.*`, and `power.profile.*` values. Lid and idle settings are
per source (`ac`, `battery`, `lowBattery`). Lid actions are `none`, `suspend`,
`hibernate`, `lock`, `screen-off`, or `power-off`; docked actions use the same
set and default to `none`. Idle durations are seconds from 0 through 14400;
sub-minute preferences are preserved. Each source has a sleep mode of
`suspend`, `hybrid-sleep`, or `suspend-then-hibernate`.
Critical action is none, suspend, hibernate, or power-off, with a countdown
from 5 through 300 seconds. Power profile values are none (leave the
provider's profile alone), or the admitted power-saver, balanced, and
performance identifiers. The default is none, preserving PowerDevil's
no-automatic-switch behavior when no PowerProfile key is stored.

Existing `power.idleDisplayOffMinutes`, `power.screensaver`, and
`power.screensaverMinutes` remain schema keys for the current Settings route
and compatibility. A stored `power.idleDisplayOffMinutes` choice is copied to each per-source
display-off enabled state and duration during schema migration (positive
minutes become seconds; non-positive values remain disabled) and takes
precedence over the legacy PowerDevil timeout. An absent legacy key stays
absent so schema defaults apply. Explicit values for any new native key also
take precedence independently for enabled state and duration. An old
disabled global preference does not synthesize a zero timeout when a new
per-source choice explicitly re-enables display-off. Defaults provide a usable policy where no supported
legacy choice exists; unsupported or malformed legacy fields are omitted
rather than converted to zero or an action.

## One-time PowerDevil import

Settings1 reads only an injected configuration path. Production supplies the
XDG config path `powerdevilrc`; tests use temporary fixtures and never read
the desktop user's configuration. The pure planner receives parsed entries
and the current user-override values, and recognizes only source keys
documented by the PowerDevil 6.6.6 KConfig definitions:

- `[AC|Battery|LowBattery][SuspendAndShutdown]`: `LidAction`,
  `InhibitLidActionWhenExternalMonitorPresent`, `SleepMode`, `AutoSuspendAction`, and
  `AutoSuspendIdleTimeoutSec`.
- `[AC|Battery|LowBattery][Display]`: dim, display-off, and
  `LockBeforeTurnOffDisplay` preferences.
- `[AC|Battery|LowBattery][Performance] PowerProfile`.
- `[BatteryManagement] BatteryCriticalAction`.

Action numbers are converted only through the cited PowerDevil enum mapping
in [ADR-0293](../adr/0293-settings1-native-power-policy-and-powerdevil-import.md).
Durations must be integer seconds within the schema bounds. The preservation
refinement and sleep-mode mapping are recorded in
[ADR-0297](../adr/0297-preserve-power-preference-precision.md). Unknown profile
identifiers and unsupported action values stay absent. PowerDevil defines no
critical-action countdown preference; Settings1 uses its bounded default.
Screensaver selection remains in the existing QindaQt setting because it is
not a PowerDevil preference.

The reader admits at most one MiB of valid UTF-8 configuration, then parses
that private bounded snapshot. KConfig cannot reopen a changed source to
bypass the size bound. The original file is never written.

The import runs only after the Settings1 process owns its D-Bus name. Imported
values and `power.migration.powerDevilImported` are committed together through
the repository's public transaction. A missing, unreadable, oversized, or
malformed source is left unmarked. A failed save changes neither values nor
marker, so a later Settings1 activation can retry. The marker and imported
preferences share the user-override document and follow Settings1's atomic
persistence contract.

PF2–PF4 add policy runtime, idle-stage evaluation, and presentation. The
native display-off stage now reads confirmed per-source Settings1 values and
selects AC, battery, or low-battery settings from the current authenticated
Power1 source and warning facts. `WarningLevel::Low`, `Critical`, and `Action`
select low-battery; unknown, none, and discharging select battery. The native
Settings route edits each source independently while showing which source is
active. The legacy global minutes route remains for compatibility and schema
migration, where an explicit old choice is copied to all three sources. The
stage boundary and limits are in [Native idle display stage](idle-policy.md).
Dim, lock-before-display-off, idle suspend, and capability masks remain
separate work until their consumers are complete.
