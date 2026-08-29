# Audio1 repaired-candidate finding: stopped disconnect idle poisons later recovery

- Reviewer: Codex Audio1 exact reviewer (different worker from implementer)
- Exact candidate: `e6423be9040edb5f28dc2f3d8d38665b7ad06030`
- Severity: **P2 — blocking lifecycle/recovery defect**
- Candidate state: detached exact HEAD; tracked worktree clean; no candidate edits.

## Finding

An explicit backend stop can supersede the deferred PipeWire disconnect reset while leaving `m_resetScheduled` latched `true`. A later start reuses the same worker without clearing that latch, so the next real PipeWire loss returns early and is never projected or reconnected.

- `src/services/audio_service/src/wireplumber_worker.cpp:406-423` sets `m_resetScheduled = true` inside the `disconnected` signal and deliberately defers `handleDisconnected()` to an idle.
- Only `handleDisconnected()` clears the latch at `wireplumber_worker.cpp:425-433`.
- The higher-priority explicit stop path at `wireplumber_worker.cpp:100-128` can clean the core and quit the context before that idle runs, but does not clear the latch.
- Neither the reused-run setup at `wireplumber_worker.cpp:152-197` nor `cleanupCore()` restores the latch. On the later loss, line 410 therefore drops the event.

This violates the advertised stopped/restart generation and deterministic PipeWire-loss recovery boundary: the run generation changes correctly, but the new run's authority-loss event can be permanently suppressed.

## Reproduction

I compiled a reviewer-only white-box scheduler probe in ignored `build/` output; it does not modify or link different product source. It starts exact candidate libraries against disposable private PipeWire runtimes and installs a second `WpCore::disconnected` observer. Because the production handler was registered first, that observer pauses immediately after production has set/scheduled its reset. The Qt creator thread then calls the normal public `backend.stop()`; releasing the observer makes the existing `G_PRIORITY_HIGH` stop run before the lower-priority reset idle. After starting a distinct generation/private PipeWire process, the probe kills PipeWire again and waits for the required `pipewire-unavailable` publication.

Independent repeated evidence in `build/audio1-reset-idle-race-probe-debug-repeat.log`:

```text
first_generation=1 second_generation=3 reset_still_scheduled=1 second_loss_observed=0
iteration=1 status=8
first_generation=1 second_generation=3 reset_still_scheduled=1 second_loss_observed=0
iteration=2 status=8
reproduced_expected_failure=2/2
```

The same result occurred in the initial run. Every disposable PipeWire child was terminated and reaped; post-probe exact-process checks were empty. A normal unforced 20-cycle loss/stop/restart probe passed 20/20, which confirms this is an ordering defect rather than universal restart failure; the scheduled timing remains valid and must be deterministic.

## Required repair/recheck

Make disconnect-reset state run-scoped and ensure explicit cleanup/stop cannot carry a stale scheduled latch into a new worker run. Add a deterministic regression that orders `disconnected` scheduling before explicit stop, then verifies a later generation still projects a second PipeWire loss, advances authority, and reconnects. Recheck component-load/sync drains, rapid 250+ stop cycles, and ASan+UBSan after repair.

Exact `e6423be9040edb5f28dc2f3d8d38665b7ad06030` cannot be accepted while this P2 remains.
