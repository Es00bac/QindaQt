# Audio1 backend implementation claim

- **Timestamp:** 2026-08-27T12:13:24-06:00
- **Worker:** Noor Hale, Audio Platform Engineer
- **Outcome:** a D-Bus-activated `qindaqt-audio-service` owning
  `org.qindaqt.Audio1`, with bounded typed output/input/application-stream
  snapshots, default/level/mute state, owner-bound asynchronous client, and
  `SetDefault`, `SetVolume`, `SetMute`, and `MoveStream` against the running
  WirePlumber/PipeWire graph
- **Exact base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Branch:** `worker/audio1-service`
- **Worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/audio1-service`

## Path ownership

Primary ownership is new `src/services/audio_protocol/**`,
`src/services/audio_client/**`, `src/services/audio_service/**`, matching
`tests/services/audio_{protocol,client,service}/**`,
`docs/wiki/architecture/audio-service.md`, and
`docs/wiki/reference/audio1-v1.md`. The assignment also authorizes one new ADR
for the Qt/GLib worker/process boundary and minimal additive integration edits
to source/test CMake, MkDocs, module boundaries, overview, roadmap, testing,
and install/activation registries.

Settings1/settings app, shell QML/runtime, themes/profiles, design tokens,
applets, Display1, every other provider, user/system PipeWire configuration,
and the host desktop/audio/session bus are explicitly out of scope.

## Contract and evidence bar

- WirePlumber remains the sole policy manager; this service observes and
  requests operations through libwireplumber 0.5 and never transports samples
  or substitutes `wpctl` parsing.
- Handles combine WirePlumber object serial with the service epoch. Provider or
  service owner replacement invalidates all handles, advances lineage, makes
  in-flight operations uncertain, and never triggers automatic replay.
- Qt main-thread publication receives only immutable bounded values from one
  GLib `GMainContext` worker that exclusively owns every `WpCore`, object
  manager, and `GObject` handle.
- Protocol, client transport state, backend abstraction, WirePlumber adapter,
  operation coordinator, service object, and process remain separate.
- Acceptance requires hostile codec tests, fake-backend model/operation and
  stale-handle tests, exact-owner client replacement/timeout/no-replay tests,
  private session-D-Bus activation, an isolated disposable PipeWire +
  WirePlumber runtime including restart where supported, Debug/Release builds
  and registries, useful sanitizer coverage, strict docs/link/source-shape/
  whitespace checks, staged install, and staged activation.

Shared registries are collision points with active Settings1 and design-token
work. I will make only additive edits on this assigned base and identify each
one in the candidate handoff so the manager can resolve integration ordering.
No hardware, UI, host graph, microphone, speaker, input, desktop, or system
configuration qualification will be claimed.
