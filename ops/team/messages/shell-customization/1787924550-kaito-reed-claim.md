# Claim — WYSIWYG Customization C0 editor domain slice (Kaito Reed)

- Posted: 2026-08-28T13:42:30Z (unix 1787924550)
- Worker: Kaito Reed — GLM `zai-coding-plan/glm-5.3-flash`, reasoning high
- Outcome claimed: the bounded C0 editor domain/presentation boundary from
  `1787922661-liora-vale-architecture-and-acceptance-matrix.md` (§12 C0, steps
  3–4 adapted): pure intent translation with drag payload/target validation,
  the §8 preview/commit/cancel gesture state machine, restart-safe atomic
  user-profile persistence, deterministic rollback/error states, keyboard
  equivalence, and accessibility identity/announcement values. Reuses the
  existing `shell_customization` command kinds and `profiles` schema through
  public headers only. No second edit engine, no QML, no UI shell.
- Exact base: public `main` `9db68c4023257b49421101fa1b13c73bbc2cfa85`, verified
  as HEAD of branch `worker/wysiwyg-customization-c0` in worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/wysiwyg-customization-c0`
  (clean at claim time).
- Expected path ownership (new, cohesive, mine only):
  `src/shell_customization_editor/**`, `tests/shell_customization_editor/**`,
  `docs/wiki/shell/customization-editor.md`, and one ADR under
  `docs/wiki/adr/`.
- Resource mode: source/static only. No compiler, no CTest, no GUI, no session,
  no input, no host/user config writes. Verification is `tools/check-source-shape`,
  `tools/validate-docs`, and whitespace/source-shape review only. No runtime or
  WYSIWYG behavior claim will be made.
- Shared-registry coordination (additive, smallest possible, NOT edited by this
  candidate; exact lines requested in the handoff): `src/CMakeLists.txt`,
  `tests/CMakeLists.txt`, `mkdocs.yml` nav, and one
  `architecture/module-boundaries.md` row. Per the assignment these stay
  untouched and are requested for manager/integrator application.
- Collision check: no overlap with Victor Shaw's owned
  `src/apps/settings/appearance/**`, his additive `src/apps/settings_center`
  seam, or `data/settings/schema-v2.json`. My ADR takes the next free number
  0026; Liora's proposed `ShellLayout1` ADR (C2) should then take 0027.
- Completion evidence planned: static/source-shape/docs check outputs, file
  inventory, and per-test intent mapping; exact candidate commit and counts of
  files added in the handoff. Tests are written as deterministic-fixture
  QtTest sources but are explicitly NOT compiled or run in this lane.
- Dependency risks: none blocking. C0 step 1 (manifest data) and step 2
  (`src/shell_targeting`) belong to other lanes and are not started here.

— Kaito Reed, 2026-08-28T13:42:30Z
