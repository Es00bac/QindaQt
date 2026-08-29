# Notification Live static integration-risk handoff

- **From:** Soren Pike
- **Timestamp:** 2026-08-27T22:24:56-06:00
- **State:** waiting for explicit compiler/private-runtime lane transfer; not
  live

## Exact identity and prior review closure

- Worktree/branch: `container-wm-workers/notification-live`,
  `worker/notification-live`
- HEAD/base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Preserved dirty tree: 70 paths = 38 tracked modifications + 32 untracked
  additions
- Tracked binary-diff SHA-256:
  `dc5d63b9cdf117457e8d341b7e5be41cdd49fe2a7203bc4a20fbeac692683e26`
- Untracked content-manifest SHA-256:
  `9454c5dc57c1f3eacd21c0680173ace6b7f87e1a88eb6f7ac661e1389cddd4c4`
- Path-manifest SHA-256:
  `8f0deffb84a07d2de654be6a220ab9d8decae85504e22beafe371585daf895fb`
- Lyra Voss's exact source rereviews `1787878072` and `1787878550` closed
  F1-F5/F8 plus N1/N3 with no blocker. No repository path changed during this
  re-entry.

Public main was read through commit
`2c52c985f846b083c2aebb7a08f04aa8318a2912`, tree
`c576b53ec935ba112a02db410bed69dac331a08d`: ten commits and 89 changed paths
after the candidate base. None already defines the Notification live probe,
ShellDevelopment endpoint, DevelopmentShellSurfaces method, or ADR-0019/0020.

## Static evidence at this exact identity

- `PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=tests/session python
  tests/session/test_notification_live_unit.py`: exit 0, 10/10. This starts
  only disposable Python interpreter children; it invokes no QindaQt product,
  bus, display, input, shortcut, or locker process.
- `python tools/docs_validation.py --root .`: exit 0, 44 Markdown documents and
  navigation validated.
- `python -m tools.source_shape.cli`: exit 0, 799 files, zero skips and zero
  warnings/errors.
- `git diff --check`: exit 0.
- Post-gate identity remains 70 paths. No configure, compiler, CTest/product
  binary, install, session/UI, host bus/display/input, shortcut, locker, or
  configuration action ran.

## Complete current collision map

Exactly four paths intersect public main; no product source path intersects:

1. `docs/wiki/adr/index.md` is a direct same-anchor additive collision at base
   line 21: public ADR-0013/0014 and Notification ADR-0019/0020. Preserve all
   four in numeric order.
2. `mkdocs.yml` has disjoint public architecture/reference additions, but the
   ADR tail is the same base-line-64 insertion anchor. Preserve QST/Audio
   navigation and ADR-0013/0014, then ADR-0019/0020.
3. `docs/wiki/development/testing-harness.md` is shared with disjoint base
   hunks: public QST line 120, deterministic never-hidden surface proof lines
   181-211, and Audio line 595; Notification lines 221, 251, 298, and 309-329.
   Preserve every section. In particular, Notification qualification must not
   revert the new `qindaqt-surface-proof`/never-hidden boundary.
4. `tests/session/CMakeLists.txt` is a shared additive registry with disjoint
   base hunks: public surface fixture/profile arguments at 165/177;
   Notification driver/syntax/tool registrations at 71-96 and isolated
   `NotificationLiveTests.cmake` include at 225. Preserve both sets exactly.

Safe later order: finish candidate-only Debug/Release/sanitizer/package/private-
nested gates on the preserved base; create one milestone commit; obtain exact-
commit different-worker review; then let the manager apply it on top of current
public main, resolving the two same-anchor registries additively and retaining
the two disjoint shared files. Rerun combined docs/source/whitespace, production
surface rows, Notification focused/live rows, package/install, and the affected
broad registries on the integrated tree. Re-audit these four paths if Controls,
Display, or public main advances again before integration.

## Current problem and exact next commands

The source is ready, but completion is blocked on serialized evidence, not a
known code defect: Controls owns the only compiler/private-runtime lane. The
candidate remains uncommitted and makes no live/runtime qualification claim.
Even if the lane appears free, explicit manager transfer is required.

First compiler-owned boundary:

```sh
cmake --build build/notification-live-debug-current --parallel 1 \
  --target qindaqt_session_supervisor_tests qindaqt-notification-live-probe
ctest --test-dir build/notification-live-debug-current \
  -R '^(qindaqt\.session-supervisor|session\.notification-live-driver-unit)$' \
  --output-on-failure
```

If that passes, refresh full Debug provenance and the prior 50-test focused
selector, then Release:

```sh
cmake --build build/notification-live-debug-current --parallel 1
ctest --test-dir build/notification-live-debug-current \
  -R '^(compositor\.(development-input-protocol|kwin-development-input-injector)|qindaqt\.(notification.*|notifications.*|settings.*|session-lock.*|session-supervisor|shell-runtime.*)|session\.(notification-live-driver-unit|python-syntax))$' \
  --output-on-failure
cmake --build build/notification-live-release-current --parallel 1
```

Sanitizer, staged package, five installed private resolution/scale rows,
race-10x, milestone commit, and exact-candidate review remain required after
these commands.
