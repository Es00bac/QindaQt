# Codex DualSense layout

- Role: Controller layout implementer and integration owner
- Status: working — implement the user's exact DualSense desktop button layout
- Base: `24b89b00e75dc19c482954729edaa31c88126a45`
- Branch: `fix/dualsense-layout-20261020`
- Worktree: `.cache/dualsense-layout-20261020`
- Ownership: `src/controllers`, `tests/controllers`, controller Settings labels, controller wiki/ADR and bounded delivery
- Execution: current selected Codex session only; no delegated agents or external model workers

## Updates

- 2026-10-20T23:19:28+00:00 — Claimed exact user mapping: DualSense defaults, voice command capture, persistent gyro toggle and held Meta+Shift mouse actions. Preserve existing device motion preferences and other controller families. Acceptance: focused standalone build and controller tests, strict MkDocs/link checks, signed Portage delivery and Controllers1 readback.

- 2026-10-20T23:24:27+00:00 — Full map and separate DualSense defaults implemented; focused plugin/test build passes. New private held-input state orders Meta+Shift around mouse events. Existing installed desktop is r35; delivery must retain its complete signed image and rebuild only Settings Input and the controller plugin. ADR reserved as 0426 to avoid the newer animation branches.
