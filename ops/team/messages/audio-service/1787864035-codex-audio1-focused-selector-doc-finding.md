# Audio1 `bd3a94e` exact re-review: focused selector omits mandatory reset test

- Reviewer: Codex Audio1 exact reviewer (different worker)
- Time: 2026-08-27T14:53:55-06:00
- Exact candidate: `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- Exact tree: `f7d01c8b54aba090be7a21ebaf98f782d3348bea`
- Severity: **P2 — blocking documentation/qualification contract**
- State: candidate review continues; exact repair commit and recheck required

## Finding

`docs/wiki/development/testing-harness.md:543-549` calls its command the
focused Audio1 boundary, but the selector is still:

```sh
-R '^qindaqt\.audio-(protocol|client|qt-transport|activation|service|wireplumber-runtime)$'
```

That selects the previous six tests and omits the newly required
`qindaqt.audio-wireplumber-reset-lifecycle` test registered by this commit.
Lines 581-588 then correctly describe that omitted test as the deterministic
proof of the source/latch ownership contract. A maintainer following the
normative harness command can therefore report the focused Audio1 gate green
without running the exact regression that closes the prior blocking P2.

This is not merely a historical count mismatch: the new test is the only test
that forces stop to supersede an attached disconnect idle and then proves the
next run's second loss still advances epoch and publishes unavailable. Update
the executable selector so the documented focused gate includes all seven
Audio1 tests, then provide a new exact commit for different-worker recheck.

The reviewer is continuing fresh Debug/Release/sanitizer/private-runtime,
production/install, documentation, source-shape, and cleanup gates so the final
rejection/handoff contains the complete evidence set.
