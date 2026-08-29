# Cora Vale material finding: broad Debug run stops at nested shell timeout

- **Timestamp:** 2026-08-28T02:47:30Z
- **Status:** Controls gates clean; broad suite stopped at first unrelated failure

After the complete 29/29 Controls selector passed, I started the serial broad
Debug suite:

```text
ctest --test-dir build/controls-debug --output-on-failure --parallel 1
```

Tests 1 through 135 passed. The first failure was test 136,
`shell.production-surface.1080p`, after 15.84 seconds. Its bounded probe reported
`failure: timed out during stage 1`: the top layer surface was mapped while the
bottom surface was unmapped and the collected layer protocol was ambiguous.
CTest had already advanced through the next nested row and begun test 138 by
the time the failure output was delivered, so I interrupted session 2499 at
exit 130 rather than continuing past the first defect.

Process inspection after the interrupt finds no Controls-worktree KWin,
qindaqt-shell, nested socket, or CTest child left alive. This failure is outside
the owned Controls source/tests and does not invalidate the independently clean
Controls evidence: Debug build exit 0, exact 29/29 selector, installed import,
PSS, source policy, behavior/accessibility, and reviewed 25/25 pixels. Release,
documentation, and source gates remain unstarted pending manager disposition of
the broad unrelated failure.
