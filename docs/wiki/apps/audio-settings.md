# QindaQt Settings — Audio route

`qindaqt-settings --page audio` is the first-party audio settings surface. It
is a modular Settings Center route composed exclusively through the public
Audio1 client boundary. The route observes bounded device and stream truth and
offers only the actions that the current authoritative snapshot admits. The
composition follows [ADR-0048](../adr/0048-settings-center-navigation-and-route-ownership.md);
the Audio1 wire contract is in the [Audio1 reference](../reference/audio1-v1.md)
and the [Audio service architecture](../architecture/audio-service.md) owns the
resident service boundary.

## Truth shown by the route

The route presents the current public snapshot in three groups:

| Group | Public truth | Interaction |
| --- | --- | --- |
| Output devices | Bounded name/description, default badge, volume, mute | Set default when admitted; volume and mute per device |
| Input devices | Bounded name/description, default badge, volume, mute | Set default when admitted; volume and mute per device |
| Application streams | Application and media name, direction, current target, volume, mute | Volume and mute per stream when admitted |

The default output and input names are shown beside the inventory. The route
model retains the exact owner, epoch, and revision for lineage gating and
focused diagnostics; the ordinary page does not render the broker's technical
owner identifier. Empty states distinguish no observed devices or streams from
loading, service unavailability, retained stale truth, and a failed or
uncertain operation. The route never exposes PipeWire object paths,
WirePlumber properties, raw diagnostics, or per-channel level detail; unknown
volumes are labeled as unknown rather than drawn as a handle position.

Audio1 exposes a bounded stream inventory, so per-stream volume and mute are
presented from the protocol surface. Stream *movement* between devices is an
advertised Audio1 capability that this route slice deliberately does not
expose; the boundary check rejects a stream-move intent surface.

## Owner, lineage, and operation lifecycle

One process-lifetime route model projects one public `AudioClient`:

- Loading has no authoritative inventory and offers only retry.
- Ready inventory is tied to the exact public owner, epoch, and revision.
- A degraded snapshot retains inventory, is labeled limited, and remains
  action-admitted because the public client dispatch preflight admits exactly
  that truth.
- Retained truth after a failed refresh is labeled stale; the client's own
  200 ms recovery refetch continues independently.
- Public-owner loss clears inventory immediately. Replacement truth appears
  only after the new exact owner publishes an accepted snapshot; a late reply
  from a retired owner cannot repopulate or complete the route.

Displayed action availability and dispatch admission are one predicate,
derived from the same public snapshot facts the client's dispatch preflight
consumes: retained snapshot presence, the snapshot's own availability, the
capability bit, the per-target can-set flags, and the client's serialized
single-operation fence. An enabled control can therefore never be locally
refused, and a disabled one is never dispatched. Serials presented to QML are
re-resolved against the current snapshot before every dispatch, so a vanished
or epoch-replaced target is refused locally instead of being sent as a stale
handle.

Retry performs one bounded stop/start rediscovery of the public client; the
public client exposes no on-demand refetch because discovery and invalidation
are automatic. A successful operation reply triggers the client's own
authoritative refetch; the route never optimistically edits inventory.
Timeout, owner or authority replacement, or another uncertain result stays
visible and is never automatically replayed.

## Composition and authority boundary

The closed Settings route registry maps only the canonical `audio` id (sixth
and last built-in route, Ctrl+6) to the compiled Audio component. Unknown and
path-like startup values exit before the Audio transport or model is
constructed. `qindaqt-settings` owns one public Audio transport, client, and
route model on the session bus for its process lifetime; only the QObject
route projection crosses into QML.

Neither QML nor the route model imports private Audio service headers,
WirePlumber or PipeWire headers, or Qt D-Bus. The resident boundary remains
the sole platform adapter and policy authority.

## Responsive interaction and accessibility

The page uses only QST-1 semantic roles and QindaQt.Controls. At compact
sizes the same ordered content remains vertically scrollable. Page Up/Page
Down and Ctrl+Home/Ctrl+End move through it, and changing keyboard focus
reveals the focused control. The page nominates the first enabled, admitted
control in traversal order — set-default, volume, then mute within each row;
output rows before input rows before stream rows — as the Settings host entry
target, recomputed whenever the projection changes, so a control the
snapshot disabled (for example a default output with `canSetVolume == false`)
is never targeted while another admitted action exists. When no admitted
control exists the page falls back to Retry, then the route surface itself.
Closing Settings remains a single window-level action; the page does not add a
second Close button. Forward and reverse Tab stay within the route and use the
route surface when no admitted domain action is available.

Device and stream rows expose accessible names, descriptions, and current
state. Sliders announce the target and the known percent level; switches
announce the target they mute; the set-default action names the device and
kind it would make default. Stale, degraded, unavailable, pending, and error
notices use truthful visible text rather than color alone, and the slider
dispatches on release or keyboard step while a pointer drag stays quiet.

## Verification and stopping point

Focused selection:

```sh
ctest --test-dir build/dev --output-on-failure --no-tests=error --parallel 1 \
  -R '^qindaqt\.settings-audio-(model|model-adversarial|page|boundary|boundary-poison)$'
```

- the model row proves bounded projection, exact lineage, capability and
  can-set admission, one-shot set-default/volume/mute dispatch for devices
  and streams, and bounded failures;
- the adversarial row proves owner loss and replacement, epoch fencing of
  pending operations and old serials, ignored foreign and late completions,
  retained stale truth, and retry rediscovery without replay;
- the page row proves accessible controls, action wiring including slider
  release and switch toggles, stale/owner-loss fail-closed presentation,
  compact focus reveal, and keyboard cycling — including the negative
  control where a valid snapshot leaves the default output with no admitted
  control and host entry must fall through to the first admitted action
  elsewhere, and safe focus fallback when Retry is visible and hidden; and
- the Settings Center navigation row additionally proves Ctrl+6 selection,
  the Audio route tab's accessible name/role, and Tab entry plus Escape
  return in both the wide (720×520) and compact (440×360) host layouts.
- the boundary and poison rows reject private service, WirePlumber/PipeWire,
  and Qt D-Bus sources, an invokable outside the closed intent surface, text
  entry, a stream-move surface, or a private service dependency.

The same selector runs in strict Debug and Release builds. Settings Center's
route and installed-package rows additionally prove canonical `--page audio`
startup, complete relocated construction, withheld-module failure, and hostile
route rejection. Tests use the injected fake transport and never touch the
host session bus, PipeWire, or WirePlumber.

This slice does not claim stream movement, per-channel balance, profile or
port selection, sample-rate configuration, an equalizer, persistence, a shell
applet (owned separately), physical audio hardware qualification, or
session-runtime integration.

## Recovery presentation

Opening Audio now activates a cold installed service through the public client.
The footer shows only an active change, without service epoch/revision counters.
Failure text explains refreshing devices rather than exposing raw reason codes.
