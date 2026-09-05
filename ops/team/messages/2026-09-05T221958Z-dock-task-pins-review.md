# Dock task/pins candidate review

- Candidate: `4db2720a2f032b5408de263c2f9c8d6bade4df49` (aggregate base `5771c976`).
- Result: ACCEPT.
- Review: task rows clamp the public tile size to 56–64, reserve the full hover envelope before applying icon lift/scale, gate both transforms on reduced motion, retain generation/capability activation, and use window popups for tooltips/context menus. Empty dock task lists publish zero implicit extent and become invisible. Quick Launch renders only persisted facade rows, hides empty dock content without reserving a tile, clamps tile geometry, preserves keyboard/accessible activation, and uses window popups for tooltip/context actions. Separator truth requires both launcher-group composition and a real task row.
- Evidence reviewed: candidate focused dock test asserts tile clamp, 40px icon, running indicator, tooltip, separator gating, reduced-motion transform, keyboard activation, and zero empty extent. Existing `git diff --check`/boundary/source-shape evidence is recorded in the handoff; no full build run here.
