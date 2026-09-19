# QindaQt Settings — Audio route

## 2026-09-19 source diagnosis

Inspection of base `01e919f6` found the earlier drag repair covers ordinary
device and stream rows but not the mixing console. `AudioConsoleSection`
still gives QVariantLists directly to both card Repeaters, so a console
snapshot replaces the held card. `AudioConsoleFader` also imperatively
assigns `livePosition` after binding it to the published position and never
restores that binding. The console still disables all its controls using
the client's global busy flag.

Every Settings console dispatch returns an AudioClient request id, but none
records it. `handleOperationCompleted` only recognizes device/stream ids,
so failed console changes, including an overlapping request answered Busy,
are silently dropped. The correction must retain the console cards, coalesce
the fader's latest gesture while a request is in flight, resume authoritative
position binding, and track console completions through the existing feedback
path. This diagnosis was recorded before edits; no compilation or runtime
check has been run for the candidate.

`qindaqt-settings --page audio` is the first-party audio settings surface. It
is a modular Settings Center route composed exclusively through the public
Audio1 client boundary. The route observes bounded device and stream truth and
offers only the actions that the current authoritative snapshot admits. The
composition follows [ADR-0048](../adr/0048-settings-center-navigation-and-route-ownership.md);
the Audio1 wire contract is in the [Audio1 reference](../reference/audio1-v2.md)
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
capability bit, and the per-target can-set flags. An enabled control can
therefore never be locally refused, and a disabled one is never dispatched.
Serials presented to QML are re-resolved against the current snapshot before
every dispatch, so a vanished or epoch-replaced target is refused locally
instead of being sent as a stale handle.

Pending state is per target, not a single page-wide flag (mirrors
[ADR-0191](../adr/0191-a-control-survives-reprojection.md) in the shell audio
applet, which named the same bug in the applet's own layer first): while one
row's request is in flight, availability keeps reading true for every row,
including that row's own, and a `pending` field on the row is presentation
only, never a gate. A second dispatch for the *same* target while it is still
pending is refused locally ("Another audio change is still in progress.");
a dispatch for a *different* target is accepted, even though the client's own
transport still serializes at the wire (a genuinely overlapping request there
resolves as `OperationStatus::Busy`/"the audio service is busy", surfaced the
same way a real refusal is, never silently dropped and never blocking the
row that dispatched it). An earlier version of this route kept one shared
pending intent and folded the client's own single-operation fence into the
availability predicate above, which disabled every row on the page for the
duration of any one row's request — the exact defect the applet's ADR-0191
diagnosed, here at the model layer instead of the QML Repeater layer.

Retry performs one bounded stop/start rediscovery of the public client; the
public client exposes no on-demand refetch because discovery and invalidation
are automatic. A successful operation reply triggers the client's own
authoritative refetch; the route never optimistically edits inventory.
Timeout, owner or authority replacement, or another uncertain result stays
visible and is never automatically replayed.

`AudioDeviceSection.qml` and `AudioStreamSection.qml` bind their `Repeater`s
by row *count*, not the row list itself (same fix as the applet's
ADR-0191): each delegate reads `root.deviceRows[index] ?? null` (or
`streamRows`) rather than taking the list as `Repeater.model` directly. A
`Repeater` given a `QVariantList` recreates every delegate whenever that list
is reassigned, and the model reassigns it on every reprojection — including
the one a row's own dispatch triggers — which destroys the very `Slider` a
drag is holding. Every field the delegate reads from a possibly-null
`modelData` uses optional chaining (`?.`/`??`); a device disappearing from
the middle of the list shifts indices below it, so a held slider can
momentarily dispatch to the device that took its index — a known limitation
shared with the applet, not fixed here (the real fix is a
`QAbstractListModel`).

## Composition and authority boundary

The closed Settings route registry maps only the canonical `audio` id (sixth
built-in route, Ctrl+6) to the compiled Audio component. Unknown and
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
notices use truthful visible text rather than color alone. A volume slider
dispatches on every move, pointer drag included — not release-only — and a
pressed slider owns its displayed value (a `Binding { when: !pressed }`, not
a plain reactive property), so a reprojection mid-drag, including the one the
row's own dispatch triggers, cannot pull the handle out from under the
pointer; keyboard steps (`pressed` is already `false`) resume the
authoritative binding immediately after each one, same as before.

## The console grid

Every console card — input strip and output bus alike — is 120 px wide and is
built from the same four bands, in the same order, at the same heights:

| Band | Height | Strip | Bus |
| --- | --- | --- | --- |
| Name | 18 px | label, and whether the strip is hardware or virtual | label, and whether the bus is physical or virtual |
| Assignment | 24 px | the capture device the strip follows | the output device the bus drives |
| Desk | 150 px | meter, fader, and the routing/mono/solo/mute pads | meter, fader, and the mono/mute pads |
| Actions | 18 px | `Rack` | `Record`, `Rack` |

**A band that does not apply to a card keeps its slot rather than collapsing.**
A virtual strip has nothing to assign and a virtual bus has no rack, so those
controls are emptied in place. This is the rule the whole grid rests on: before
it, a virtual strip hid the device picker a hardware strip carried and lifted
its entire desk band above its neighbours, and buses put their picker at the
bottom while strips put it at the top, so no two cards in a row showed their
faders over the same pixels. A console is read across, and a ragged grid is a
misread level. `qindaqt.settings-audio-page` fails if the two files drift
apart.

Cards flow left to right and wrap onto as many rows as the window is wide;
there is deliberately no horizontal scroller anywhere in this surface.

One presentation note that is a bug fix rather than taste: the shared
`QindaQt.Controls` `ComboBox` draws its closed face with a read-only
`TextField`, and a `TextField` whose text is wider than its box scrolls to keep
the end visible — so a long device name lost its **first** characters and read
as "k Microphone" instead of "Desk Microphone". Every non-editable picker in
this route overrides that content item with an eliding `Text`, which drops
characters from the end where the eye expects it. The shared control has the
same defect for every non-editable consumer.

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
