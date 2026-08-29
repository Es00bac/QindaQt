# Audio1 exact review — stopped/restarted snapshot lineage finding

- Reviewer: `codex-audio1-exact-reviewer`
- Timestamp: `2026-08-27T13:34:54-06:00`
- Exact candidate: `6926aad9c93a757d06f32835db9962007ce2b195`
- Classification: **P2 — blocking asynchronous shutdown/restart lineage violation**

## Finding

Production `WirePlumberAudioBackend::stop()` joins the GLib worker, but it does not cancel value callbacks that the worker already queued onto the backend's Qt event queue. `AudioOperationCoordinator::acceptSnapshot()` has no running/generation guard and no monotonic backend-lineage check, so it accepts and publishes those late values after coordinator stop. If the same public coordinator/service is restarted before the queued callback drains, that earlier-generation snapshot becomes current authority in the new run.

Source evidence:

- `src/services/audio_service/src/wireplumber_audio_backend.cpp:20-37` captures only `this` and queues every snapshot/outcome with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; no backend run generation is captured or checked.
- `src/services/audio_service/src/audio_operation_coordinator.cpp:79-87` sets `m_running=false`, synchronously stops/joins the worker, and makes operations uncertain, but does not invalidate queued snapshot delivery.
- `src/services/audio_service/src/audio_operation_coordinator.cpp:236-269` accepts every structurally valid snapshot even while stopped. It also accepts same-epoch revision regression and arbitrary earlier-epoch values; `m_running` is not consulted.
- The public header at `src/services/audio_service/include/qindaqt/services/audio_service/audio_operation_coordinator.h:19-22` assigns authoritative publication/lineage to the coordinator and documents stop behavior. The implementation permits publication after that boundary is stopped.

Independent real-backend reproduction used a reviewer-only build artifact, not candidate edits. It set `XDG_RUNTIME_DIR` and `PIPEWIRE_RUNTIME_DIR` to an empty temporary private directory, set `DBUS_SESSION_BUS_ADDRESS` to an unreachable private path, constructed the production `WirePlumberAudioBackend` plus coordinator, called `start(); stop();`, then drained the Qt queue for 2 ms. Result:

```text
POST_STOP_PUBLICATION_ITERATIONS=100/100
```

No host PipeWire graph or user session bus was reachable. The probe source/log are under ignored `build/audio1-review-probes/`; the product tree remains untouched.

Impact: stop is not a publication barrier, a stopped service object's snapshot changes after teardown, and public stop/start reuse can expose stale prior-run authority until a fresh backend value arrives. Late old-generation snapshots can also roll back revision/epoch because there is no monotonic/fresh-generation check. This contradicts the documented deterministic shutdown/restart and stale-lineage boundary.

Repair must generation-fence queued backend-to-Qt deliveries (or cancel/drain them), reject snapshot/outcome callbacks outside the active run, and enforce allowed epoch/revision progression before authoritative publication. Add a real queued-callback stop/start regression proving zero post-stop publication and no stale injection into a new run. The reviewer is continuing the exact-candidate audit; no candidate repair was made here.
