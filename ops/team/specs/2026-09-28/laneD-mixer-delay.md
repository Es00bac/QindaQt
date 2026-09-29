# Lane D (Sonnet) — per-bus delay stage in the mixer (true output alignment)

Worker name `claude-mixer-delay`, voice seat `claude-helper-three`, speech name "mixer delay".
Worktree: `cd ~/work_SPaC3/container-wm && git worktree add ~/work_SPaC3/container-wm.worktrees/mixer-delay -b feature/mixer-bus-delay hub/main`
Push to `hub`. Amend ADR-0288 with a dated section (no new ADR).

Why: the ADR-0288 latency offset only changes *reported* latency; lining up two speakers or a VBAN
peer fed from the same mixer bus needs real delay. Owner approved a mixer delay stage.

1. **Model**: each output bus (A1…An, B…; see `console_model.cpp` bus creation ~176–184) gets
   `delayMs` (integer 0–1000, default 0), carried like the existing bus processing (bus EQ) through the
   console types (`src/services/audio_protocol/include/.../audio_types.h`, `audio_console.h`), the
   console store, presets, validation (`audio_validation.cpp`) and the Audio1 snapshot. Schema change
   is additive; bump the Audio1 schema version exactly as ADR-0288 did (13 → 14) and document it in
   a new `docs/wiki/reference/audio1-v14.md` (copy the v13 page's shape).
2. **Graph**: in `src/services/audio_service/src/wireplumber_processing.cpp`, the bus filter-chain
   graph (bus EQ nodes ~235–240) appends PipeWire's builtin delay node when `delayMs > 0`:
   `{ type = builtin name = delay label = delay config = { "max-delay" = 1.0 } control = { "Delay (s)" = <delayMs/1000> } }`
   linked after the last existing node (or as the only node). A bus with delay but no other
   processing must still get a filter-chain (`wireplumber_worker_processing.cpp` ~140). Verify the
   builtin's exact label/port/control names against the installed PipeWire 1.6.8
   (`/usr/share/doc/pipewire*` or `pw-cli`/`spa-json` in the module source under
   `/usr/lib64/pipewire-0.3/`); keep `max-delay` ≥ the 1 s bound.
3. **Settings**: a compact per-bus "Delay" ms spin box (QindaTK, like the latency control in
   `AudioLatencyControl.qml`) on the Mixer tab's bus strip (`AudioConsoleBus.qml`), with Reset and an
   accessible name; the operation goes through the existing bus-processing request path.
4. **Tests**: graph string for delay 0 (no node), 25 ms, 1000 ms (bounds), delay-only bus creates a
   chain; validation rejects <0 and >1000; console store/preset round trip; the Settings control
   (offscreen QML). Run the audio and settings-audio ctests (`ctest -R "^qindaqt\.(audio-|settings-audio-)"`).
Build on qinda `-j8 -l24`; commit, push, report commits, files, test counts. Keep tokens low.
