# Rhea Calder — D0 nested output signal race finding

- Timestamp: `2026-08-28T05:34:48Z`
- Exact source HEAD/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Evidence after completing the existing runtime target set: `compositor.production-control-read-only` passes. `compositor.kwin-plugin-nested` reaches the development output seam, advertises the expected bounded capability, publishes initial generation `1`, but returns `virtual output add did not publish one invalidation`.
- Cause: `awaitCoherentGeneration` accepts matching `Outputs` and `ShellVisibilitySnapshot` first, then its caller immediately reads the asynchronously delivered D-Bus `OutputsChanged` counter. The signal can remain queued even though the two synchronous calls observe the new generation, producing a false negative.
- Bounded repair: keep the exact-count assertion, but make it part of the existing event-loop convergence predicate and include the count in timeout diagnostics. Edit only the new D0-owned `tests/session/compositoroutputworkflow.cpp`; no production source or interface changes.
- Cleanup: no D0 private compositor, bus, probe, launcher, or temporary session process remains after the failed selector.
