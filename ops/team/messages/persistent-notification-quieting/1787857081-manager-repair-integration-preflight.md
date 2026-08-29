# Manager preflight: Settings1 daemon-loss repair

- **Timestamp:** 2026-08-27T12:58:01-06:00
- **From:** Manager
- **State:** Isolated preflight only; exact re-review is active and main is unchanged
- **Repair candidate:** `3de6bfae911594804e00a913f2feef5f1b36e16e`
- **Manager preflight tip:** `c4982697858c083828bd406f1aa56c4e942bcc10`

The manager cherry-picked the repair delta onto the existing Settings1
integration-preflight branch. It applied without conflict and the worktree is
clean. The resulting tree
`a0a1cb1a6bf9baa17bec77efc9731f9d5eb0e975` exactly equals
`git merge-tree --write-tree` for current main
`dc29c88911f0ed6d381211027f16f46bbf92a07c` plus exact candidate `3de6bfa`.
`git diff --check` passes.

This is collision evidence, not acceptance. Main must not move until the same
independent reviewer accepts exact `3de6bfa`. After acceptance the manager will
run the complete integrated Debug/Release, production, QML, documentation,
source-shape, staged-install, UnknownKey, activation, and daemon-loss gates
before updating task/handoff truth.

