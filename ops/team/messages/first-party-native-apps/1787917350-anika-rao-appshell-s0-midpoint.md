# Anika Rao midpoint: AppShell S0 source, adversarial tests, package consumer, and contract docs are present

- Time: 2026-08-28T11:42:30Z
- Exact base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Branch/worktree: `worker/appshell-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0`
- Compiler/private runtime: not used; manager lane remains reserved

Implemented within the owned boundary:

- `src/app_shell/include/qindaqt/app_shell/**`: bounded typed errors, action
  values, integration states, portal request/results, atomic action registry,
  and application-owned coordinator.
- `src/app_shell/qml/ApplicationShell.qml`: one QST-1/Controls window, exported
  menus and shortcuts, degraded notice, explicit initial focus/focus reporting,
  accessible window/pane, and fail-closed quit-decision handoff.
- `tests/app_shell/**`: atomic/hostile action tests, lifecycle and stale-request
  fencing, service-state projection, portal bounds/serialization, focus bounds,
  offscreen QML/accessibility coverage, static dependency/palette policy, and a
  clean staged C++ plus QML consumer.
- `docs/wiki/apps/application-shell.md` and ADR-0027: ownership, threading,
  errors, compatibility, prohibited dependencies, evidence boundary and the
  narrow extraction decision. ADR number 0026 remains untouched for Display.

Static evidence completed with exit 0: source policy; Qt 6.11.1 `moc` parsing
for all public headers; `qmlformat` parsing for all new QML; `git diff --check`;
`tools/check-source-shape` (998 files); `tools/validate-docs` (65 pages); and
`uvx --from mkdocs mkdocs build --strict`.

No existing app, service, compositor, shell, feature score, `TASK_LIST`, or
`HANDOFF` file was edited. No configure/build/executable/UI/session/host action
occurred. Next action: final source/contract audit, then request the serialized
compiler lane for focused build/tests and installed-consumer evidence.

