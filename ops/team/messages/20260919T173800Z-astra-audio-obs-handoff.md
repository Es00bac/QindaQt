# Audio/OBS code-ready handoff

- Exact candidate: `74553748eadf83b01843d88506c71f54f02f9c54`.
- Diagnostic parent: `433069bd`, recorded before production source edits.
- Branch/worktree: `agent/audio-obs-20260919`, `.cache/audio-obs-20260919`.
- Base: `01e919f6bff7c3c111778f814aaec8288ddd7777`.
- Request: root review/integrate the candidate history, then coordinate the first integrated compilation and focused runtime checks. Worker continues the root-requested read-only review of desktop `ecb67171` and toolkit `d59b080`.

## Causal corrections

- Preserve OBS authentication/protocol close codes, keep paused recordings and reconnecting streams active, discard stopped/replaced connection state and frames, and clear stale run counters.
- Retain compact named OBS switches with authoritative checked bindings, per-action pending admission and confirmation feedback, a readable horizontal chip, direct Streaming settings action, and a native popup implicit-height cap. Granted read access survives a denied control capability.
- Give Settings Streaming the same checked-state repair, one outstanding output/scene command, readable connection diagnosis, and correct empty-mapping/Audio1-loss distinction.
- Keep Settings console fader delegates alive, restore published position binding, coalesce the latest gesture at 40 ms while the client is free, and track every console request through existing failure/uncertainty feedback.
- Use the existing PipeWire destroy listener for virtual-bus modules, avoiding a retained dead pointer and duplicate teardown.
- Drop the OBS bridge's retained Audio1 owner wiring and mapping when the public client has no snapshot, including restored sources after a collection change. Integration review subsequently corrected capture retirement to preserve the managed OBS source objects and silence only their private capture children.

## Paths

`data/applets/obs.json`; `src/services/audio_service/src/wireplumber_worker_{p.h,modules.cpp,endpoints.cpp}`; OBS transport/client headers and implementations in `src/services/obs_client`; `src/obs/module/bridge_controller.cpp`; Settings Audio model/header/console projection and `AudioConsole{Section,Fader}.qml`; Settings Streaming model/header and `Streaming{Page,OutputsSection}.qml`; OBS applet controller/header/presentation, all three OBS applet QML files; `src/shell/runtime/obsappletcomposition.cpp`; five owning audio/OBS/Streaming wiki pages; this worker's durable record.

## Evidence and limits

`git diff --check` returned exit 0. No compilation, configure check, runtime test, new test, live session mutation, package install or deployment ran. Strict MkDocs/link and focused module gates remain with the coordinated integrated check. Source review kept the Audio model's shared completion callback cohesive at 501 nonblank lines; its existing console projection shrank to 576, and no changed production file exceeds 600.

ADR-0208 still activates every bus and raw strip as an OBS mixer source: a signal can arrive through both, and raw-strip captures precede the rack/send gains. No source-selection/default policy change was made. Count-based delegates repair normal reprojection but do not guarantee arbitrary same-count console-id replacement. Raw lingering-sink proxy recovery and physical audio hardware remain unqualified.

Final focused check after the integrated build: drag a console fader through several authoritative snapshots; confirm the final level and a refused console operation's feedback; open the OBS popup with a long scene list; observe wrong-password, paused-recording, reconnecting-stream and refused-output states; replace the Audio1 owner while OBS is running and check bridge source identities/scene/filter/mixer associations survive, with capture silenced until the next authoritative snapshot retargets them. Do not describe these checks as passed until run.
