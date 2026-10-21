# Lane: Audio1 service heap leak on graph churn (2026-10-21)

Branch `fix/audio-heap-leak-20261021` (worktree
`~/work_SPaC3/container-wm.worktrees/audio-heap-leak-20261021`), base hub main
`814f5e518`. Owner: Claude (Opus worker). No install or emerge until the package-once pass.

## Evidence (qinda-top, installed r42 build, 2026-10-21)

- `qindaqt-audio-service` anonymous heap grows about 310 kB each time a client
  playback stream appears and disappears. Thirty
  `timeout 1 pw-cat -p --volume 0 --format s16 --rate 48000 --channels 2 --raw - </dev/zero`
  runs: RSS+swap 63.3 → 72.8 → 82.1 MB over two rounds. No new mappings, fds or threads.
- Previous 46 h session: 53.5 MB peak + 191.7 MB swap peak, 2 h 10 min CPU.
- Not the cause: 1,500 `GetSnapshot` calls (zero growth), meter-stream recreation
  (node ids unchanged), idle time.

## Task

Reproduce in the private PipeWire/WirePlumber runtime harness. Find and fix the
leaking allocation on the graph-change path. Add a bounded-growth regression
test and run the audio_service, audio_client and audio_protocol tests. Push the branch to hub only.

## Related findings (not in this lane)

- Installed `media-plugins/obs-qindaqt-0.1.0_pre20260921-r2` pins QindaQt
  `4f2eec0f2` (Audio1 schema 11). The service is schema 14, so OBS rejects every
  snapshot and polls `GetSnapshot` every 200 ms forever (pre-ADR-0394 client).
  Bump the ebuild to current source in the package-once pass.
- The service keeps 11 meter streams, each with its own `pw_context` via
  `pw_stream_new_simple`. Non-passive bus meters keep ALSA devices awake (~4% CPU).
  This is ADR-0394's deferred `AcquireMeters`/`ReleaseMeters` contract.

## Outcome (2026-10-21, Claude)

Five commits on the original base, then a merge of hub main `89ffa5022` (the
hub refuses non-fast-forward updates, so the lane is merged, not rebased).
Nothing installed.

1. *Stop refused strip racks leaking Audio1's heap on every graph event.* Root
   cause: `wireplumber_processing.cpp` linked builtin biquads as
   `Input`/`Output` (they are `In`/`Out`), so every EQ rack was refused, and the
   worker retried the refused filter-chain on every rebuild. PipeWire 1.6.8
   `filter-graph.c` `impl_init` leaks the loaded nodes on each refusal
   (48–110 kB). Live config confirmed: `strip.virtual.1` EQ on, low shelf
   ~+15 dB, which becomes audible after install. Churn row: old 624–638 kB per
   stream, fixed under 0.5 kB.
2. *Keep meter readings out of Audio1 snapshot identity.* AudioClient treated a
   same-revision refetch with moved meters as a contradiction
   (Unavailable/malformed-snapshot, backoff to 10 s).
3. *Install retry_backoff.h with the AudioClient SDK.* Fixes
   `qindaqt.audio-installed-sdk-closure`; the OBS plugin rebuild needs it.
4. *Publish graph snapshots that arrive after console changes.* The coordinator
   fenced backend snapshots against its own console-bumped revision, so the next
   N graph changes after N console operations were dropped.
5. *Attempt refused console loopbacks and VBAN streams once per declaration.*
   Busy-port or refused-route VBAN streams were retried and logged on every
   graph event; refusals are now logged once.

Left open: a virtual strip sink that PipeWire destroys is not recreated until
reconnect (the raw-proxy record blocks it); doing it needs a proxy `removed`
listener plus a deferred rebuild. Upstream filter-graph refusal leak remains.
Parallel ctest runs flake single applet QML rows (pass alone and serially).
