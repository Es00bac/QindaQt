# Mira Quill — exact production-surface candidate handoff

- 2026-08-28T03:49:48Z — Candidate commit:
  `6b57ef3c34d12967df837333a6cfb0ab1a7f5acd`, directly parented by exact public
  base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`. It is one non-amended commit on
  `worker/shell-surface-repair`; `git show --check` passes and the worktree is
  clean.

## Outcome and changed paths

The candidate adds a schema-valid, never-hidden two-panel proof profile and
passes its exact ID through the nested launcher and surface probe. This makes
the initial LayerShell publication/work-area proof deterministic without
changing production automatic-hide behavior.

- `tests/session/fixtures/shell_surface_profiles/qindaqt-surface-proof.json`
- `tests/session/CMakeLists.txt`
- `tests/session/test_shell_surface_nested.py`
- `tests/session/shellsurfaceprobe.cpp`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/panel-surfaces.md`

No Controls or production shell-policy path changed.

## Acceptance evidence

- Serial target build of `qindaqt-shell-surface-session-probe`: exit 0,
  remaining 63/63 steps after the environment-only `/tmp` capacity stop was
  redirected to ignored worktree compiler temp.
- Private nested production rows selected by
  `^shell\.production-surface\.(1080p|wuxga|1440p)$`: 3/3 passed, exit 0, in
  4.73 s. Each row proved two unambiguous mapped layer-2 roles, causal
  configure/ack/attach/commit, 84-pixel work-area reservation, and teardown
  restoration at 1920x1080, 1920x1200, and 2560x1440.
- Focused shell targets: 142/142 serial build steps, exit 0. Focused CTest
  selector covering layout, customization, visibility, orchestration, surface,
  protocol trace, runtime options, and applet resolution: 25/25 passed, exit 0.
- Serial `qindaqt-shell-preview`, `qindaqt-shell_qmllint`, and
  `qindaqt-shell-preview_qmllint`: exit 0. Diagnostics were existing warnings
  in unchanged QML paths.
- Shell-module staging installed exactly `qindaqt-shell` and
  `qindaqt-shell-preview`; both executable, no unresolved `ldd` dependency,
  both list-only paths loaded `qindaqt-surface-proof`, and the test fixture is
  absent from the install script/stage.
- Final fixture JSON/Python validation, `git diff --check`, source-shape
  (831 files, 0 allowlisted skips), and documentation validation (47 Markdown
  documents plus navigation) passed.
- Every short private runtime root was empty, removed by exact path, and verified
  absent. No candidate-owned compiler, CTest, KWin, Xwayland, or probe process
  survives. No host display/input/audio/session setting or physical output was
  used or changed.

## Bounded caveats and next action

`mkdocs build --strict` could not run because MkDocs is unavailable; the
repository documentation validator passed. Live automatic-hide transitions,
animation, partial panels, and heterogeneous multi-output remain separate
documented boundaries. The sole compiler/private-runtime lane is explicitly
released. Please assign an independent reviewer to attack the immutable exact
commit, then integrate only after that review passes.
