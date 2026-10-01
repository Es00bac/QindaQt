# Manager: integrated native sleep modes qualify

- Time: 2026-10-01T15:27:00+00:00
- Exact integrated tested source: 6d449d988a60a707d52e7d34ef737e530cc2094f
- Accepted candidate: 50fb9c11620df99122665b3d6bdbe39684d003c0
- Exact different-worker review: 1d397416b88a432f08ed91884b08f7cfb4f4ac5c
- Strict affected configure/build: exit0; seven original focused Sleep/SupervisorSupport targets, not an all-target claim.
- Original eight-row CTest selection: exit0,8/8,60.64 seconds; actual six Qt totals10+12+17+16+72+16=143 passed,0 failed/skipped.
- Isolation: readonly host root, only private own build writable, private /tmp, empty activation session.conf overlay and /dev/null-only bind; actual UID1000 and RLIMIT_CORE0 before children, null/read-write/private tmp preflights pass. Installed configuration hash unchanged; actual namespace and process group gone; source unchanged.
- Evidence: qinda combined worktree build/manager-sleep-6d449d98-{configure,build}.log and build/combined/manager-sleep-6d449d98/{command.json,namespace-identity.json,ctest.log,parsed-summary.json}.
- Static gates: own docs484/strictMkDocs exit0; affected accepted ADR adopted from exact integrated evidence.

Root compiler/private slots released. Capture worker owns only minimal required-token fixture repair and bounded two selected probes, preserving every failure and unchanged success assertions. The observed ScreenshotResponse2 is not yet causally attributed to restricted admission; ScreenCast had not reached Start. No PF2 completion, real host sleep, physical protection or installed cutover claim.
