# Exact candidate handoff — customization-editor Release portability

- Posted: 2026-08-31T02:15:37-06:00 (unix 1788164137)
- Implementer: Margaret Hamilton
- Exact candidate: `fa22af5028e8dd4bfd9c0951cf742ea74d4b914f`
- Exact tree: `7732c39369f6212c0549220b9926273ba41b63f6`
- Sole parent / assigned base:
  `21c5a779c15b315c763c916c68b646cb4c19d8bb`
- Branch: `worker/release-customization-warning`
- Worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/release-customization-warning`
- Requested next action: different-worker review of this exact immutable commit

## Outcome

The strict GCC 15.3 Release portability failure is closed without suppression,
warning downgrade, compiler gate, public API change, or semantic weakening.
`steppedPanelTarget` now initializes the complete neighboring-panel append
target in one aggregate expression and explicitly copies the const value into
the outer optional. The anchor remains deliberately disengaged, preserving the
documented append-at-neighbor-end behavior. An `AGENT-GUARD` records why this
construction must not regress to an implicit move through inactive nested-
optional storage.

The focused keyboard-navigation row now compares the complete forward and
backward result values, including zone preservation and anchor disengagement,
and proves first/last-list and missing-panel failures.

## Exact changed paths

- `src/shell_customization_editor/src/keyboard_navigation.cpp`
- `tests/shell_customization_editor/tst_accessibility_navigation.cpp`

Diff size: two paths, 27 insertions, 9 deletions. The worktree is clean.

## Verification

- Exact pre-repair reproduction: GCC 15.3.0, Release `-O3 -DNDEBUG`, C++20,
  full `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
  -Werror`; failed at action 59/61 with two `maybe-uninitialized` diagnostics
  naming the inactive nested `QString` optional storage.
- Fresh strict Release standalone owning-module build: **85/85 actions**,
  exit 0.
- Fresh strict Debug standalone owning-module build: **85/85 actions**, exit 0.
- Complete `^qindaqt\\.customize-editor-` selector: **6/6 passed** in Release
  and **6/6 passed** in Debug, each with `--no-tests=error`.
- Direct repaired method `panelStepsAppendAtTheNeighborEnd`: **3/3 QtTest
  cases passed** in Release and **3/3 passed** in Debug.
- Exact-commit incremental build/test rerun: no rebuild work; both 6/6
  selectors and both direct 3/3 rows pass.
- `./tools/validate-docs`: **110 Markdown documents plus navigation**, exit 0.
- isolated MkDocs 1.6.1 `build --strict`: exit 0.
- `./tools/check-source-shape`: **1,662 source files**, exit 0. Its three
  threshold warnings are pre-existing and outside this candidate's paths.
- `git diff HEAD^ HEAD --check`: exit 0.
- Provenance: candidate has the assigned base as sole parent and merge base;
  current public `main` was still that exact base. Read-only `git merge-tree
  --write-tree main HEAD` produced the candidate tree exactly.
- Final `git status --porcelain=v1`: empty.

## Bounded caveat

This candidate proves and repairs the reported customization-editor compiler
gate. It does not claim a complete default whole-tree Release build, change any
documented interaction contract, or qualify presentation, nested sessions, or
hardware. The normative customization-editor documentation remains accurate
and therefore receives no prose-only churn.
