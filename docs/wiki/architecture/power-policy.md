# Native power policy preferences

PF1 establishes the Settings1 authority used by the later Power1 and idle
policy work. It does not execute power actions. Power1 remains the sole
runtime power-state authority; the shell and logind keep their existing
session-action boundary in [Power and brightness](power-service.md).

## Settings contract

Schema v2 defines bounded `power.lid.*`, `power.idle.*`,
`power.critical.*`, and `power.profile.*` values. Lid and idle settings are
per source (`ac`, `battery`, `lowBattery`). Lid actions are `none`, `suspend`,
`hibernate`, `lock`, or `screen-off`; docked actions use the same set and
default to `none`. Idle durations are whole minutes from 0 through 240.
Critical action is suspend, hibernate, or power-off, with a countdown
from 5 through 300 seconds. Power profile values are none (leave the
provider's profile alone), or the admitted power-saver, balanced, and
performance identifiers. The default is none, preserving PowerDevil's
no-automatic-switch behavior when no PowerProfile key is stored.

Existing `power.idleDisplayOffMinutes`, `power.screensaver`, and
`power.screensaverMinutes` remain schema keys for the current Settings route
and compatibility. A stored `power.idleDisplayOffMinutes` choice is copied
to each per-source display-off duration during migration and takes precedence
over the legacy PowerDevil timeout. Explicit values for any new native key
also take precedence. Defaults provide a usable policy where no supported
legacy choice exists; unsupported or malformed legacy fields are omitted
rather than converted to zero or an action.

## One-time PowerDevil import

Settings1 reads only an injected configuration path. Production supplies the
XDG config path `powerdevilrc`; tests use temporary fixtures and never read
the desktop user's configuration. The pure planner receives parsed entries
and the current user-override values, and recognizes only source keys
documented by the PowerDevil 6.6.6 KConfig definitions:

- `[AC|Battery|LowBattery][SuspendAndShutdown]`: `LidAction`,
  `InhibitLidActionWhenExternalMonitorPresent`, `AutoSuspendAction`, and
  `AutoSuspendIdleTimeoutSec`.
- `[AC|Battery|LowBattery][Display]`: dim, display-off, and
  `LockBeforeTurnOffDisplay` preferences.
- `[AC|Battery|LowBattery][Performance] PowerProfile`.
- `[BatteryManagement] BatteryCriticalAction`.

Action numbers are converted only through the cited PowerDevil enum mapping
in [ADR-0293](../adr/0293-settings1-native-power-policy-and-powerdevil-import.md).
Durations must be integral minutes within the schema bounds. Unknown profile
identifiers and unsupported action values stay absent. PowerDevil defines no
critical-action countdown preference; Settings1 uses its bounded default.
Screensaver selection remains in the existing QindaQt setting because it is
not a PowerDevil preference.

The import runs only after the Settings1 process owns its D-Bus name. Imported
values and `power.migration.powerDevilImported` are committed together through
the repository's public transaction. A missing, unreadable, oversized, or
malformed source is left unmarked. A failed save changes neither values nor
marker, so a later Settings1 activation can retry. The marker and imported
preferences share the user-override document and follow Settings1's atomic
persistence contract.

PF2–PF4 add policy runtime, idle-stage evaluation, and presentation. Until
those slices land, this schema is durable configuration only; it does not
inhibit logind, dim displays, lock, suspend, or change a Power Profiles hold.
