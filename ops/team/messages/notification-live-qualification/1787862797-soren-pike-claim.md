# Soren Pike claim: installed notification interaction qualification

- **Timestamp:** 2026-08-27T14:33:17-06:00
- **From:** Soren Pike, notification live-session qualification
- **To:** Manager, shell/session owners, and future exact-commit reviewer
- **State:** working; no implementation or qualification result is claimed yet
- **Exact base:** `c4982697858c083828bd406f1aa56c4e942bcc10`
- **Branch/worktree:** `worker/notification-live` at
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`

## Owned outcome and paths

I claim the user-visible outcome and acceptance boundary in
`1787854530-manager-next-shell-outcome.md`. I own the focused driver and
fixtures under `tests/session/**`, the primary notification-presentation and
session-testing wiki sections, and only the smallest production shell/session
repair that failed production-path evidence proves necessary. Shared CMake and
documentation registries will receive additive edits only, with a board
checkpoint before any dependency or test-harness convention changes.

## Safety and evidence boundary

All runtime work will use a staged install, disposable XDG roots and Wayland
socket, and a private D-Bus daemon. Input may enter only through the existing
production-gated nested KWin development device. This lane will not touch host
uinput, the developer's cursor or seat, the active desktop/session bus,
KGlobalAccel registry, lock screen, or user configuration. A package/runtime
absence will be reported precisely after safe discovery; it will not be
replaced by weakened PID authentication or fabricated success.

## First actions

1. Audit the staged production session composition and existing nested surface,
   development-input, private-bus, Settings1, shortcut, and lock-state seams.
2. Build one focused, bounded driver/probe architecture rather than extending a
   monolithic session script.
3. Establish actual environment support for private KGlobalAccel and nested
   KScreenLocker before deciding which production fixes are needed.
4. Qualify the exact required interaction and lifecycle matrix, then run broad
   Debug/Release/production/package/documentation gates before one candidate
   commit and independent review request.

