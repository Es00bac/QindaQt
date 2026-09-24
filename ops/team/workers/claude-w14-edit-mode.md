---
name: claude-w14-edit-mode
role: Worker (lane A — panel edit mode, applet drag and drop within and between panels)
provider: Anthropic Claude Code
model: claude-opus-5-5
status: handoff
feature: W14 drag applets to rearrange panels, within and between panels
worktree: /home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w14-edit-mode
started_at: 2026-09-24T22:25:40Z
updated_at: 2026-09-24T22:58:40Z
---

# claude-w14-edit-mode

- Status: handoff — W14 candidate committed and pushed on worker/claude-w14-edit-mode-20260923 (configured and syntax-checked, not built); awaiting merge into the round branch.
- Exact base: `2d896244` (head of `round/code-first-20260924`, includes W7, W13, W17/W18, W19).
- Branch: `worker/claude-w14-edit-mode-20260923`.
- Owns: the panel QML chain (`RuntimePanel`, `PanelContent`, `PanelLiveCustomization`,
  `PanelAppletRow`, `AppletChip`, `AppletEditHandle`, `PanelEditBar`,
  `PanelQuickConfigMenu`), the edit-mode half of `LiveCustomizationController`
  (`livecustomizationcontroller_drag.cpp`), the Edit Panels entries (panel and desktop
  right-click menus, desktop menu View), `PanelEditKeyboard`, ADR-0266. Not the dock's
  own tile drag and drop (W13) and not layout-profile JSON (W20).

## What cannot work today (read end to end, 2026-09-24)

1. **Hard to find.** "Enter edit mode" exists only in the Meta+right-click
   customization menus (`PanelCustomizeMenu`, `DesktopCustomizeMenu`) and behind
   Meta+Shift+E. A plain right-click on a panel opens `PanelQuickConfigMenu` and on
   the desktop `DesktopContextMenu`; neither offers it, and nothing on screen names
   the shortcut.
2. **Applets keep working in edit mode.** `AppletEditHandle` only paints a frame and a
   ☰ and adds a passive `DragHandler`. Presses still reach the applet beneath: clicks
   fire, popups open, and an applet control that grabs on press (or the dock strip's
   own tile `DragHandler`, W13) competes with or blocks the edit drag.
3. **The zone `Flickable` can steal the drag.** `PanelAppletRow` is a horizontal
   `Flickable` (`HorizontalFlick` makes `xflick()` always true); in an overflowing
   zone it takes the grab from the edit drag.
4. **The Done/Undo bar covers the panel's trailing end.** It overlays the end zone, so
   the rightmost applets (clock, tray, clipboard on the Mac-style top bar) cannot be
   grabbed, and on a dock it sits outside the input mask, so it cannot be clicked.
5. **Releasing off target commits.** `hoverDropTarget` is only called when a target
   resolves; over the desktop nothing clears the last accepted hover, so a release
   there drops at the last hovered place instead of cancelling.
6. **Cross-panel drops are guesses, and cross-display drops are impossible.** The
   source surface resolves another panel from solved geometry by thirds and always
   appends (wrong for a centred dock); `panelSurfaceAt` filters by the source's own
   output id, so a panel on another display is never found, and a wildcard panel's copy
   on another display (same panel id) resolves to nothing.
7. **No preview.** Nothing shows where the applet will land (no gap, no marker).
8. **Escape does nothing.** Panels are `KeyboardInteractivityNone` layer surfaces, so no
   key ever reaches them.

What does work: each surface is its own layer-shell window, but a pointer drag keeps
its implicit grab (KWin blocks pointer-focus changes while a button is held), so the
source window keeps receiving motion outside itself, in its own coordinates, even on
another output. Solved surface geometry is in global logical coordinates.

## Updates

- 2026-09-24T22:25:40Z — Claimed W14 at `2d896244` in an isolated worktree; configured `build/dev` (configure only). Read the edit-mode path end to end; findings above.
- 2026-09-24T22:58:40Z — Handoff: Edit Panels entries (panel, desktop context menu, desktop menu View), inert applets with a whole-chip drag, the global drag point resolved by the panel under the pointer (cross-panel and cross-display; off-target cancels), gap and marker preview, the bar beside the zones with the Add applet… picker, Escape through on-demand panel keyboard; ADR-0266, docs and tests written. syntax-check 18 ok / 5 NEEDS-GENERATED (test moc) / 0 FAIL, the 5 re-checked clean without their moc include; validate-docs OK. Handoff message posted.
