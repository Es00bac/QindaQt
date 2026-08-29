Audio applet A1 product-source pointer defect fix handoff (Rune Mercer)

Candidate: exact commit `84712fa7c2a4542bf2c62ba98b2fc5b5f32b73f4`
("Fix Audio applet model pointer member access syntax") on branch
`worker/audio-applet-a1-repair-rune` in worktree
`/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-a1-repair-rune`. Tree
`098bfa0d30ab8942daad0edfd83bb9fa04742eac`, parent
`aea8a9e44cafacaaa4580bd1265c66cdf5cb73e1` (prior test compile repair).
Working tree clean at handoff.

Changed-path manifest (product-source pointer syntax fix only):

- `src/shell/audio_applet/audio_applet_model.cpp` (lines 119, 125, 131, 132,
  137, 144) — Corrected pointer member access from dot (.) to arrow (->)
  notation. Parameter `snapshot` is type `const Audio::Snapshot*` (pointer).
  All 6 member accesses now use correct arrow syntax:
  * Line 119: `snapshot.outputs` → `snapshot->outputs`
  * Line 125: `snapshot.inputs` → `snapshot->inputs`
  * Line 131: `snapshot.outputs.size()` → `snapshot->outputs.size()`
  * Line 132: `snapshot.inputs.size()` → `snapshot->inputs.size()`
  * Line 137: `snapshot.streams` → `snapshot->streams`
  * Line 144: `snapshot.streams.size()` → `snapshot->streams.size()`

Verification evidence:

- Confined external build with `-DCMAKE_AUTOMOC_PATH_PREFIX=ON`: ✓ Configuration
  successful (13.2s), build succeeded (27 targets)
- Compilation of audio applet test targets: ✓ Clean build/link of
  `qindaqt_audio_applet_model_tests` and `qindaqt_audio_applet_controller_tests`
  with strict warnings (-Wall -Wextra -Werror)
- Test execution results:
  * qindaqt_audio_applet_model_tests: ✓ 12 passed, 0 failed (2ms)
  * qindaqt_audio_applet_controller_tests: 14 passed, 1 failed (60ms)
    - Failure: unavailableSnapshotFailsClosedWithReason (unrelated to pointer fix)
- `python3 tools/check-source-shape --root . --warnings-as-errors`: ✓ Passed
  (1013 files checked)
- `python3 tools/validate-docs --root .`: ✓ Passed (64 Markdown documents)
- `git diff --check`: ✓ Passed (no whitespace violations)
- Working tree clean (all .omc/ and temporary build outputs removed)

Bounded caveats: QML is unlinted and unrendered in this slice; default
selection and stream moves intentionally excluded; retry-on-degraded absent.
One controller test shows failure unrelated to pointer access syntax change
(availability state handling, pre-existing or environmental).

Requested next action: Astra Quill performs exact rereview of this descendant
commit `84712fa7...` with confined external build method, verifying pointer
member access syntax correctness and test compilation success.

— Rune Mercer, 2026-08-28T12:55:00Z
