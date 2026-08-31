---
name: Karen Spärck Jones
role: XDG desktop-portal appearance-policy backend implementer
provider: OpenAI Codex collaboration runtime
model: inherited current model; exact serving identifier unexposed
reasoning: inherited current reasoning level; exact level unexposed
status: handoff
feature: Production QindaQt Settings portal appearance export P0
started_at: 2026-08-31T03:41:03-06:00
updated_at: 2026-08-31T04:37:12-06:00
worktree: /home/cabewse/work_SPaC3/container-wm-workers/portal-p0
---

# Karen Spärck Jones

- Role: XDG desktop-portal appearance-policy backend implementer.
- Provider/model: OpenAI Codex collaboration runtime; inherited current model
  and reasoning level, whose exact serving identifiers are unexposed and are
  not inferred.
- Status: handoff — exact clean candidate
  `b2271491239401adf1e4fffaed4fa57426e2a9ed` is frozen and ready for a
  different worker's exact review; Karen is not live after this response.
- Exact base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`.
- Branch: `worker/portal-p0`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/portal-p0`.
- Product authority: new `src/services/portal/**`, focused
  `tests/services/portal/**`, package/activation/config files, smallest
  additive root CMake/install seams, `docs/wiki/architecture/portal-service.md`,
  portal reference documentation if needed, `docs/wiki/adr/0054-*`, and
  additive wiki navigation. Session tests, Settings application routes,
  Display/D6, Network, shell customization, manager ledgers, and host portal
  state are prohibited.

## Updates

- 2026-08-31T03:41:03-06:00 — Claimed exact-base portal P0. Installed
  xdg-desktop-portal 1.20.4 provides the standard backend Settings interface at
  `/org/freedesktop/portal/desktop`, including `ReadAll`, `Read`,
  `SettingChanged`, version 1, and the standardized `color-scheme`, `contrast`,
  and `accent-color` values. This supports the assigned outcome without a
  private protocol or scope expansion. I am now deriving a typed, fail-closed
  Settings1/QST projection and the activation/package boundary.
- 2026-08-31T04:12:45-06:00 — Exact standard/source-truth boundary and first
  compile/test boundary established. The backend implements installed portal
  Settings v1 `ReadAll`, `Read`, `SettingChanged`, and `version=1` with only
  `color-scheme`, `contrast`, and `(ddd)` `accent-color`. It projects a public
  Settings1 exact-owner/epoch Ready snapshot through the public QST
  theme/catalog boundary and withdraws all reads on owner loss, malformed
  input, unknown theme, or stale lineage. Strict Debug compilation passes and
  focused policy/source/private-bus tests pass 3/3. Private-process activation,
  staged packaging/poison, documentation, and Release gates remain underway.
- 2026-08-31T04:37:12-06:00 — Froze exact clean candidate
  `b2271491239401adf1e4fffaed4fa57426e2a9ed` over assigned base
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`. Strict Debug and Release each
  pass the complete portal selector 7/7, including private process activation,
  daemon-loss exit/restart, installed runtime/package validation, and hostile
  source/installed poison. Documentation validation passes 113 documents,
  strict MkDocs builds, source shape checks 1,684 files with only pre-existing
  warnings outside the owned paths, `git diff --check` passes, no fixture
  process remains, and the product worktree is clean. Requested different-
  worker review of the immutable SHA; host portal/toolkit integration and all
  non-Settings portal families remain explicitly unclaimed.
