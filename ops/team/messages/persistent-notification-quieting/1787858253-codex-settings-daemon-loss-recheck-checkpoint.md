# Settings1 repaired-candidate recheck checkpoint

- Reviewer: `codex-settings-final-release-reviewer`
- Timestamp: `2026-08-27T13:17:33-06:00`
- Exact candidate under review: `3de6bfae911594804e00a913f2feef5f1b36e16e`
- State: detached exact candidate, clean worktree; review remains in progress.

Completed evidence:

- Audited the one-commit repair diff from parent `2a1e2626e5d4e8e4526bfadbb8100931208f3179` and the cumulative Settings1 candidate. The production entry point binds `org.freedesktop.DBus.Local.Disconnected` on the same captured `QDBusConnection` used by `ResidentSettingsService`, before repository/service startup, and exits rather than reconnecting a stale repository lineage. The matching wiki, ADR, and testing-harness wording is consistent with that policy.
- Fresh reviewer-owned Debug and Release builds completed `864/864` each; the fresh production build completed `429/429`.
- Correctly quoted, inspectable CTest selection and execution completed in each Debug and Release tree: focused Settings1 registry `16/16` passed, full `qindaqt.*` registry `82/82` passed. The new `qindaqt.settings-service-process-lifecycle` test additionally passed `20` consecutive repetitions in each configuration.
- `all_qmllint` completed with status 0 in the fresh Debug, Release, and production trees (pre-existing warning output remains visible in the logs).
- `git diff --check HEAD^..HEAD` passed.

No P1/P2/P3 finding has been established at this checkpoint. Remaining release gates are the fresh staged install, independent installed private-D-Bus UnknownKey set/remove/mixed/stale/epoch-precedence proof, two real daemon-loss/replacement cycles with exact PID cleanup, deliberate failure-path cleanup check, strict docs/link/source-shape checks, and final HEAD/worktree/process cleanliness. No acceptance verdict is being issued before those candidate/runtime gates complete.
