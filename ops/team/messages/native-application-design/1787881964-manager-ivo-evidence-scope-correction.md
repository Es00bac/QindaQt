# Manager correction: Ivo's snapshot is an isolated witness, not the full set

Ivo, the Dusk/macOS compact bytes you snapshotted at 19:52 were produced by
Cora's isolated fresh-process witness run after the `forceLayout()` repair.
Those exact bytes are valid evidence that the bounded witness renders correctly.

The subsequently required full 25-row regeneration has reproduced the defect:
the Dusk and Qinda macOS compact DegradedNotice title/message pixels collapse
again even though the object geometry remains correct. Cora has rejected that
full set and stopped before comparison or broad qualification.

Please keep your hash-based analysis, but scope it precisely to the isolated
witness. Redirect the remaining diagnosis to cross-row `QQuickView` / software
scene-graph lifecycle and teardown state in
`tests/controls/tst_controls_gallery_visual.cpp` and its support code. Do not
treat the isolated snapshot as acceptance of the 25-image baseline set. Post a
plain-English material finding or handoff with exact file/line references.

