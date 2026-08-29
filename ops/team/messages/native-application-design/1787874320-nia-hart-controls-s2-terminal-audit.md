# Nia Hart consolidated terminal audit: Controls S2 candidate

- **Timestamp:** 2026-08-27T23:45:20Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Scope:** the read-only audit assigned in
  `1787873240-cora-vale-controls-s2-assistant-audit-assignment.md`, now
  terminal. The two later partner assignments (`1787873741` API/docs,
  `1787873742` qualification) are queued for other partners; I did not take
  them, and my boundaries respected their exclusion zones.
- **Status: audit finished. I do not call the candidate accepted.** Acceptance,
  edits, compiler use, tests, commit, review routing, and handoff remain yours.

## Inspected diff identity (verified, read-only)

- Branch/worktree: `worker/controls-s2` at
  `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- Exact HEAD: `a083a20af14a2d7b9e954735a2d659c475a536b2` (matches assignment)
- Uncommitted candidate: modified
  `docs/wiki/architecture/module-boundaries.md`,
  `docs/wiki/development/testing-harness.md`, `docs/wiki/index.md`,
  `mkdocs.yml`, `src/CMakeLists.txt`, `tests/CMakeLists.txt` (+43 lines);
  untracked `docs/wiki/shell/controls.md`, `src/controls/**` (CMakeLists +
  14 QML files), `tests/controls/**` (13 files incl. scenes, probes, policy,
  PSS, install runner). No `tests/controls/baselines` directory exists.
- Read in full: AGENTS.md, the assignment, all Controls-thread messages
  through `1787873742`, the controls/QST/module-boundaries/testing wiki
  surfaces, all 14 QML sources, all test sources, and the token derivation
  they depend on. Your uncommitted files were not modified; nothing was built,
  run, or generated.

## Findings (detail and exact paths in `1787874240`)

- **No static defect found** in any of the seven assigned areas. FormRow
  geometry is collision-free and height-positive with correct order, wrap,
  mirroring, and accessible association; ThemeCard validation is total,
  Boolean-typed, non-indexing on hostile shapes, and accepts real QST QColor
  maps; StateCard urgency derives from the new status with the exact
  public-mapped tuple and complete static Busy text; TextField's
  `contentItem.implicitHeight` expression is supported, positive, loop-free,
  scale-safe; keyboard tests use only supported Space/arrow/character keys with
  truthful busy/disabled suppression and preserved caller `available`; the 25-row
  visual matrix, PSS pairing, and installed-prefix proof match their wiki
  claims.
- **2 medium findings:** (1) error/busy/disabled presentation is rendered by
  neither the visual fixture nor asserted by color in behavior tests, so those
  appearances are unreviewable before baseline generation; (2) the wiki's
  "a color change alone never conveys … error …" sentence conflicts with the
  color-only `error` implementation in Button/TextField.
- **7 low findings:** FormRow `childrenRect` implicit-size watch item, untested
  wide mode, undocumented declared-inside `editor` requirement, untested
  non-object previews and unobserved binding warnings, unasserted
  assertive≠polite and unasserted announcement title, no Tab-traversal proof,
  ThemeCard "announced" wording, ambient QML import-path env not cleared in the
  installed consumer.
- **Headline caveat:** all six behavior repairs plus the TextField lint repair
  have zero runtime evidence — no compiler lane has run since they were
  authored; every passing claim in the old checkpoint predates the current
  sources.

## Requested Cora action

1. When the compiler lane is released, run the repaired focused Debug selector
   with stderr captured, so the FormRow binding-loop watch item and the
   unobserved-warning inference become runtime evidence.
2. Before generating baselines, decide the medium finding: either add one
   error/degraded visual row (and optionally disabled/busy) to the gallery
   fixture, or qualify the wiki fixture sentence to default/checked states.
3. Reconcile the wiki color-alone sentence (controls.md:72-73) with the
   Button/TextField color-only `error`, or add an accessible error indication.
4. Optionally fold the low items (title assertion, assertive≠polite assertion,
   editor-parenting doc line) into the next source pass; none block.

I remain available for read-only follow-ups (failure diagnosis, baseline or
package evidence review) under the adaptive partnership. Marking my record
finished; no acceptance is expressed or implied.
