# Native shared idle policy

`session/idle_policy` owns source-specific dim, display-off and idle-suspend
policy. NativeLockRuntime remains the automatic-lock consumer. The supervisor's
NativePowerComposition assembles these consumers on separate ordinary
`ext-idle-notify-v1` observations, so changing one timeout cannot overwrite another.
See [ADR-0338](../adr/0338-own-scoped-display-power-and-shared-idle-composition.md),
[Power1](power-service.md) and [native sleep](../adr/0321-supervisor-owned-native-sleep-admission.md).

## Ownership and scoped display power

The supervisor borrows one admitted CompositorAttachment on its constructing
session bus. DisplayPower's ScopedDisplayPower binds the selected compositor's
actual unique owner, PID and UID. Nonce-correlated DisplayPower1 receipts describe
all enabled physical output identities, logical geometry, DPMS capabilities and
actual modes. Incomplete or unexplained mixed physical baselines fail closed.
The adapter uses the existing upstream global Workspace DPMS implementation;
it does not disable outputs or change their layout/calibration.

Admission starts false and only the current same-UID Session1 sender can enable
it while PowerDevil is absent. Independent finite idle and lid causes expire or
release through the original unique peer. The compositor records external Off
before upstream's same-mode early return. Releasing our cause preserves both
preexisting and later external Off. Physical wake consumes owned episodes.
Output topology/authority loss withdraws admission and owned causes. Cleanup
never reconnects to a replacement peer. Final acknowledgement is bounded to
250 ms and remains best effort when the original peer is unavailable.

ScreenPower1 shares Session1's actual constructing owner. Current Power1 caller
and epoch may request one persistent lid ScreenOff; the original caller and ID
can release it after capability loss. Lock-before-off uses the existing runtime's
actual Protected callback. A successful method reply means cause admission;
actual physical blanking comes from fresh mode receipts after backend completion.

The old AttachedDisplayPowerPort/KWayland adapter remains a compatibility library
and focused protocol fixture. Production NativePowerComposition uses scoped causes,
superseding [ADR-0319](../adr/0319-supervisor-owned-native-idle-display-policy.md)'s
unconditional all-output On restoration. Development OFF mode relies on the
transitional PowerDevil child and does not compose the old competing raw writer.

## Settings, activity and inhibition

All stages require Ready current-owner Settings1 values, a validated current
Power1 source snapshot and admitted ordinary attachment. Source selection is AC
when not on battery, lowBattery at Low/Critical/Action warning, otherwise battery.
Effective `power.idle.<source>` settings are typed and bounded to 0–14400 seconds:
`dimEnabled`, `dimSeconds`, `displayOffEnabled`, `displayOffSeconds`,
`lockBeforeDisplayOff`, `suspendAction` (none/suspend/hibernate), `suspendSeconds`.
Zero/disabled settings disarm their stage. Source/settings changes cancel owned
pending work without manufacturing user activity.

A genuine current-generation resumed event identifies activity. Startup, timer
rearm and unavailable state cannot create that event or replay a consumed episode.
Current-owner unknown inhibitor state fails closed. DisplayOff scope suppresses
both dim and display-off; AutomaticLock and IdleSuspend scopes gate their own
consumers. Epoch-fenced RegisterIdleConsumers admits the complete three-scope mask
only after the actual stage assembly/settings/source/scoped peer is ready.
Method-reported registration acceptance is not inhibitor truth: consumers use
PowerClient's authenticated current-owner receipt. Loss withdraws capabilities
and leases. See [ADR-0333](../adr/0333-authenticate-shared-idle-consumer-registration.md).

Dim adapts the upstream [PowerDevil DimDisplay 30% policy](https://invent.kde.org/plasma/powerdevil/-/blob/master/daemon/actions/bundled/dimdisplay.cpp),
retaining Dario Freddi's GPL-2.0-or-later attribution. It selects only the existing
Power1 admitted internal panel, never a fallback device, and restores the same
owner/epoch/handle only when authenticated readback still equals its dim value.
Mutation receipts alone do not imply physical readback. Unknown writes quarantine
the stage. Existing Power1 lacks an atomic expected-value comparison, so a concurrent
external brightness write racing stale readback remains a bounded limitation;
this candidate does not claim that race is qualified. External-monitor/keyboard
brightness and adaptive policy are outside this stage.

Idle suspend queries the selected mode's existing logind capability and uses the
existing Protected sleep coordinator. Own request-token cancellation cannot cancel
a replacement/manual request. Late Can responses and uncertainty cannot replay
an idle episode. External privileged sleep is not claimed to be vetoable.

## Cutover and evidence boundary

`QINDAQT_NATIVE_POWER_EXCLUSIVE` defaults OFF in source. The r5 installed
cutover selected ON but returned the physical laptop login to SDDM when the
exclusive receipt barrier failed. The r6 profile restores OFF pending the
physical gate in [ADR-0345](../adr/0345-hold-physical-native-power-cutover-until-receipts-pass.md).
An ON build supplies Power1's profile/critical/lid/idle native-exclusive
activation arguments and the supervisor's exclusive default. Explicit
`--native-power=off|exclusive` controls supervisor assembly; exclusive
suppresses only its owned PowerDevil child. Power1 remains installed in r6,
but its bus availability does not qualify full native idle and display policy.

Focused tests separate source validation/idle episodes, brightness readback and
external-value preservation, delayed sleep capability cancellation, exact owned
sleep cancellation, actual ordinary Wayland activity, real private-bus owner/nonce
receipts, Protected ScreenOff and independent cause release. Pure compositor
policy tests cover same-mode/preexisting external Off, wake, expiry and capacity.
These are component/private-protocol evidence. Full actual renderer/output/shared
inhibitor/lid-source-dock integration and package activation remain manager gates;
no physical host sleep or display mutation is an implementation verification step.

Exclusive startup now waits at most6seconds for authenticated display admission,
accepted full consumer registration, current source/settings/inhibition and native
lock/logind readiness. An unavailable prerequisite stops owned children and exits2;
it cannot retain a session with PowerDevil suppressed and native consumers absent.
The `off` mode preserves its compatible owned PowerDevil lifetime. New actual CLI
regression rows and the real nested DisplayPower fixture are source-ready; their
compiled/runtime evidence is a separate gate, not implied by this change.

The startup CLI regression runs on a private bus with no activation directories,
so installed Settings or other services cannot enter the fixture. The ordinary
parent caller is explicitly refused Logout; the existing actual supervised shell
helper performs authenticated cleanup. Probe PID/starttick and absence after
shutdown are retained in the actual test log. The first unisolated fixture failure
remains preserved separately and is not acceptance evidence.
