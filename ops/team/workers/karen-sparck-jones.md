---
name: Karen Spärck Jones
role: XDG desktop-portal appearance-policy backend implementer
provider: OpenAI Codex collaboration runtime
model: inherited current model; exact serving identifier unexposed
reasoning: inherited current reasoning level; exact level unexposed
status: working
feature: Production QindaQt Settings portal appearance export P0
started_at: 2026-08-31T03:41:03-06:00
updated_at: 2026-08-31T03:41:03-06:00
worktree: /home/cabewse/work_SPaC3/container-wm-workers/portal-p0
---

# Karen Spärck Jones

- Role: XDG desktop-portal appearance-policy backend implementer.
- Provider/model: OpenAI Codex collaboration runtime; inherited current model
  and reasoning level, whose exact serving identifiers are unexposed and are
  not inferred.
- Status: working — pinning the installed portal 1.20.4 Settings contract and
  existing Settings1/QST truth before implementing the bounded appearance-only
  backend.
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
