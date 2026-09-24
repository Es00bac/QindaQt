---
name: claude-w8-desktop-work-area
role: W8 desktop icons below the top bar implementer
provider: Anthropic Claude
model: claude-opus-5-5
status: handoff
feature: W8 Desktop icons below the top bar (per-output desktop work area)
worktree: /home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w8-desktop-work-area
started_at: 2026-09-24T14:25:59Z
---

# claude-w8-desktop-work-area

- Role: W8 implementer (plan docs/plans/2026-09-23-settings-qindatk-and-network-plan.md).
- Status: handoff — W8 candidate on branch worker/claude-w8-desktop-work-area-20260923 (head SHA in the manager handoff); ctest -L desktop-surface 12/12 and the panel/shell pipeline rows green in build/dev; awaiting independent review.
- Exact base: `3dfec4c1`.
- Branch: `worker/claude-w8-desktop-work-area-20260923`.
- Product authority: `src/shell/desktop_surface/**`,
  `tests/shell/desktop_surface/**`, the minimal additive runtime wiring in
  `src/shell/runtime/` (reservation publication), and the desktop-surface wiki
  text (`docs/wiki/shell/applet-runtime.md` desktop-icons entry; ADR-0261 if a
  durable contract is recorded).

## Updates

- 2026-09-24T14:25:59Z: claimed W8; worktree created from 3dfec4c1.
- 2026-09-24T14:54:40Z: material findings (recorded at handoff, not refreshed live) — cause confirmed (full QScreen geometry into the icon flow; availableGeometry blind to layer-shell zones); chose runtime-published per-output depths from the accepted panel plan (PanelReservationInsets) consumed by DesktopSurfaceController as `workArea`; found a pre-existing failure in qindaqt.desktop-surface-customize-menu at base (test listed the real ~/Desktop, 24 entries cover its click point) and isolated it.
- 2026-09-24T14:54:40Z: handoff — ctest -L desktop-surface 12/12; panel/shell pipeline rows 26 passed (component-closure needs a full install build, not run meaningfully); mutation proof: 7/8 work-area rows and the auto-hide reservation row fail with the old behavior restored; qindaqt-shell builds; ./tools/validate-docs green. ADR-0261.
