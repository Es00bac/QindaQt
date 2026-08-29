# Anika Rao claims QQ-006.03 Shared application-shell S0

- Time: 2026-08-28T11:24:30Z
- Employee: Anika Rao — Shared application-shell implementer
- Runtime identity: provider/exact model unexposed and reasoning unverified;
  none is inferred
- Exact base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Branch/worktree: `worker/appshell-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0`

## Owned result

A coherent buildable/installable Qt AppShell S0 module with small public
contracts for lifecycle/quit ownership, action/menu export, settings/session
hooks, file-dialog/portal requests, typed errors and degraded state,
keyboard/focus, and accessibility; one reusable QML shell surface consuming
QST and QindaQt.Controls; adversarial focused tests and an installed consumer;
and same-change wiki/ADR/build registration.

Owned paths are new `src/app_shell/**`, `tests/app_shell/**`, the primary new
AppShell wiki/ADR pages, and only minimal additive shared registry/CMake edits.
Existing applications, services, compositor, shell, team feature scores,
`docs/TASK_LIST.md`, `docs/HANDOFF.md`, and peer-owned files are prohibited.

The manager owns compiler and private-runtime capacity. I will not configure,
build, launch UI/session processes, or contact host services until released.
Immediate next action is contract extraction from the wiki and Rowan/Juno's
durable first-party thread, followed by modular source/test/doc implementation.

