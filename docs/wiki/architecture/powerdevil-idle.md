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
authority.

## Verification boundary

The focused private-bus test uses a temporary `XDG_CONFIG_HOME` and a fake
PowerDevil owner. It covers exact minutes-to-seconds conversion, preservation
of unrelated keys, disabled settings, absent-owner rejection, owner restart
recovery, invalid timeout rejection, refresh failure, immediate consumer apply
on owner availability, and replacement-owner refresh ordering. It does not
touch host settings, DPMS, Wayland, or the system bus. End-to-end video/game
inhibition remains a session qualification: portal Idle and legacy ScreenSaver
inhibitors must be tested while the real PowerDevil owner is running.
