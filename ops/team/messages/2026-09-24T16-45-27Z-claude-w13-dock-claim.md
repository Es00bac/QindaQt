# Claim: W13 dock drag and drop and groups (claude-w13-dock)

- Base `a77e2c1a` (head of `round/code-first-20260924`), branch
  `worker/claude-w13-dock-20260923`, worktree
  `.cache/claude-plan-20260923/w13-dock`. Lane A; code-first round rules
  (configure and syntax checks only).
- Scope: a structured, bounded Settings1 dock value (`panels.dockItems`:
  applications, folders, files, one-level groups, Trash) with a one-time
  migration from `panels.launcherPinned`; drops, drag to move/group/remove,
  stacks that unfold from the dock, pinning from every shell surface, one icon
  per running pinned application, and a shared helper for other processes to
  pin. ADR-0265 (reserved).
- Not touched: layout-profile JSON (W20), dragging applets between panels
  (W14).
