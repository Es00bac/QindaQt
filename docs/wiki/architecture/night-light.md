# Night light

The night light service owns bounded night-light values, one injected-path
config port over the two persisted files, one live-state port over KWin's
public NightLight interface, and one availability monitor for the schedule
daemon. KWin's nightlight plugin stays the only live-output authority and
`knighttimed` stays the only schedule authority; the decision record is
[ADR-0136](../adr/0136-night-light-through-kwin.md). The Display route consumes the
public boundary through its own composition singleton (see
[display settings](../apps/display-settings.md)); the shell quick-toggle that
can hold a real inhibition is a later outcome.

## Module boundary

The module is Qt Core value types plus Qt DBus transports over injected
connections, with `KConfig` (KF6::Config::Core) as the one allowed KDE
dependency for owned-key IO. It owns no platform object, no KWin header, no
schedule computation, no `knighttimestaterc` access, no Settings persistence,
and no QML. File paths and `QDBusConnection` instances are constructor-injected, so
the module never resolves HOME, XDG variables, or standard locations itself,
and every consumer — including tests — stages its own disposable files and
private buses. A source-policy test row rejects forbidden dependencies, and a
poison-negative row plants one in a disposable copy to prove the policy is not
vacuous.

## Values and bounds

`NightLightSettings` is one output half (`kwinrc [NightColor]`) and one
schedule half (`knighttimerc`). The enumerators persist exactly as the
authoritative schemas define them — KWin's `nightlightsettings.kcfg` choice
names `Constant`/`DarkLight` and the schedule daemon's `Location`/`Times` —
never as integers. The installed D-Bus XML's "0 automatic … 3 constant" mode
documentation is outdated and is rejected as hostile input. Bounds: night and
day temperatures 1000–6500 K on a 100 K grid (the kcfg declares no range, so
this module's bound is the contract), latitude ±90 and longitude ±180 decimal
degrees with NaN/infinity refused, schedule times valid `QTime`s with sunrise
strictly before sunset, and transition length 60–7200 seconds (default 1800,
stored in seconds per the upstream migration). Validation is fail-closed:
unknown tokens and out-of-bounds numbers never normalize silently.

## Config port

`NightLightConfigPort` reads and writes exactly the owned keys: `kwinrc
[NightColor]` `Active`, `Mode`, `DayTemperature`, `NightTemperature`, and
`knighttimerc` `General`/`Location`/`Times` `Source`, `Automatic`,
`Latitude`, `Longitude`, `SunriseStart`, `SunsetStart`, `TransitionDuration`.
Values are read as raw strings and parsed locally — a typed `readEntry` would
silently swallow a malformed stored entry into its default, while this module
must fail the whole read. Every other group and key survives untouched, values
that already read back equal are never written, and the result distinguishes
`Applied`, `Unchanged`, and `Failed`. Reads classify as `Absent` (nothing
stored: defaults), `Loaded`, or `Failed` (a stored token or number is
hostile); a failed read is never repaired by merging, a later write converges
the two owned key sets with validated values. `changedExternally()` reports
outside edits, with the port's own write echoes suppressed by comparing file
identities, so subscribers never see their own writes as external intent.

## State port and schedule monitor

`NightLightStatePort` publishes complete `NightLightStatus` frames read from
`org.kde.KWin.NightLight` properties. Every bus read is an asynchronous queued
pending call — blocking calls never reliably complete against peer
connections in this environment — and `PropertiesChanged` is treated as an
invalidation hint only: the frame that follows is always re-read through
`GetAll`, so a hostile partial notification can never publish on its own. A
frame with an unknown mode integer, an out-of-window temperature, a wrong
property type, or missing properties is rejected whole; the last complete
frame stays published and the port reports degradation. Service loss publishes
the explicit unavailable frame; a frame that fails validation right after the
service (re)appeared publishes the unavailable frame instead, so a hostile
producer can never masquerade as a working one. `preview` forwards one
in-range temperature to KWin's 15-second preview and refuses out-of-range
values locally; `stopPreview` ends it early. `NightTimeScheduleMonitor`
answers only whether `org.kde.NightTime` is currently on the bus, failing
closed; it never subscribes to the schedule, never activates the daemon, and
never becomes a second schedule authority.

## Focused proof

```sh
ctest --test-dir build/debug \
  -R '^qindaqt\.night-light-' --output-on-failure --no-tests=error
```

Six rows: values (bounds, snapping, coordinates, times, token and bus-mode
mapping with the outdated documentation as negative control), the config port
(temp-dir round-trips, byte-stable unchanged writes, unrelated-key survival,
hostile stored values that fail closed and converge on rewrite, refused
invalid writes, external-change truth with suppressed self-echoes), the state
port against a faithful fake `org.kde.KWin.NightLight` on a private bus
(missing service, live updates, out-of-range and malformed frames, preview and
stopPreview forwarding, connection-scoped inhibit release), the schedule
monitor's fail-closed availability, the boundary policy row, and its poison
negative. These are deterministic module evidence; the private headless KWin
proof that writes temperatures through a real compositor is recorded
separately in the ADR.
