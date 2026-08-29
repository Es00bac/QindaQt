Audio applet A1 fail-closed projection contract fix handoff (Rune Mercer)

Candidate: exact commit `14abe57028227ac5f2d152bfe062a01fdafaded1`
("Fix Audio applet fail-closed projection contract for Unavailable/Loading phases") on branch
`worker/audio-applet-a1-repair-rune` in worktree
`/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-a1-repair-rune`. Tree
`5f7be43d017e6771d3ac18848122253029d75d10`, parent
`84712fa7c2a4542bf2c62ba98b2fc5b5f32b73f4` (prior pointer syntax fix).
Working tree clean at handoff.

Changed-path manifest (fail-closed contract enforcement only):

- `src/shell/audio_applet/audio_applet_model.cpp` (lines 113-119) — Added early
  return contract check for Unavailable and Loading phases in project() method.
  When phase is Unavailable or Loading, return model immediately with no rows
  populated (fail-closed). This preserves the contract: Ready and Degraded phases
  show bounded rows within window; Unavailable and Loading phases expose no rows,
  only phase/reason text.

Verification evidence:

- Confined external build at /mnt/d/QindaQt/builds/audio-applet-a1-repair-rune:
  ✓ Configuration successful with -DCMAKE_AUTOMOC_PATH_PREFIX=ON
  ✓ 27 targets built successfully (all audio protocol/client/applet targets)
- Test execution with fail-closed fix:
  * qindaqt_audio_applet_model_tests: ✓ 12 passed, 0 failed (11ms)
  * qindaqt_audio_applet_controller_tests: ✓ 15 passed, 0 failed (55ms)
    - Previously failing test now PASSES: unavailableSnapshotFailsClosedWithReason()
- `python3 tools/check-source-shape --root . --warnings-as-errors`: ✓ Passed
  (1013 files checked)
- `python3 tools/validate-docs --root .`: ✓ Passed (64 Markdown documents)
- `git diff --check`: ✓ Passed (no whitespace violations)
- Working tree clean (all .omc/ and temporary outputs removed)

Bounded caveats: QML is unlinted and unrendered in this slice; default
selection and stream moves intentionally excluded; retry-on-degraded absent.

Requested next action: Astra Quill performs exact rereview of this descendant
commit `14abe57...` verifying that the fail-closed contract is correctly
enforced (Unavailable/Loading phases clear rows; Ready/Degraded show bounded
rows) and that all 15 controller tests pass.

— Rune Mercer, 2026-08-28T13:05:00Z
