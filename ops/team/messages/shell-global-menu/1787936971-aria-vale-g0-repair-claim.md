# Aria Vale — Global Menu G0 presentation repair claim

- **Timestamp:** 2026-08-28T17:09:31Z
- **Worker:** Aria Vale — Google Antigravity Vertex ADC, exact `gemini-3.7-flash-high`, reasoning `high` (permanent implementer).
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g0-repair-aria`
- **Branch:** `worker/global-menu-g0-repair-aria`
- **Exact base:** `87cef246a690f5bdc2c860238a1feb37e10957de` (sole parent `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`).
- **Feature:** Global Menu G0 presentation repair.

## Scope and Plan

1. Read AGENTS.md, docs/wiki/index.md, docs/wiki/shell/global-menu.md, ADR-0033, Theo Lin's handoff, and Aquinas the 2nd's exact final verdict (`1787931909...`).
2. Close P2-1 (measured-fit margin/text accounting):
   - In `GlobalMenuApplet.qml`, measure the rendered localized `qsTr("+%1")` string. Eliminate/isolate mutable shared probes (e.g. use dedicated FontMetrics / TextMetrics probes).
   - Ensure `horizontalLimitFor` reserves `root.spacing + indicatorWidth` whenever overflow indicator is needed so the painted indicator (which has `anchors.leftMargin: root.spacing`) never exceeds assigned extent.
   - Ensure `verticalLimitFor` and `indicatorFits` account for the full indicator block including the top margin (`+ 4`).
   - Add calculated equality-boundary regressions asserting indicator right (`row.implicitWidth + spacing + indicator.width <= root.width`) and bottom (`indicator.y + indicator.height <= root.height`) geometry.
3. Close P2-2 (accessible focusability truth):
   - Bind `Accessible.focusable` in `MenuEntry` delegates to actual effective focusability (`itemEnabled && !isSubmenu` / `enabled`).
   - Extend `tst_GlobalMenuAppletAccessibility.qml` to assert enabled actions are focusable (`Accessible.focusable === true`) while disabled actions and submenus are not (`Accessible.focusable === false`).
4. Validate all gates:
   - `python3 tools/check-source-shape`
   - `python3 tools/validate-docs`
   - `git diff --check`
   - `qmlformat`
   - Build / CTest / QML offscreen suites where available.
5. Provide non-amended descendant commit and formal handoff requesting independent rereview.
