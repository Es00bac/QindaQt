# Night light

The `night_light` service is the resident schedule authority. Settings1 owns
persisted preferences under `display.nightLight.*`; this service calculates
bounded schedule frames and sends them to the compositor over a nonce-bound
session-bus stream. The KWin `nightlight` plugin remains the only live-output
authority and applies temperature factors per output. The public KWin
`org.qindaqt.KWin.NightLight` interface remains stable. This supersedes the
legacy split between `kwinrc`, `knighttimerc`, and `knighttimed` described by
[ADR-0136](../adr/0136-night-light-through-kwin.md); migration is covered by
[ADR-0313](../adr/0313-native-night-light.md).

## Module boundary

The module owns bounded night-light values, schedule calculation, a resident
Settings1-backed schedule service, and an authenticated schedule client. It
also retains the old config reader temporarily for one-time import only; it is
never a live write path or schedule authority. The live-state port and preview
continue to use the compositor's public NightLight interface. The module owns
no KWin private headers or output object, and exposes no UI.

The schedule service is an argumentless session-bus activated executable,
`/usr/bin/qindaqt-night-light-service`, and an optional resident child of the
session supervisor. It borrows Settings1 and Display1 public clients and the
constructing session bus connection. Display1 stable output IDs are resolved
to current compositor UUIDs in each frame; ambiguous or unavailable mappings
produce a fail-neutral frame so an opted-out display never receives a tint. When already started by D-Bus activation, a supervised sibling
exits successfully after observing that the service name is owned. Both paths
share one owner and teardown behavior.

## Values and schedule

The Settings1 keys are `display.nightLight.active` (false), `mode`
(`Constant`/`DarkLight`, default `DarkLight`),
`dayTemperatureKelvin` (6500 K), `nightTemperatureKelvin` (4500 K),
`scheduleSource` (`Location`/`Times`, default `Location`),
`automaticLocation` (true), `latitudeDegrees` and `longitudeDegrees` (0.0),
`sunriseStart` (`06:00:00`), `sunsetStart` (`18:00:00`),
`transitionSeconds` (1800), and `disabledOutputs` (empty). Temperatures are
bounded to 1000–6500 K on a 100 K grid; coordinates reject non-finite values
and are bounded to ±90/±180 degrees; times must be valid and sunrise must
precede sunset; transition is bounded to 60–7200 seconds. Disabled output IDs
are unique, bounded values using the compositor's public stable output
identity. Invalid values fail closed.

The pure calculator accepts an injected current time and optional automatic
location. Fixed times work without location. Manual location computes solar
events for the requested date and reports polar-day/night conditions as
unavailable when the configured sunrise/sunset cannot exist. Automatic
location is unavailable until a fresh, permitted location fix is available;
no stale coordinate is substituted. A missing clock or location input is
reported as unavailable instead of silently switching schedule source.

## Schedule1 transport

`org.qindaqt.NightLight` at `/org/qindaqt/NightLight`, interface
`org.qindaqt.NightLight.Schedule1`, exposes `Subscribe(ay)`,
`Unsubscribe(ay,t)`, and the `ScheduleFrame(ay,t,t,a{sv})` signal. A client
creates a fresh 16-byte nonce for each owner generation. The service accepts
only a unique bus caller and targets every frame to that caller's unique name.
Frames carry the nonce, a nonzero subscription cookie, a monotonic revision,
complete bounded settings and schedule values, and a diagnostic. Method replies
do not authorize schedule application; only a correctly targeted signal from
the pinned service owner does. Owner loss, replacement, malformed data,
nonce/cookie mismatch, timeout, or revision rollback clears the published
schedule and leaves the compositor in its neutral daylight state. A nonce is
never reused across an owner generation.

Settings1 owner loss clears the service's ready state and publishes an
unavailable frame. A successful Settings1 snapshot is the only source for
resident preferences. The service is active while Settings is closed and
refreshes on Settings1 invalidation, owner changes, and the bounded schedule
timer.

## Live output and status

The KWin plugin owns its DBus and output objects. Schedule frames select the
existing constant or transitioning temperature policy; each output can opt
out independently by its bounded stable identity. The resident service
resolves stable IDs to current runtime compositor UUIDs; the plugin matches
only those UUIDs. If selected identities cannot be mapped unambiguously, all
outputs remain neutral until a valid frame arrives. Opted-out outputs receive
unity channel factors against the existing ICC base. Schedule owner loss or
unavailability cancels transitions and returns output factors to neutral.
The QindaQt KWin interface remains the read/status and preview boundary for
Settings; Settings does not write files or address outputs directly.

## Focused proof

```sh
ctest --test-dir build/pf12 \
  -R '^qindaqt\.(settings-schema|night-light-schedule|night-light-values|night-light-schedule-client|night-light-schedule-service)$' \
  --output-on-failure --no-tests=error
```

The focused rows cover schema aliases and defaults, pure schedule boundaries,
bounded IDs and values, exact-owner/nonce/cookie/revision stream handling,
owner replacement and stale signal rejection, private-bus schedule service
publication, and Settings1-backed preferences. The fork plugin must also build
against the production KWin target. Hardware output coverage remains separate
from these deterministic tests.
