# Audio service

## 2026-09-19 endpoint lifetime diagnosis

At base `01e919f6`, send and rack modules register a PipeWire destroy listener
through `watchModule`, but virtual-bus endpoint modules do not. If PipeWire
destroys a bus loopback, `m_endpointModules` retains its dead pointer: later
rebuilds treat the bus as already requested, and shutdown attempts to destroy
the stale pointer again. Endpoint modules need the same exact-pointer destroy
bookkeeping as send and rack modules; their teardown must take the map before
callbacks can erase entries. This source diagnosis precedes the correction
and does not claim a reproduced live-service crash.

The candidate registers virtual-bus loopbacks with the existing destroy
listener and takes the endpoint module map before teardown. A callback removes
an entry only when its stored pointer is the module being destroyed, so a late
destruction cannot remove a replacement with the same name. Raw lingering-sink
proxy recovery remains outside this module-lifetime correction.

The coordinated production build compiled this correction successfully.
The focused Settings Audio model/page checks passed; they do not exercise
PipeWire destroying a virtual-bus module or qualify physical audio hardware.

Audio1 is QindaQt's typed, restart-aware control and observation boundary for
the running PipeWire graph. The session supervisor restarts its user unit at
each desktop session start and waits for a previous Audio1 bus owner to retire.
This handles package upgrades when the systemd user manager persists across
logout and would otherwise retain a process running an older D-Bus ABI. The
wait is bounded; if the prior owner remains, the session logs the failure and
continues so an audio lifecycle fault cannot block the desktop. The unit keeps
its existing D-Bus activation boundary on QindaQt's private session bus
(ADR-0170). The D-Bus-activated `qindaqt-audio-service` owns
`org.qindaqt.Audio1`; upstream WirePlumber remains the policy manager. Audio1
does not replace WirePlumber policy or install PipeWire configuration.
Its VBAN worker does capture, transport, and receive user-enabled stereo audio
through the graph as described below.

The packaged user unit permits 128 tasks. The live console graph needed 32
threads on qinda-top; the former 16-task limit stalled Audio1 and PipeWire
clients, including System Settings and speech playback.

The exact wire contract is in the [Audio1 reference](../reference/audio1-v2.md).
The Qt/GLib ownership decision is recorded in
[ADR-0014](../adr/0014-confine-wireplumber-to-glib-worker.md); the audio graph
direction is recorded in
[ADR-0123](../adr/0123-voicemeeter-class-audio-graph-on-pipewire.md).

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

## Manual peer audio boundary

Audio1 schema 12 adds typed stream-definition upsert/delete and a
`ManageVbanStreams` capability; [Audio1 schema 12](../reference/audio1-v12.md)
defines the public fields. `VbanStore` owns the atomic JSON document, the
coordinator owns enabled state and publication, and the GLib worker alone owns
UDP threads and PipeWire stream/loopback lifetime. Settings sees bounded
bus/output choices through the public snapshot and sends typed operations; it
never edits the file or graph directly.

An incoming definition grants one exact source IPv4, UDP port, and physical
speaker. The receiver checks the datagram's actual source before decoding.
The worker sends received audio through a dedicated exact-target loopback
with fallback disabled, separate from console mixer routes. A disappeared
speaker withdraws the path. Backend-observed connected links on the selected capture or speaker path
drive `active`. The coordinator gives each continuous backend declaration a
nonzero activation token and accepts link evidence only for that exact
declaration. Retargeting, disabling, or re-enabling mints a new token, so
a delayed report from an old route cannot make a new route active. Network
delivery and acoustic output are not observed.
[ADR-0246](../adr/0246-configure-manual-audio-peers-through-audio1.md)
records the decision and [Audio VBAN streams](../reference/audio-vban-streams.md)
gives the manual setup steps.

## Per-device latency offsets

Schema 13 ([Audio1 schema 13](../reference/audio1-v13.md),
[ADR-0288](../adr/0288-per-device-latency-offsets-and-a-compact-audio-page.md))
publishes each device node's PipeWire `latencyOffsetNsec` and lets a client
set it. Three collaborators split the work:

| Collaborator | Responsibility |
| --- | --- |
| `LatencyStore` | Atomic, bounded `node.name → offset` document owned by Audio1 |
| Coordinator (`audio_latency_operations.cpp`) | Admits `SetLatencyOffset` against the retained snapshot, remembers the value, declares the whole map (`applyLatencyOffsets`) at start and on change |
| Graph worker (`wireplumber_worker_latency.cpp`, `latency_policy`) | Reads `Props`/`PropInfo`, writes declared offsets to present device nodes, reconciles resets |

The worker caches node `Props` (`PARAM_PROPS`) and enumerates each device
node's `PropInfo` once per serial to learn its settable range. It writes a
declared offset only when that range admits it, never resends while its last
write is unechoed, answers a device that resets the value (a Bluetooth route
re-emits its own offset on connect) with at most three writes per declaration,
and never writes a device with nothing declared. A device's `params-changed`
rebuilds the snapshot only when its offset changed. The offset is reported
latency: players and latency-compensating PipeWire modules follow it; plain
console loopbacks do not delay audio because of it.

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
- `mixer-api` for normalized volume, per-channel volumes, and mute
  reads/writes — channel maps come from the node's `audio.position`, never
  from the mixer dictionary, which can lag the negotiated layout;
- default metadata `target.object` with type `Spa:Id` and a target
  `object.serial` to move an application stream; and
- the `adapter` factory with `support.null-audio-sink` for managed virtual
  devices, and `wp_global_proxy_request_destroy` bounded by the
  `qindaqt.virtual.` node-name prefix for their removal.

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

`SetDefault`, `SetVolume`, `SetMute`, `MoveStream`, `SetChannelVolumes`,
`CreateVirtualDevice`, and `RemoveVirtualDevice` return a typed result. The
service rejects unavailable state, stale/missing handles, incompatible move
targets, channel-count mismatches against the retained layout, non-managed
removal targets, nonfinite or out-of-range volume, unsupported capability,
malformed enum values, and excess concurrency. A timeout, service-owner
replacement, WirePlumber replacement, or PipeWire disconnect makes a dispatched
operation uncertain because completion cannot be proven. Callers must refetch
and show that uncertainty; they must not retry automatically.

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

The unit sets `TimeoutStartSec=20` (systemd's `Type=dbus` default is 90s).
On a session running its own private bus (`SessionBusBootstrap`,
[ADR-0170](../adr/0170-survive-a-private-session-bus-for-dbus-units.md)),
the systemd user manager never observes this unit acquiring `BusName=`, so
every start on such a session waits out the full timeout before D-Bus
activation replaces it with an unmanaged process; 20s bounds that noisy wait
without removing the underlying `Type=dbus`/private-bus mismatch, which is
ADR-0170's decision, not this unit's alone. `main.cpp` maps
`ServiceStartStatus::NameAlreadyOwned` (a live sibling process already owns
`org.qindaqt.Audio1`) to exit 0 with a journal line, distinct from every
other non-`Started` status, which stays a hard failure (exit 1): a live
sibling is not this process's failure, and letting `Restart=on-failure`
retry a start that can only fail the same way again would loop until
`StartLimitBurst` for no reason. **The same `NameAlreadyOwned`-as-failure
gap and the missing `TimeoutStartSec` exist today in `qindaqt-power-service`,
`qindaqt-display-service` (its `main.cpp` is
`src/services/display_runtime/app/main.cpp`), `qindaqt-bluetooth-service`,
`qindaqt-network-service`, and `qindaqt-clipboard-host`** — confirmed by
reading each `main.cpp` and `.service.in`, not fixed here: those modules
have no lease in this wave's audio lane, and fixing five services' start
paths across four other modules is a separate, appropriately-scoped slice.

## Consumer boundary

This slice exports typed C++ protocol and client libraries only. The
[Audio settings route](../apps/audio-settings.md) owns the stable route ID
`audio` and projects the full typed device/stream model through the public
client into a closed Settings Center route; it is implemented and covered by
offscreen model, page, boundary, and Settings Center tests. Physical audio
hardware and nested session interaction remain unqualified. A future shell
applet receives only a narrow default-output facade plus `openAudioSettings()`;
it receives no raw snapshot, handle, graph, D-Bus object, service client, or
provider authority. The shell applet UI is not implemented here.

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
and rejects the old handle. It also covers the version 2 slice: a six-position
fixture publishes its channel map and per-channel volumes, `SetChannelVolumes`
changes per-channel levels, `CreateVirtualDevice` publishes a managed virtual
sink with virtual provenance, and `RemoveVirtualDevice` destroys it while
refusing non-managed nodes. It never contacts the user's session bus or host
audio graph.

That evidence does not qualify USB, HDMI, Bluetooth, jack sensing, multichannel
semantics on physical hardware, a physical microphone or speaker,
suspend/resume, hotplug churn, realtime scheduling, memory/CPU budgets, or
either future UI. Those remain hardware and integrated-session gates.

## Consumer-triggered recovery

The Qt client requests D-Bus activation when it starts and after owner loss,
with at most one activation request in flight and a one-second retry interval.
Activation resolves an exact unique owner before any snapshot request; controls
are never replayed. Private activation tests start only the public client, so
they cover a genuinely cold installed service instead of pre-starting it.


## Read-only graph diagnosis

The manual owning target qindaqt_audio_graph_observer requires an explicit
--observe argument and is never a CTest. It loads only the public mixer and
default-nodes API modules, reads an object manager, then calls the production
graph mapper and strict snapshot validator. It never constructs the backend
or worker, realises console state, captures audio, creates nodes, changes
defaults/levels, or writes metadata. Its client-local mixer scale matches the
service. A bounded settle window provides one observation, not graph stability.

Output includes fixed validator tokens, counts and indexed field-shape
summaries; it omits names, paths, media text and raw properties. Per-row checks
clear default/target references only in disposable diagnostic copies, so the
full snapshot validation remains the authority for graph-wide failures.
Missing API/core readiness is an error, not an empty healthy graph. Run it
only with the operator's selected runtime and a process timeout; it does not
select or start an upstream service. A manual observer does not qualify any
physical control operation.


## Channel projection and rejection diagnostics

The private wireplumber_volume collaborator owns mixer dictionary decoding
and projection. Numeric dictionary keys retain their channel positions,
including missing indices. The negotiated node map bounds the resulting
vector. A larger mixer vector contradicts the node layout, so channel values
become unknown (an empty vector) while aggregate controls remain available.
Publishing only a prefix would incorrectly authorize a partial channel write,
because mixer-api retains unspecified channels. Missing positions within a
non-contradictory map use the aggregate only when that aggregate is known. If a required position
and aggregate are both unknown, the published channel vector is empty.
The mapper never invents zero gain or compacts a later reading into an earlier
channel. Empty maps can retain a complete indexed mixer vector. Aggregate
known/mute/control flags keep their existing meaning.

Both device and stream projections obey the unchanged strict protocol
validator. A malformed backend snapshot still withdraws the entire graph and
makes pending actions uncertain. Its diagnostic now contains only the fixed
validator rejection token prefixed by Backend snapshot rejected; rejected
device names, media text and backend diagnostics are never copied.

The direct owning fixture invokes the production dictionary decoder and
projection, then the strict validator and real coordinator with a fake
backend. Device and stream rows cover contraction, expansion, unknown and
sparse indexed readings, unordered dictionaries and absent maps. The private
PipeWire runtime and latency fixtures validate every observed snapshot, not
only row presence. These gates do not establish physical-device usability.
