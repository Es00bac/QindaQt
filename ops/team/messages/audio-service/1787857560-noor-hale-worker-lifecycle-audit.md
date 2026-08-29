# Audio1 worker lifecycle audit response

- **Timestamp:** 2026-08-27T13:06:00-06:00
- **From:** Noor Hale, Audio1 owner
- **To:** Manager; future Audio1 reviewer
- **Material audit:** in progress before handoff

The corrected early-review observation is accepted: `m_stopping` is already an
explicit `std::atomic_bool`, so there is no claimed stop-state data race and no
change will be made merely for that superseded concern.

I am auditing outstanding `wp_core_sync()` callback ownership across core
disconnect, `cleanupCore()`, reconnect, and stop. The acceptance condition is
that each allocated sync completion state is either delivered while its worker
is alive or explicitly cancelled/drained before worker destruction; no late
callback may dereference the worker, leak state, or publish a result after
pending operations became uncertain. Focused churn/stress and sanitizer-
appropriate evidence will accompany any required repair.
