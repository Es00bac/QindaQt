# Audio1 exact review: P2 rapid start/stop leaks worker resources

Reviewer: Codex Audio1 exact reviewer  
Candidate: `6926aad9c93a757d06f32835db9962007ce2b195`  
Decision impact: blocking P2

The production WirePlumber backend does not make rapid start/stop a complete
async-shutdown barrier:

- `src/services/audio_service/src/wireplumber_worker.cpp:167-200` starts two
  `wp_core_load_component()` operations with raw `this` callback data, creates
  the core/manager, and starts connection work.
- `wireplumber_worker.cpp:86-114,398-419` stops by cleaning the core and quitting
  as soon as operation-sync callbacks are drained. Component-load/connect work
  is neither cancellable nor counted in the drain condition.
- `wireplumber_worker.cpp:202-221` assumes a live worker and current core when a
  component callback arrives; there is no callback-lifetime token or shutdown
  ownership. The public backend contract says `stop()` synchronously joins the
  worker and releases its GLib context.

Reviewer-only ASan+UBSan probe (ignored `build/` scratch, production libraries,
private empty XDG/PipeWire runtime, invalid private session-bus address, no host
audio/session state) called `WirePlumberAudioBackend::start(); stop();` 50 times
without pumping Qt. It measured `/proc/self/fd` in the same process:

```
RAPID_START_STOP=50/50 FD_BEFORE=5 FD_AFTER=506 FD_DELTA=501
```

The probe intentionally returned 1 when the delta exceeded five. A 250-cycle
version aborted before completion with GLib `Creating pipes for GWakeup: Too
many open files` (exit 133). Logs and source are under reviewer-owned ignored
paths:

- `build/audio1-review-probes-sanitize/rapid-stop50.log`
- `build/audio1-review-probes-sanitize/rapid-stop.log`
- `build/audio1-review-probes-sanitize/rapid_stop_probe.cpp`

This is deterministic resource exhaustion in the documented lifecycle, and it
also leaves raw async callback-data ownership unproved after destruction.

Required repair: give every component-load/connect async path explicit
cancellation/generation ownership, keep the private GLib loop/context alive
until all callbacks release their state, and make `stop()` a true resource and
callback barrier. Add an ASan+UBSan rapid start/stop regression that asserts a
bounded FD delta and clean destruction, including failure paths.
