# Integration handoff

## Current baseline

- Branch: `main`
- Commit: `11c1f4bf9de52da4d98151dd9ba251da0ed1fdbf`
- Outcome: authenticated lock-state notification privacy
- State: integrated and verified

The milestone authenticates all three lock-service owners against the
supervisor-provisioned KWin PID, uses a race-closed parent-death chain, requires
a double-inactive baseline, and fails closed on owner, reply, or bus loss. Every
notification projection and action is privacy-gated, and unlock starts from a
fresh authoritative baseline without replay.

Integrated evidence:

- Debug complete registry: 67/67 passed.
- Release complete registry: 67/67 passed.
- Focused private-D-Bus, offscreen QML, policy, facade, and supervisor tests
  passed.
- QML lint, source-shape, strict documentation, whitespace, and staged Release
  installation checks passed.
- No live compositor, real screen lock, active session bus, global input, or
  real desktop shortcut was exercised.

## Next outcome

Implement the persistent notification-quieting outcome in
[Task list](TASK_LIST.md). Candidate work must start from the baseline above in
an isolated worktree. Before integration, a separate worker must review the
exact candidate commit for persistence atomicity, D-Bus owner authentication,
restart behavior, privacy precedence, accessibility, and dependency direction.
