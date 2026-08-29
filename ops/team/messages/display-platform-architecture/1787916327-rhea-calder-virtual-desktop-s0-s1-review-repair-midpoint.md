# Rhea Calder — Interactive Virtual Desktop S0+S1 review-repair midpoint

- Timestamp: `2026-08-28T11:25:27Z`
- Reviewed HEAD: `fd9faab5ab79017be903dafc6f0587d09c511f49`
- Repair scope: nine modified paths, all within the original 20-path candidate
- Runtime/build: prohibited/not run; manager D0 integration compiler remains live

All five bounded review findings now have source and focused fake/unit coverage:

1. A hard-deadline polling contract reacquires complete public-D-Bus snapshots and accepts only one where service ownership, exact output/generation, input, dock, and both apps are valid simultaneously. Fakes cover services becoming owned before apps/dock, bounded never-ready timeout, and immediate method error.
2. Application extraction now requires and retains exact compositor-observed `applicationId`, window ID, and title. It emits the declared topology process role only because the current public Windows inventory exposes no client PID; a matching-title/wrong-ID negative test fails.
3. Each attempt reserves a never-reused build/run-sentinel result directory, copies every regular artifact and process log plus sandbox output and result metadata from a failure-safe `finally`, and rejects stale or symlink destinations. Units cover success, stale/symlink, timeout, and cleanup-failure provenance.
4. Complete evidence now requires exactly `residentPssKiB` and the 1,048,576 KiB ceiling, with nonnegative/bounded integer validation and missing/malformed/over-limit cases.
5. Exact teardown now returns one authenticated role/PID/process-group/executable-path/start-time record with terminal observation phase `already-exited`, `term`, or `kill`; validation requires every topology role and no code or prose calls these phases graceful.

Current direct safe evidence is 32/32 focused Python units, Python compilation exit 0, 962-file source-shape exit 0, 58-document/navigation validation exit 0, and whitespace exit 0. Source shape initially rejected a 470-nonblank runtime; moving observed application extraction to the cohesive existing topology module reduced it to 446, which is below the file limit and leaves only the tool's nonblocking decomposition-review warning. No compiler, CMake build, CTest package install, bubblewrap, compositor, private bus, nested session, or host endpoint was used.
