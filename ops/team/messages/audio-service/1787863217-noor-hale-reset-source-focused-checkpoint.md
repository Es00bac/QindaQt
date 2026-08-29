# Audio1 reset-source repair focused checkpoint

The lifecycle fix is implemented without timing assumptions: the old `m_resetScheduled` boolean is removed. The GLib worker now retains the actual disconnect-reset idle source, tags its payload with the worker-run token, destroys/releases the source synchronously from cleanup/stop, releases ownership on normal dispatch, and rejects any stale-run dispatch before it can mutate epoch/core state. Private test-only lifecycle hooks provide deterministic scheduling observation without changing the public backend/client contracts.

New `qindaqt.audio-wireplumber-reset-lifecycle` forces the exact finding order: after the production disconnect handler attaches the idle, it pauses the worker; another thread queues normal high-priority stop; only then does the test release the signal turn. It performs two complete private PipeWire start/loss/superseding-stop/restart/second-loss cycles, requires both later losses to advance epoch and publish `pipewire-unavailable`, and bounds file descriptors. Direct run passed in 0.22 s; focused Debug Audio1 passed 7/7; deterministic repetition passed 20/20. The existing 250-cycle production backend and full isolated runtime also pass in the focused registry. Source shape passes 748 files/zero skips; changed worker is 484 nonblank lines. Whitespace is clean.

Per resource coordination, all remaining builds are capped at parallel 2 and sequenced rather than overlapped. Next: ASan+UBSan focused 7/7 including both lifecycle tests, then Release/Debug broad 90-test registries, private lifecycle repetition, docs/install/cleanup, and a new non-amended commit on `e6423be`.

— Noor Hale, Audio1 implementer
