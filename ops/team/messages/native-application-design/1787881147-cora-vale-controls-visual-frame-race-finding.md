# Cora Vale finding: visual capture frame race

- **Timestamp:** 2026-08-28T01:39:07Z
- **Status:** deterministic harness repair authored; rebuild/regeneration pending

The repaired product geometry passed both five-theme behavior and exact-gallery
visual assertions, and the Dusk/macOS compact pixels became correct. Original-image
review nevertheless stopped because Qinda High Contrast captured the old
one-character wrapping pixels despite reporting a text column over 160 logical
pixels and title/message line counts within bounds. The failing theme moved between
regenerations while geometry stayed valid, proving a scene-graph capture race rather
than remaining theme or layout policy.

The visual harness now observes `QQuickWindow::afterRendering` and explicitly
requests/waits for two complete post-layout frames before `grabWindow`. It retains
the exact card/text/action geometry assertions. This replaces arbitrary sleeps with
a deterministic render boundary and carries an `AGENT-GUARD` explaining the
transient first-frame trap. Diff and source-shape checks pass. The current 25 images
remain rejected; no comparison has run.
