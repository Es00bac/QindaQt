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

`QINDAQT_NATIVE_POWER_EXCLUSIVE` defaults OFF. Reviewed package cutover ON supplies
Power1's profile/critical/lid/idle native-exclusive activation arguments and the
supervisor's exclusive default. Explicit `--native-power=off|exclusive` controls
supervisor assembly; exclusive suppresses only its owned PowerDevil child.
Bare Power1 policies remain OFF. Source integration does not enable installed
host settings or qualify actual native Workspace DPMS.

Focused tests separate source validation/idle episodes, brightness readback and
external-value preservation, delayed sleep capability cancellation, exact owned
sleep cancellation, actual ordinary Wayland activity, real private-bus owner/nonce
receipts, Protected ScreenOff and independent cause release. Pure compositor
policy tests cover same-mode/preexisting external Off, wake, expiry and capacity.
These are component/private-protocol evidence. Full actual renderer/output/shared
inhibitor/lid-source-dock integration and package activation remain manager gates;
no physical host sleep or display mutation is an implementation verification step.
