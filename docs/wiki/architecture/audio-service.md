# Audio service

Audio1 is QindaQt's typed, restart-aware control and observation boundary for
the running PipeWire graph. The D-Bus-activated `qindaqt-audio-service` owns
`org.qindaqt.Audio1`; upstream WirePlumber remains the policy manager. Audio1
does not open devices, transport samples, install PipeWire configuration, or
replace WirePlumber policy.

The exact wire contract is in the [Audio1 reference](../reference/audio1-v1.md).
The Qt/GLib ownership decision is recorded in
[ADR-0014](../adr/0014-confine-wireplumber-to-glib-worker.md).

## Module shape

| Module | Responsibility | Boundary |
| --- | --- | --- |
| `audio_protocol` | Typed values, fixed D-Bus marshalling, limits, and fail-closed validation | Qt Core/DBus only; no transport or platform handles |
| `audio_client` | Exact-owner discovery, snapshot fetching, invalidation coalescing, timeout recovery, and serialized public operations | Depends only on the protocol and Qt Core/DBus |
| `audio_service` | Backend abstraction, operation coordination, resident object/name ownership, process entry point, and WirePlumber adapter | Qt main thread publishes D-Bus; confined GLib worker owns every WirePlumber/GObject handle |

The service object never owns graph policy. The coordinator validates public
requests against its current immutable snapshot before dispatching to the
backend. The production adapter then resolves the same serial in the current
WirePlumber object manager and synchronizes the core after an accepted action.
These two checks prevent a disappeared or replaced object from being mutated
through an earlier snapshot.

## Authority and handle lineage

Every public handle is `(epoch, object.serial)`. `object.serial` is PipeWire's
stable object serial, not a transient bound object ID. Bound IDs are used only
inside one GLib worker turn when calling a WirePlumber API.

The service starts with a nonzero random epoch. Within one resident service
owner it strictly increases that epoch when the PipeWire connection is replaced
or the observed `wireplumber.daemon` client serial disappears or changes. All
handles from an earlier epoch are stale.
Pending operations cross that boundary as `Uncertain`; neither the resident
coordinator nor the public client replays them. A D-Bus unique-owner change is
an independent client authority change: the client discards its snapshot,
marks an in-flight mutation uncertain, and fetches from the new exact owner.

Snapshot revisions are monotonic within an epoch. A `Changed(epoch, revision)`
signal carries no graph data and only prompts a fetch. The client subscribes to
the current unique owner rather than the well-known name, rejects late replies,
coalesces invalidations while a fetch is active, and rejects older epochs,
regressing revisions, equal-revision content contradictions, and malformed
snapshots before publication. Public mutation results are always queued until
after the request ID returns; stop cancels undelivered results except for one
queued `client-stopped` uncertainty for a still-dispatched mutation, and object
destruction safely drops that queued delivery. An accepted new epoch immediately
makes any dispatched mutation uncertain; a delayed old-epoch result cannot
restore success and is never replayed.

## WirePlumber integration

One dedicated standard thread creates and owns a private `GMainContext`,
`GMainLoop`, `WpCore`, object manager, plugins, metadata objects, proxies, and
all other GObject references. Qt never receives those pointers. Only bounded
`Snapshot` and `BackendOperationOutcome` value copies cross to the Qt thread by
queued invocation; requests cross in the other direction as copied typed
values. Every adapter run has a fresh generation carried by both value types.
Generations are opaque equality tokens; they are never ordered numerically.
Stop invalidates that generation before joining the worker, and both the adapter
and coordinator drop queued values from stopped or superseded runs. Reusing the
backend advances the public service epoch before the new run can publish.

Core API component loads begin only after PipeWire accepts the private core.
Every component load and operation sync owns a cancellable tracked by the GLib
worker. Stop cancels all such work and keeps the context alive until every
completion callback releases its state, so thread join is both a callback and
resource barrier rather than merely a loop exit.

PipeWire disconnect cleanup is deferred out of the GObject signal turn through
an explicitly owned idle source. That source carries the worker-run token and
cleanup destroys it synchronously; stop therefore cannot leave a latch or stale
reset callback that suppresses or mutates the next run's disconnect handling.

The adapter loads WirePlumber 0.5's public default-nodes and mixer API modules.
It observes nodes, links, clients, and default metadata. It uses:

- `default-nodes-api` for current defaults and configured-default changes;
- `mixer-api` for normalized volume and mute reads/writes; and
- default metadata `target.object` with type `Spa:Id` and a target
  `object.serial` to move an application stream.

Capabilities are explicit. A missing plugin or metadata object degrades or
removes the corresponding capability; the service does not emulate it with
`wpctl`, parse command output, or claim success without the public API. Graph
records are sorted by serial and bounded before crossing the thread or process
boundary. If graph truncation would leave a stream target outside the retained
device set, the target becomes unknown and the snapshot is degraded.

## Availability and operations

Audio1 keeps lifecycle and domain truth typed locally; it does not introduce a
generic property map, Platform1 base class, or shared SDK availability type.
Two accepted service clients must establish common availability fields before
such a shared type is considered.

`SetDefault`, `SetVolume`, `SetMute`, and `MoveStream` return a typed result.
The service rejects unavailable state, stale/missing handles, incompatible move
targets, nonfinite or out-of-range volume, unsupported capability, malformed
enum values, and excess concurrency. A timeout, service-owner replacement,
WirePlumber replacement, or PipeWire disconnect makes a dispatched operation
uncertain because completion cannot be proven. Callers must refetch and show
that uncertainty; they must not retry automatically.

Backend operation outcomes are untrusted platform values. The coordinator
accepts only known status values, bounded stable reason-code tokens, safe
bounded diagnostics, and valid lineage. Any malformed field replaces the whole
outcome with the protocol-valid `Failed/backend-malformed` classification; raw
adapter text never reaches D-Bus.

## Activation and hardening

The build installs the executable, D-Bus activation descriptor, introspection
XML, and a systemd user unit. The user unit is D-Bus named, starts after the
normal PipeWire/WirePlumber user services, limits tasks and address families,
uses a private temporary directory/device view, and enables the available
filesystem, kernel, namespace, personality, privilege, and syscall hardening.
It does not start, reconfigure, or supervise upstream audio services.
The executable binds `org.freedesktop.DBus.Local.Disconnected` on the exact
constructing session connection to process exit. It never reconnects stale
in-memory graph/epoch state to a replacement bus; a replacement daemon must
activate a fresh process and epoch.

Diagnostics are short, control-character-sanitized, and contain no raw
properties, paths, process environments, stream data, or secrets. Stable
reason codes are the programmatic error surface.

## Consumer boundary

This slice exports typed C++ protocol and client libraries only. A later
Settings view model owns the stable route ID `audio` and may project the full
typed device/stream model. A future shell applet receives only a narrow
default-output facade plus `openAudioSettings()`; it receives no raw snapshot,
handle, graph, D-Bus object, service client, or provider authority. Neither UI
is implemented or qualified here.

## Qualification boundary

Focused protocol tests cover exact signatures, round trips, aggregate counts,
ordering, lineage, malformed enums/text/levels, and oversized arrays. Fake
backend/client tests cover model publication, operation validation, stale
handles, exact owner/revision handling, invalidation/refetch, timeout,
replacement, equal-lineage contradictions, queued exactly-once completion,
stopped/superseded backend generations, malformed backend outcomes, and no
replay. A 250-cycle production-backend test against an unreachable private
runtime bounds file-descriptor growth while exercising immediate start/stop and
callback cancellation. A separate scheduler regression forces a disconnect
reset idle to be pending when the higher-priority stop task runs, repeats the
stop/restart/loss sequence twice against private PipeWire processes, and proves
each second loss advances epoch and publishes. Private `dbus-daemon` tests cover
successive owners and executable activation/lifecycle.

The production adapter test launches disposable PipeWire and WirePlumber
processes against a private runtime directory and an invalid private D-Bus
address, creates only null sink/source fixtures, observes graph updates, changes
default/volume/mute, moves a synthetic playback stream, restarts WirePlumber,
and rejects the old handle. It never contacts the user's session bus or host
audio graph.

That evidence does not qualify USB, HDMI, Bluetooth, jack sensing,
multichannel/channel-volume semantics, a physical microphone or speaker,
suspend/resume, hotplug churn, realtime scheduling, memory/CPU budgets, or
either future UI. Those remain hardware and integrated-session gates.
