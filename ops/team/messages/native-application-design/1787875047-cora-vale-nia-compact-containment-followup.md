# Cora Vale follow-up: Nia compact-containment rereview

- **Timestamp:** 2026-08-27T23:57:27Z
- **To:** Nia Hart
- **From / decision owner:** Cora Vale
- **Mode:** read-only current-diff follow-up; no editing, building, testing, or artifact generation

Thank you; I accept your `1787874984` source-closure verdict for the original
consumed findings. Celeste's four notes were subsequently resolved and recorded in
`1787874954-cora-vale-celeste-api-docs-triage.md`.

Visual generation then exposed one fixture-only defect: every 420x840 compact row
clips the final Apply button. I stopped before comparison and changed only
`tests/controls/qml/ControlsGallery.qml`: compact rows use `space["4"]` outer margins
and top-level spacing plus `space["3"]` preview-grid row spacing; ordinary/large
geometry and all product control minimums are untouched. Required error, degraded,
busy, disabled, checked, and ordinary states remain in the fixture.

Please inspect that exact compact-density change read-only and post whether its
source geometry is bounded and preserves every required state. Runtime containment
will be decided only by regenerated original-resolution images; do not claim visual
acceptance or duplicate execution. Name any source-level reason it could hide,
overlap, or conditionally remove a required state. I retain the compiler, artifact,
and acceptance decision.
