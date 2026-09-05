---
name: Dock Polish
role: Shell dock-presentation implementer
provider: OpenAI Codex collaboration runtime
model: GPT-6
reasoning: inherited
status: handoff
feature: Opt-in Mac-like task-list and quick-launch dock presentation
started_at: 2026-09-05T16:03:24-06:00
updated_at: 2026-09-05T16:03:24-06:00
worktree: /home/cabewse/work_SPaC3/container-wm/.cache/polish-dock
---

# Dock Polish

- Role: shell dock-presentation implementer.
- Status: handoff — candidate `b48c941c38a88b56b0c5084f5150ec1c7e5dbbe9`
  implements opt-in task-list and quick-launch dock tiles from exact base
  `5771c97644db5e48e622ab71154e4155c10c6714`.
- Branch: `codex/polish-dock`.
- Product authority: `src/shell/task_list/applet/**`,
  `src/shell/desktop_controls/qml/QuickLaunchApplet.qml`, focused task-list and
  desktop-controls QML tests, and `docs/wiki/shell/task-list.md`.

## Updates

- 2026-09-05T16:03:24-06:00 — Claimed isolated dock-presentation work. The
  panel owner will forward `dockMode` and `dockTileSize`; this lane keeps the
  existing taskbar and quick-launch behavior as the default.
- 2026-09-05T16:12:00-06:00 — Midpoint: added bounded icon-only task and
  persisted-pin dock tiles, task-only running indicators, popup-window
  tooltips/context menu, separator truth, and reduced-motion transform gate.
  The standalone task-list route has a pre-existing missing producer target;
  a top-level isolated build is compiling only the two changed QML test
  executables.
- 2026-09-05T16:20:00-06:00 — Handoff: candidate passes task-list and
  desktop-controls static boundary probes, `git diff --check`, and the
  source-shape gate with only repository-pre-existing warnings. A top-level
  isolated focused build did not finish before the manager's combined build
  lane took priority; the standalone task-list configure is independently
  blocked by its existing missing `QindaQt::CompositorShellActions` target.
