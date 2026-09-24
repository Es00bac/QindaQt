---
name: claude-w13-dock
role: Worker (lane A — dock drag and drop, groups, and pinning)
provider: Anthropic Claude Code
model: claude-opus-5-5
status: handoff
feature: W13 drag and drop onto the dock and panels, dock groups, app pinning everywhere
worktree: /home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w13-dock
started_at: 2026-09-24T16:45:27Z
updated_at: 2026-09-24T21:56:47Z
---

# claude-w13-dock

- Status: handoff — W13 candidate committed and pushed on worker/claude-w13-dock-20260923 (configured and syntax-checked, not built); awaiting merge into the round branch.
- Exact base: `a77e2c1a` (head of `round/code-first-20260924`).
- Branch: `worker/claude-w13-dock-20260923`.
- Owns: `src/services/dock_items/**`, `tests/services/dock_items/**`, the
  dock facade and presentation in `src/shell/desktop_controls`
  (`quick_launch_*`, `dock_path_port.h`, `file_manager_dock_paths.*`,
  `QuickLaunchApplet.qml`, `Dock*.qml`, `DockDropGeometry.js`), the
  launcher's dock persistence, the pin/Keep in Dock menu entries, the
  `panels.dockItems` schema key, ADR-0265, and `docs/wiki/shell/dock-items.md`.
  Not layout-profile JSON (W20) and not applet dragging between panels (W14).

## Updates

- 2026-09-24T16:45:27Z — Claimed W13 in an isolated worktree at `a77e2c1a`; configured `build/dev` (configure only).
- 2026-09-24T21:51:41Z — Resumed after a usage-limit pause with the worktree intact: the dock value, launcher persistence, dock facade and QML, pinning entries, and tests are written and syntax-clean; writing docs, ADR-0265, and the handoff.
- 2026-09-24T21:56:47Z — Handoff: ADR-0265, docs, and tests written; syntax-check 33 ok / 10 NEEDS-GENERATED (moc) / 0 FAIL, the 10 re-checked clean without their moc include; validate-docs OK. Handoff message posted.
