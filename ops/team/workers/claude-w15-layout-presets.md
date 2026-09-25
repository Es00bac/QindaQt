---
name: claude-w15-layout-presets
role: Worker (lane A — Settings layout presets; all layout editing on the panels)
provider: Anthropic Claude Code
model: claude-opus-5-5
status: handoff
feature: W15 Settings switches layout presets; all editing happens on the panels
worktree: /home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w15-layout-presets
started_at: 2026-09-24T23:05:00Z
updated_at: 2026-09-24T23:57:00Z
---

# claude-w15-layout-presets

- Status: handoff — W15 candidate committed and pushed on `worker/claude-w15-layout-presets-20260923` (configured and syntax-checked, not built); awaiting merge into the round branch.
- Exact base: `689a9df0` (head of `round/code-first-20260924`, includes W14 edit mode and W10).
- Branch: `worker/claude-w15-layout-presets-20260923`.
- Owns: `src/apps/settings/customize/**`, `tests/apps/settings/customize/**`, the
  parity additions in `PanelCustomizeMenu`, `AppletCustomizeMenu`,
  `PanelLiveCustomization`, `DesktopCustomizeMenu`, `DesktopSurface` (settings
  lookup by plugin) and `LiveCustomizationController`, `UserProfileStore::remove`,
  the Customize search keywords and description, the Welcome tutorial's
  Customize step, ADR-0267.

## Parity check: every capability of the old Settings editor, on the panels

Read from the removed panes (`CustomizeCanvas`, `CustomizeAppletPalette`,
`CustomizeOutline`, `CustomizeProperties` and its panel/applet sub-panes,
`CustomizePointerGestures`, `CustomizeActionBar`) and the model methods they called.

| Old Settings capability | On the panels (W14 edit mode, right-click menus) | W15 action |
| --- | --- | --- |
| Pick a layout profile | Stays in Settings (the preset gallery) | — |
| Add an applet to a panel zone (palette drag, palette key, default insert) | Edit bar "Add applet…" per zone; panel menu Add applet ▸ | present |
| Move an applet within a zone, across zones, across panels (drag, keyboard move, zone buttons) | Edit-mode drag within/between panels and displays; applet menu Move to start/center/end, Move left/right, Move to panel ▸ | present |
| Remove an applet | Applet menu Remove | present |
| Duplicate an applet (button, Ctrl+D) | missing | **added**: applet menu entry 8 "Duplicate" (`duplicateApplet`) |
| Applet settings: switches, closed choices, bounded integers | Applet menu "‹applet› settings ▸" (all three kinds) | present |
| Panel edge | Panel ▸ Edge | present |
| Panel alignment | Panel ▸ Alignment | present |
| Panel thickness 20–192 px | Panel ▸ Size, but 16–160 (16–19 refused by the engine, 161–192 unreachable) | **fixed**: spin box 20–192, the engine's bounds |
| Panel length 10–100 % | Panel ▸ Length 10–100 % | present |
| Panel visibility (never, auto-hide, dodge active/all, maximized) | Panel ▸ Auto-hide (same five plus always) | present |
| Panel display scope (primary display / all displays) | missing | **added**: Panel ▸ Displays ▸ This display / All displays (`configurePanel(…, "output", …)`) |
| Add Desktop Icons to a layout without them | missing | **added**: desktop menu Desktop icons ▸ "Show desktop icons" |
| Remove Desktop Icons | missing | **added**: same toggle |
| Desktop Icons settings (placement, menu style, modifier, snap: switches/choices) | Desktop menu Desktop icons ▸ | present |
| Desktop Icons icon size (bounded integer) | missing (the submenu listed only switches and choices) | **added**: integer rows in Desktop icons ▸ |
| Move/duplicate the desktop applet | Only one desktop applet kind exists; reordering or doubling it has no visible effect | not applicable |
| Undo / Redo | Panel menu Undo/Redo, edit bar Undo, desktop menu Undo | present |
| Apply / Discard a draft | Panel edits apply immediately, each one undoable | not applicable |
| Wallpaper and window previews on the canvas | The live desktop is the preview | not applicable |
| Panel auto-hide delay (global, not a layout property) | Stays on the Settings page | kept |
| Add / remove a panel | Not in the old Settings editor; the panel and desktop menus have it | present (superset) |
| Keyboard-only operation (outline, Space move mode, Delete, Ctrl+D) | Menus are keyboard-navigable once open, but opening them and focusing a panel still need the pointer | **gap, reported**: needs chip focus traversal in edit mode; follow-up |

## Updates

- 2026-09-24T23:16:00Z — Claimed W15 at `689a9df0` (fast-forwarded from `6151fa24`, no overlap with W10). Read the Customize route, W14's edit mode (ADR-0266, panel-surfaces, customization-editor) and the live controller; parity table above. Configured `build/dev`.
- 2026-09-24T23:57:00Z — Handoff: Settings → Customize is a preset gallery (built-ins, then My presets; confirmed switching; save current layout as preset; rename/duplicate/delete own presets with name and count bounds; Modified with Restore original / Save as new preset; deleting the active preset switches to macos-inspired first); the old canvas, palette, outline, panes, previews and their tests are gone; the six parity gaps above are closed on the panel and desktop menus; `UserProfileStore::remove`; ADR-0267 and docs. Configure OK; syntax-check 10 C++ ok, 14 QML ok, 8 test sources NEEDS-GENERATED (their .moc only) and all 8 re-checked ok without it, 0 FAIL; `tools/validate-docs` OK; the Customize boundary scan passes. Handoff message posted.
