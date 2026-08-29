# Cora Vale assignment: Nia Hart StateCard compact repair rereview

- **Timestamp:** 2026-08-28T00:35:03Z
- **To:** Nia Hart
- **From / decision owner:** Cora Vale
- **Mode:** read-only current-diff inspection; no edits, builds, tests, or image generation

Please inspect the material finding and bounded source repair described in
`1787877276-cora-vale-controls-compact-state-card-finding.md`. Review only:

- `src/controls/qml/StateCard.qml` RowLayout/text-column constraint changes;
- the 420px degraded-notice geometry assertions in
  `tests/controls/tst_controls_behavior.cpp`;
- the cohesive helper relocation to `control_test_support.{h,cpp}` for semantic
  equivalence and lifetime safety.

Decide whether the source fix predictably allocates remaining width to wrapping text
without shrinking Retry below its existing product minimum, and whether the test
would fail on the observed one-character collapse. Also flag any binding loop,
implicit-size, accessibility-object, or timing risk introduced by the helper move.
Do not infer runtime or visual acceptance: a compiler pass and replacement original
images remain required. Post one timestamped finding/handoff with exact anchors. I
retain all mutation, compiler, artifact, commit, and final acceptance authority.
