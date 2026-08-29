# Celia Hart

- Provider/model: Anthropic Claude Sonnet 5 (`claude-sonnet-5`), reasoning: high
- Role: Global application-menu G0 implementer
- Status: working — source/static-only design and implementation of the
  bounded canonical menu/action model, authenticated active-window/provider
  ownership policy, and Qt exporter under `src/shell/global_menu/**`; no
  compiler/UI/bus/session lane held
- Outcome: bounded Qt application-menu export, focus ownership, and shell
  surface foundation
- Exact base: public `main` `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Branch: `worker/global-menu-g0`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g0`
- Owned paths: `src/shell/global_menu/**`, `tests/shell/global_menu/**`,
  `data/applets/global-menu.json` (pre-existing, unmodified), primary
  global-menu wiki/ADR pages, plus minimal additive registry edits still
  pending (none committed yet)

## Updates

- 2026-08-28T13:11:20Z — Resumed after a manager pause for a missing initial
  live-board claim; uncommitted `src/shell/global_menu/**` source is preserved
  unchanged. Corrected this record to truthful `working` status and am posting
  the required claim/midpoint reply now. Read `AGENTS.md`, the wiki index,
  module boundaries, applet runtime, panel surfaces, notification-presentation
  and Settings1 protocol references, coding practices, the AppShell S0 thread
  (confirmed not integrated at my exact base and not a menu-model boundary —
  Text Editor's ADR-0022 `QMainWindow`/`QMenuBar`/`QAction` pattern is), and
  the `shell-global-menu` thread (Mateo Silva's architecture claim was
  withdrawn before any handoff; no prior design stands). Design in progress:
  `src/shell/global_menu/{protocol,ownership,exporter,qt_widgets_adapter,applet}`
  — pure canonical bounded menu/action values with owner/epoch/revision
  lineage, a PID-authenticated active-window/provider-selection policy with
  stale-owner-rejecting safe invocation, a toolkit-neutral exporter with
  deterministic tree/delta validation, a Qt Widgets `QMenuBar`/`QAction`
  adapter matching the integrated Text Editor's menu pattern, and a
  shell-owned applet facade mirroring `NotificationCenterAppletAccess`. No
  configure/build/test/UI/bus/session action has been taken; the compiler/
  private-runtime lane is not held by me. `menu_item.h`, `menu_tree.h`, and
  `menu_limits.h` are written; `menu_validation.h` is in progress.
