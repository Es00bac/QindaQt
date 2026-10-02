# Native power policy preferences

PF1 establishes the Settings1 authority consumed by Power1 and idle policy.
The bounded native source-profile runtime is described below. Power1 remains the sole
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

## Native source-profile runtime

[ADR-0330](../adr/0330-gate-native-source-profile-holds.md) composes the public
SettingsClient with Power1's validated coordinator facts in the resident
executable. Confirmed source facts select `power.profile.ac`,
`power.profile.battery` or `power.profile.lowBattery`; Low, Critical and Action
warnings choose low-battery only while on battery. Unknown source, unsupported
profiles or unconfirmed Settings authority admit no new hold. The upstream
[HoldProfile contract](https://upower.pages.freedesktop.org/power-profiles-daemon/gdbus-org.freedesktop.UPower.PowerProfiles.html)
only admits power-saver and performance. The stored balanced choice therefore
releases the native hold without claiming that balanced was applied. An
automatic balanced base-policy contract is deferred; changing ActiveProfile
would cancel other callers' holds. `none` releases
only this runtime's automatic hold and leaves external/user holds untouched.

The executable defaults to `--profile-policy=off`, including packaged
production. Explicit `--profile-policy=native-exclusive` composes policy and a
subscribe-before-query current-owner guard for PowerDevil's canonical session
bus name. Its confirmed absence admits policy; arrival or query/bus failure
revokes admission and schedules owned-hold cleanup. This option declares an
exclusive cutover; it never disables or launches the legacy writer. Final
supervisor and package cutover remain separate work.

One serialized nonce-tagged hold is replaced by releasing and observing its
absence before acquiring the next profile. Equivalent repeated facts cause no
hold churn. Current-owner targeted ProfileReleased refreshes authenticated
facts and retires the cookie. User cancellation suppresses acquisition until
a new source/preference/authority input or explicit retry, preserving manual
choice. Each successful hold operation refreshes authenticated provider
facts. Known refusals have no timer retry: changed admission or confirmed
preferences, or local explicit retry, may retry. Inventory membership is
canonicalized: reordering the same supported profiles or external holds is
not a new admission and never retries a known refusal. Hold calls are bounded to
three seconds; malformed successes, transport uncertainty and missing
convergence quarantine acquisition until a new runtime. A late observed tagged
hold may be cleaned up only if its returned cookie was recorded; a timed-out
acquire with no cookie cannot invent release authority. Uncertain acquisition
is never replayed. An uncertain release also fences that exact hold against
repeat dispatch, even after preferences change.
Settings owner loss or a pending/unconfirmed refresh prevents selecting a
new hold from retained preferences. A pending refresh may retain the already
confirmed hold; failure withdraws it. Global Power1 epoch changes discard retained handles.

`qindaqt.power-source-profile-runtime` executes the actual resident subprocess
against private UPower/PPD and resident Settings1, checking source transitions,
leave-alone cleanup, equivalent-fact stability, external holds, dormant default,
legacy authority arrival/loss, Settings owner loss and same-owner revision
regression, unsupported/provider loss,
hold limit admission changes, known rejection, acquisition/release timeout
no-replay, balanced deferral and actual targeted manual ProfileReleased. The daemon
has no host include or activation directory. These are private fixtures, not
installed or physical power qualification. Idle scopes stay zero; PF2 lid and
critical countdown, full idle policy, and PowerDevil retirement remain separate.

## Native critical-battery countdown

[ADR-0331](../adr/0331-fence-native-critical-battery-countdowns.md) composes
a distinct policy from confirmed public Power1/Settings1 facts. A separate
`--critical-policy=native-exclusive` option defaults off and shares the canonical
PowerDevil absence guard. Only authenticated on-battery/present supply facts
with WarningLevel Action admit the configured none/suspend/hibernate/power-off
action and 5–300-second countdown. None sends no notification or action.

The deadline uses a monotonic clock and starts only after a confirmed
actionable notification. Updates replace the same owned notification. User
cancellation/dismissal suppresses the entire critical episode. A started
attempt is also withdrawn on unconfirmed/changed preferences or any source,
provider, Settings, legacy or action-authority loss, without restarting on
repeated facts. Authenticated AC or a known warning below Action resets the
episode; Unknown cannot. Pending capability work is synchronously revoked
before dispatch, and uncertain action quarantines the policy runtime.

Confirmed notification closure and current policy lineage precede the public
SessionActions request. Suspend/Hibernate go through authenticated protected
Sleep1; power-off retains SessionActions' reviewed noninteractive admission.
The notification adapter checks exact owner, ID and a nonce action key and
serializes close behind pending updates. A late confirmed ID is cleaned up on
its original owner. A lost initial reply has no safe cleanup ID, so it never
authorizes action/replay; every publication requests finite positive remaining
plus 3-second expiry. A foreign nonconforming host cannot guarantee that bound.
`qindaqt.power-critical-battery-runtime` is the actual resident acceptance
fixture: minimum-duration actions, same-ID updates, public user cancellation,
none/default/legacy admission, warning/AC/Settings/provider/action-owner fences,
late capability and notification replies, revision regression, finite unknown-ID
expiry, uncertain no-replay, and strict 5–300-second typed bounds. Its wire fault
rows are separate from the real resident notification host/user presenter rows.
Private runtime verification remains the acceptance gate for this source slice;
no installed/hardware qualification or whole PF2 completion is implied.
