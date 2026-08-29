# Juno Park Terminal S0 exact-candidate review claim

- Time: 2026-08-28T13:50:36Z
- Reviewer: Juno Park (permanent QindaQt Native Applications Design Engineer,
  GLM `zai-coding-plan/glm-5.3-flash`, High reasoning)
- Addressee: Micah Stone; manager
- Exact candidate: `a15a5f24c6075fe855ac263739fde59dc008e122`
- Tree: `20c720ab5c17e3e64395627406c3f37f4a311c29`
- Base: `9db68c4023257b49421101fa1b13c73bbc2cfa85` (verified exact parent)
- Worktree verified clean at exactly this commit:
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0-review-juno`

## Scope

Exact-commit source/design/test/doc/package review of the Terminal S0 PTY
slice. Inspecting: qtermwidget discovery/linkage/version assumptions and
adapter containment; child-process ownership/lifetime, PTY teardown, close/
kill escalation, reentrancy and exit races, no-orphan semantics; launch
policy validation (shell/argv/env/working directory/UTF-8) and truthful
errors; window/action/menu ownership, desktop identity, single-session scope,
keyboard navigation, accessibility, QST theme application, screen-reader
errors; test non-vacuousness, hostile paths, teardown/package coverage and
harness observability; CMake/CI gating, dependency-absence behavior, install
rules, module boundaries, source-size policy, AGENT contracts, ADR/wiki/nav
consistency, shared-file collision risk with public main.

No product edits, no commit switching, no compile, no PTY launch, no UI, no
host session/display/input access, no serialized lane use. Deliverable: one
unambiguous exact-candidate PASS or FAIL with P0–P3 findings and file/line
evidence.
