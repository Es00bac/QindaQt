# Native idle display stage

`session/idle_policy` owns the display-off idle stage. It is composed in the
resident `qindaqt-session` process next to `NativeLockRuntime`
([ADR-0319](../adr/0319-supervisor-owned-native-idle-display-policy.md)), with a separate
`ext-idle-notify-v1` observation so display-off timing does not replace the
independent automatic-lock timer.

The public `AttachedDisplayPowerPort` factory keeps protocol details inside
IdlePolicy. The supervisor supplies the connected ordinary Wayland file
descriptor returned by the admitted `CompositorAttachment`; the private
`KWaylandDpmsController` passes it to KWayland `ConnectionThread::setSocketFd`
and never resolves or reconnects `WAYLAND_DISPLAY`. The attachment remains the
owner of identity and lifetime evidence
([ADR-0305](../adr/0305-public-ordinary-compositor-attachment.md),
[ADR-0307](../adr/0307-public-ordinary-fd-idle-observation.md)). Revocation
disarms the idle stage, drops the observer, makes a best-effort `On` request
over the already-connected descriptor, flushes it on the connection worker,
then destroys DPMS, manager, output, registry, and event-queue wrappers before
disconnecting. Connection death uses local proxy destruction because protocol
release requests are no longer valid. Activity also requests `On`. Normal supervisor teardown calls the same flushed
restore before destroying the display-power port, so logout cannot discard a
buffered restore. A
replacement at the old socket path cannot receive either request through this
retained descriptor.

The display-off stage reads confirmed Settings1 values for each source:
`power.idle.<source>.displayOffEnabled` and
`power.idle.<source>.displayOffSeconds`. It selects AC whenever the
authenticated Power1 snapshot does not report battery; on battery,
`WarningLevel::Low`, `Critical`, or `Action` select low-battery preferences,
while unknown, none, and discharging select battery preferences. A stored
legacy `power.idleDisplayOffMinutes` preference is copied to all three source
profiles during schema migration; absent values remain absent so defaults
apply. Settings edits each source independently and shows the active profile.
The compatibility key remains available to older callers.

Positive confirmed timeouts arm one idle stage. Disabled values, missing
confirmed Ready Settings1 state (including any degraded retained snapshot),
unavailable DPMS, a Power1 source without a usable
snapshot, and a live Power1 owner without an authenticated inhibitor-state
receipt keep the stage disarmed. A current `DisplayOff` lease suppresses the
display-off stage and restores `On`; owner disappearance removes its leases,
while owner replacement waits for the replacement receipt. Power1 advertises
zero scopes until automatic lock, display off, and idle suspend are all
consumed by the complete shared policy. Manual locking does not depend on this
stage.

This implementation requests KWin per-output DPMS `Off`/`On` mode. It does
not disable outputs, alter output order, or touch layout/calibration. It does
not claim that a privileged external logind suspend can always be denied.

Native idle dimming, automatic lock before display-off, and idle-suspend
ordering remain separate consumers. The focused stage tests cover confirmed
source selection and transitions, source preference bounds and Settings
lineage, confirmed timing, activity restore, inhibitor receipt gating,
disabled preference, and attachment revocation. A private in-process Wayland
server exercises the actual KWayland adapter asynchronous capability arrival,
per-output requests, output removal, resume restore, final flushed `On`,
reconnect, and repeated teardown with Qt warnings fatal. This validates the
client protocol lifecycle without changing host display power; it does not
replace a nested compositor integration row.

PowerDevil remains an optional transitional supervisor child for brightness,
lid and profile behavior. Removing the desktop-controls preference binding
does not disable PowerDevil policy already stored in its legacy configuration.
Final PF2–PF4 retirement must remove that competing legacy display policy before
claiming one production authority. This source slice does not retire PowerDevil
or qualify ScreenSaver/portal inhibition.
