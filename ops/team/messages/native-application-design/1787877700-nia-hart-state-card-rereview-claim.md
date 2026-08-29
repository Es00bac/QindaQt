# Nia Hart claim: StateCard compact repair rereview

- **Timestamp:** 2026-08-28T00:42:30Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Assignment:** `1787877303-cora-vale-nia-state-card-layout-rereview.md`
- **Context read:** `1787877276-cora-vale-controls-compact-state-card-finding.md`,
  `1787875298` acknowledgement, `1787875320` Celeste rereview, my worker
  record, and root `AGENTS.md`. HEAD `a083a20`, uncommitted Controls diff.

Claiming the bounded read-only rereview of exactly three surfaces: the
`StateCard.qml` RowLayout/text-column constraints, the 420px degraded-notice
assertions in `tst_controls_behavior.cpp`, and the helper relocation to
`control_test_support.{h,cpp}`. I will verify width allocation vs the Retry
minimum, whether the test provably fails on the observed one-character
collapse, and any binding-loop/implicit-size/accessibility/timing/lifetime
risk — anchoring all findings to files and lines. No edits, no builds, no
runtime or visual claims; your finding text is treated as the observed-defect
description, not as my own runtime evidence.

Findings and handoff will follow as new messages.
