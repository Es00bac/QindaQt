# Cora Vale handoff: nested 1080p shell timeout reproduces in isolation

- **Timestamp:** 2026-08-28T02:48:11Z
- **Status:** reproducible broad blocker; shell-owner action requested

Per manager direction, I reran only the first failing broad row without changing
shell product code:

```text
ctest --test-dir build/controls-debug \
  -R '^shell\.production-surface\.1080p$' \
  --output-on-failure --parallel 1
```

Result: **0/1 passed**, CTest exit 8, 15.49 seconds. The same stage-1 timeout
reproduced. The exact probe reports:

- `shellStarted: true`, `shellStopped: true`;
- baseline/output geometry 1920 x 1080 and expected reservation 84;
- top layer surface mapped with exclusive zone 30;
- bottom 998 x 54 layer surface configured and buffer-committed, then role
  destroyed and `mapped: false`;
- `protocolAmbiguous: true`, no active mapped snapshot, zero reserved/restored
  geometry, and `failure: timed out during stage 1`.

`LastTestsFailed.log` now contains exactly
`136:shell.production-surface.1080p`, and `LastTest.log` records the full probe
at line 10 and `Test Failed` at line 205. Post-test process inspection finds no
Controls-worktree KWin, qindaqt-shell, nested socket, or CTest child alive.

This is a reproducible broad shell-harness/product blocker outside Controls
ownership and should be routed to the shell owner. Controls itself remains
qualified at Debug build exit 0 and 29/29 focused tests. I have not changed any
shell source, started Release, or claimed a clean broad suite.
