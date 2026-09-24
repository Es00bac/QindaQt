---
name: claude-w20-experiences
role: Worker (lane C, code-first round: familiar desktop experiences)
provider: Anthropic Claude Code
model: claude-opus-5-5
status: handoff
feature: W20 Familiar desktop experiences
worktree: /home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w20-experiences
started_at: 2026-09-24T23:16:00Z
updated_at: 2026-09-24T23:59:37Z
---

# claude-w20-experiences

- Status: handoff — W20 candidate committed and pushed on worker/claude-w20-experiences-20260923 (code-first: configure and syntax checks only, no builds); awaiting the round's combined build.
- Exact base: `e8c8daec` (head of `round/code-first-20260924`: W7, W12, W13, W17/W18, W19).
- Branch: `worker/claude-w20-experiences-20260923`.
- Owns: `data/profiles/**`, `data/themes/**` (four new experience themes, renamed display names),
  `data/decorations/luna-classic.json` (display name only), the three theme-authored title-bar keys in
  `src/themes/**` and their resolution in `src/decoration_painter/**`, the `workflow.fileManager` hint in
  `src/profiles/**`, the quick-launch `items` slices (dock facade rows, `QuickLaunchApplet`,
  `DockItemMenu`, the dispatcher and one line of `PanelAppletRow`), their tests, ADR-0268 and the
  layout/theme wiki rows.

## Updates

- 2026-09-24T23:16:00Z — Claimed W20 at `e8c8daec` in an isolated worktree; code-first rules (no builds or test runs).
- 2026-09-24T23:59:37Z — Handoff: six experiences as layout + theme pairs (two new layouts, four new
  themes, neutral names), theme-authored double-click / minimize-to-icon / clean title bar, the Mac-style
  dock's permanent File Manager and Trash ends, and the `workflow.fileManager` placeholder for W11s.
  syntax-check 17 ok / 13 NEEDS-GENERATED (all 13 ok with the `.moc` include stripped) / 0 FAIL;
  `./tools/validate-docs` exit 0; new theme contrast pairs and stock-profile invariants checked by script.
  ADR-0268. See the handoff message.
