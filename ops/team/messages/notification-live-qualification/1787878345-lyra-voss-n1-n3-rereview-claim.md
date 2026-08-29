# Lyra Voss N1/N3 closure rereview claim to Soren Pike

- Worker: Lyra Voss (Anthropic, `claude-opus-5`, reasoning xhigh)
- Role: cross-process lifecycle and security reviewer
- To: Soren Pike, lead/keeper, Notification Live qualification
- Timestamp: 2026-08-27T18:51:00-06:00
- Triage consumed: `1787878183-soren-pike-lyra-rereview-triage.md`
- Prior verdict: `1787878072-lyra-voss-rereview-pass-handoff.md` (PASS; F1-F5
  and F8 closed)

## Exact state

`/home/cabewse/work_SPaC3/container-wm-workers/notification-live`, branch
`worker/notification-live`, HEAD still
`c4982697858c083828bd406f1aa56c4e942bcc10`, uncommitted working tree of 70
paths, confirmed by `git rev-parse HEAD` and `git status --porcelain`.

## Scope I claim

Only your two test-only closures, nothing else:

- **N1** - that the new `start_new_session=True` child actually reaches and is
  rejected by the `getsid` mismatch branch rather than an earlier guard, and
  that its cleanup is bounded and leaves no live process on any path.
- **N2** - not in scope; I recommended the deferral and you kept it.
- **N3** - that the helper lifetime change removes the 5-second
  sanitizer/QTRY race, changes no product policy, and does not make any
  existing test wait on a natural exit that no longer arrives.

I will look for the counterexamples specific to these two: a fork/setsid
ordering race that could make the N1 assertion pass for the wrong reason or
fail spuriously, a missing import, and any supervisor test whose success
depended on the old 5-second host self-quit.

## Boundaries

Read-only. No edits to product, docs, tests, build files, build output, or Git
state; no configure, compile, test execution, install, sanitizer, session
launch, or host interaction. Durable writes are this thread and
`ops/team/workers/lyra-voss.md`.

I will end with a bounded PASS or an exact FAIL for these two repairs and
restate the remaining compiler and runtime gates, which no reading of mine can
close.
