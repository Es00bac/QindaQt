# ADR-0313: Native night-light schedule authority and Settings1 preferences

- **Status:** Accepted
- **Date:** 2026-09-29
- **Supersedes:** [ADR-0136](0136-night-light-through-kwin.md) for schedule ownership and persisted preference ownership

## Context

PF12 and PF13 remove the runtime dependency on KNightTime and `knighttimed`.
The compositor must continue to own live color output changes, while QindaQt
needs one resident schedule authority and native persisted preferences.
Settings also needs a one-time migration from the existing `[NightColor]` and
`knighttimerc` values without overwriting a user's native Settings1 values.

## Decision

Settings1 owns the model-aligned `display.nightLight.*` preferences. The
existing `org.qindaqt.KWin.NightLight` interface remains the compositor's
public live-state and preview boundary. The compositor's `nightlight` plugin
consumes schedule frames and is the only component that applies output
temperature factors. The module handles validated values and schedule
calculation without importing KWin private headers.

The schedule authority is the resident `qindaqt-night-light-service`, exposed
as `org.qindaqt.NightLight.Schedule1` on the session bus and eligible for the
session supervisor's optional resident-child policy. It reads Settings1
through its public client boundary, so scheduled behavior continues while the
Settings application is closed. The executable takes no arguments and owns no
settings file.

Schedule subscriptions are bound to the service's actual unique bus owner.
Each client generation uses a fresh 16-byte nonce; frames also carry a
nonzero cookie and monotonic revision. The service emits targeted signals to
the subscribing unique name. A method reply is not a schedule receipt. Owner
loss/replacement, invalid input, timeout, stale revision, or missing automatic
location makes the schedule unavailable and leaves the compositor at neutral
daylight output. No stale location or alternate schedule source is substituted.

Settings values retain the existing typed defaults and tokens:

| Key suffix | Type and default | Bound |
| --- | --- | --- |
| `active` | bool, false | — |
| `mode` | enum, `DarkLight` | `Constant` or `DarkLight` |
| `dayTemperatureKelvin` | integer, 6500 | 1000–6500 K, 100 K grid |
| `nightTemperatureKelvin` | integer, 4500 | 1000–6500 K, 100 K grid |
| `scheduleSource` | enum, `Location` | `Location` or `Times` |
| `automaticLocation` | bool, true | — |
| `latitudeDegrees` | double, 0.0 | −90–90; finite |
| `longitudeDegrees` | double, 0.0 | −180–180; finite |
| `sunriseStart` | time string, `06:00:00` | valid local time |
| `sunsetStart` | time string, `18:00:00` | valid and after sunrise |
| `transitionSeconds` | integer, 1800 | 60–7200 seconds |
| `disabledOutputs` | string list, empty | unique bounded compositor output IDs |

Location-based schedules use either explicit coordinates or a fresh, permitted
automatic-location fix. Fixed `Times` schedules do not require location.
Unavailable system clock, time zone, permission, or fresh location is reported
as unavailable rather than silently changing behavior.

The old config port becomes read-only migration input. Import is attempted only
while the persisted import marker is false. Existing native user-layer values
win per key; valid legacy values fill only keys without a native override.
Malformed legacy input is surfaced to Settings and does not set the completion
marker. Successful values are saved through Settings1 and the marker is
persisted only after all writes are acknowledged. A failed or uncertain save
keeps import incomplete so Settings can retry from a fresh authoritative
snapshot. After migration, the legacy files and `knighttimed` are not read or
written as live state.

Per-output opt-out matches the compositor's public stable output identity, not
a connector alias synthesized by Settings. An opted-out output receives unity
night-light factors while retaining its existing ICC color base.

## Consequences

- KNightTime and `knighttimed` are not runtime or schedule dependencies.
- Settings preferences are recoverable through Settings1 source layers and
  revisions; the legacy configuration is only migration input.
- The native schedule service can outlive the Settings UI and has no authority
  to apply output changes directly.
- Automatic location depends on an authorized location provider. When it is
  missing, denied, or stale, the schedule remains unavailable.
- Existing KWin status, preview, and compositor output behavior remain public
  QindaQt interfaces.

## Verification

PF12 verifies the pure schedule calculator, Settings schema/defaults,
nonce/cookie/revision and exact-owner checks, owner replacement, malformed
frame rejection, and a real private session-bus service/client receipt.
PF13 verifies native precedence, one-time migration, malformed input, retry
after failed saves, schema persistence of the completion marker, and the
Display Settings route. The isolated compositor plugin build and output
fixtures provide the compositor half of the acceptance evidence.
