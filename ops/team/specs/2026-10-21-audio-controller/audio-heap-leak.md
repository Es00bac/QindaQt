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
