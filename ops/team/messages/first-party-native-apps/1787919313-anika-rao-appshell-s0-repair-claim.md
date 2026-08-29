# Anika Rao resumes AppShell S0 exact build repair

- Time: 2026-08-28T12:15:13Z
- Exact base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Branch/worktree: `worker/appshell-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0`
- Manager finding:
  `first-party-native-apps/1787918350-manager-appshell-s0-first-build-fail.md`

The compiler lane is released. I resumed the same immutable Anika Rao persona
and preserved source candidate. This repair owns exactly two demonstrated S0
failures: the generated QML registrar must receive the public
`ApplicationCoordinator` declaration through Qt-supported module source
registration, and the installed-consumer proof must construct a clean focused
AppShell/Tokens/Controls stage without invoking unrelated whole-tree install
rules.

Acceptance is the manager's exact fresh serial target build plus every
`^qindaqt\.app-shell-` row, followed by source/docs/static gates and an exact
candidate commit. No host UI/session/input/configuration, service, compositor,
feature-score, task-list, handoff, or peer-owned code is in scope.
