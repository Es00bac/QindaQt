# PowerDevil idle display preferences

The `QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter` is the narrow
preference boundary for idle display-off. PowerDevil 6.6.6 owns the idle timer,
inhibition policy, and DPMS action. QindaQt never creates a second timer,
observes compositor idle directly, or drives KWayland DPMS from this adapter.
The decision is fixed by [ADR-0105](../adr/0105-delegate-idle-display-off-to-powerdevil.md).

## Configuration contract

PowerDevil reads `powerdevilrc`. The adapter opens that file through KConfig,
reparses it before a write, and changes only these entries in each of the
`AC`, `Battery`, and `LowBattery` groups:

| Group | Key | Value |
| --- | --- | --- |
| `AC/Display` | `TurnOffDisplayWhenIdle` | requested enabled flag |
| `AC/Display` | `TurnOffDisplayIdleTimeoutSec` | requested minutes multiplied by 60 |
| `Battery/Display` | `TurnOffDisplayWhenIdle` | requested enabled flag |
| `Battery/Display` | `TurnOffDisplayIdleTimeoutSec` | requested minutes multiplied by 60 |
| `LowBattery/Display` | `TurnOffDisplayWhenIdle` | requested enabled flag |
| `LowBattery/Display` | `TurnOffDisplayIdleTimeoutSec` | requested minutes multiplied by 60 |

The timeout is bounded to 1–9,999 minutes, matching the range accepted by the
PowerDevil 6.6.6 duration editor. A disabled request still stores its bounded
timeout so enabling it later does not invent a duration. KConfig sync merges
unrelated entries and the adapter rejects a non-writable or failed config
sync without dispatching a daemon reload.

## Live adoption and ownership

After a successful sync, the adapter calls the supported session-bus method
`org.kde.Solid.PowerManagement.refreshStatus` at
`/org/kde/Solid/PowerManagement`. PowerDevil reparses its configuration and
reloads the active profile. The adapter reports the asynchronous result through
`applyFinished`, and exposes `available`, `applying`, and `error` properties for
its consumer.

Availability follows the exact owner of
`org.kde.Solid.PowerManagement`. A missing owner rejects new writes with
`powerdevil-unavailable` and leaves the config untouched. Owner loss during a
reload completes the request with `powerdevil-owner-lost`; a newly registered
owner triggers one refresh so a daemon restart adopts the persisted profile.
The adapter clears a stale unavailable error when the owner returns, then
reports any refresh failure. It does not create or monitor a replacement
service. The session composition owns the real `/usr/libexec/org_kde_powerdevil`
process and its lifetime; the adapter only consumes its documented D-Bus
authority. It starts the daemon after the shell, before desktop controls, and
reaps it when the session ends. One unexpected daemon exit gets an independent
restart without ending the desktop. An empty `powerDevilExecutable` in private
session options disables this child; tests never discover a host daemon from
that options default. Production executable selection and installed validation
remain part of the session composition qualification.

## Desktop-controls binding

`PowerDevilIdlePreferencesBinding` connects the Settings1 idle-preference
provider to the adapter in the desktop-controls process. It retains the latest
preference, coalesces changes while an asynchronous write and reload are in
flight, and drains that value through a queued callback. The queued boundary is
intentional: PowerDevil owner and apply signals can be delivered synchronously,
so applying from a signal handler could allow a recovery request to supersede
the consumer's request and strand the adapter's in-flight state.

When the PowerDevil owner is absent, the binding keeps the latest value and
replays it after the exact owner returns. A configuration or reload failure is
reported once and blocks repeated automatic retries until a new preference or
an owner transition provides an explicit retry boundary. Settings1 represents
a disabled idle policy with a zero-minute value; the binding stores the
PowerDevil-required positive timeout using the default ten-minute duration
while leaving the enabled flag false.

The production desktop-controls composition uses this binding and the
PowerDevil adapter instead of the old KIdleTime-to-DPMS path. The retained
idle-policy classes remain available to focused migration tests, but they are
not instantiated by the resident process and do not own display power.

## Verification boundary

The focused private-bus test uses a temporary `XDG_CONFIG_HOME` and a fake
PowerDevil owner. It covers exact minutes-to-seconds conversion, preservation
of unrelated keys, disabled settings, absent-owner rejection, owner restart
recovery, invalid timeout rejection, refresh failure, immediate consumer apply
on owner availability, and replacement-owner refresh ordering. It does not
touch host settings, DPMS, Wayland, or the system bus. End-to-end video/game
inhibition remains a session qualification: portal Idle and legacy ScreenSaver
inhibitors must be tested while the real PowerDevil owner is running.

The separate `qindaqt.session-powerdevil-lifetime` process test starts a private
helper as the daemon, terminates it once, verifies replacement without session
exit, and checks that session stop reaps the replacement. It does not start
PowerDevil or change host power settings.
