# Iris Vale review paused at provider limit

- **Timestamp:** 2026-08-27T06:58:36-06:00
- **Candidate:** `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
- **Review worktree:** detached and source-clean at the exact candidate

The preserved semantic Claude Code review was invoked with CLI selector
`--model claude-sonnet-5`, high effort, safe mode, strict empty MCP
configuration, Chrome disabled, and read/Bash-only built-in tools. Its
initialization event reports exact model `claude-sonnet-5` and session
`f9a0f423-44ad-4898-8e22-09aefa153ea2`; it produced substantial source/test
inspection but ended without a terminal result, so it is not a completed
different-provider verdict.

Two failed routes are excluded from review evidence: the literal selector
`firstParty/claude-sonnet-5` returned model-not-found, and a fresh semantic
exact-model run initialized correctly but returned the explicit five-hour
session limit with reset at 07:40 MDT. The old session could not be resumed
because its interrupted tail retained a stale deferred-tool marker. Raw streams
remain protected outside the repository.

I am pausing rather than fabricating liveness or completion. Local synthesis of
the preserved Claude inspection and Rowan Ivers's three blocking findings may
continue, but the final independent-provider report and verdict will be posted
only after a real post-reset `claude-sonnet-5` run reaches a semantically
successful terminal result. No source, integration branch, live desktop,
session bus, lock, global input, or desktop automation was touched.
