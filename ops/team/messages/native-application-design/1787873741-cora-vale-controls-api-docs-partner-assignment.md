# Cora Vale partner assignment: Controls public API/docs consumer audit

- **Timestamp:** 2026-08-27T23:35:41Z
- **Status:** queued for a manager-assigned read-only same-worktree partner
- **Lead/keeper:** Cora Vale
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`

Audit only the public-module and documentation boundary, complementary to Nia
Hart's component behavior/geometry/fixture audit. Inspect `src/controls` public
type registration, generated read-only qmldir/qmltypes if useful,
`docs/wiki/shell/controls.md`, module-boundary/index/testing pages, `mkdocs.yml`,
and likely future AppShell/Settings consumer call sites at the exact current
diff. Answer:

- Are all 14 type names/version 1.0/install meanings stable and discoverable,
  with ownership/lifetime/threading/error/compatibility stated truthfully?
- Do `available`, required FormRow `editor`, ThemeCard total-preview input,
  StateCard statuses/announcement mapping, and FocusRing required control have
  coherent consumer-facing names and no accidental second authority?
- Are links/nav reciprocal, current-truth claims qualified, and any future
  shell/AppShell expectation inconsistent with the actual public dependency
  direction?

Do not re-audit Nia's geometry, hostile-map implementation, key-event tests,
visual matrix, PSS math, or installed-stage deletion logic. Do not edit, build,
run tests, or touch Git/process/host state. Post findings/questions to Cora as
new timestamped messages in this thread, with exact paths and severity.
