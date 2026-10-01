# Exact DPMS restore review: ACCEPT

- Reviewer: pf-dpms-restore-review-sol-20261001
- Timestamp: 2026-10-01T12:12:10Z
- Exact accepted product: `d08e49d13747cb0b71c3f13abfc8ecf058c46a4c`
- Exact base: `52c08cdb9f6fa0181beebdd02b42a54af78c2ba2`
- Review branch: `review/pf-dpms-restore-sol-20261001`
- Isolated review worktree: `.cache/pf-dpms-restore-review-20261001`
- Verdict: ACCEPT; no consequential blocking defect found. Acceptance applies only to this immutable candidate.

## Evidence and reasoning

Read the source, fixture/header, original and added assertions, changed idle-policy/testing-harness documentation, and authored operational handoff `2026-10-01T120212+0000-dpms-restore-handoff.md`. Exact handoff child `2c8db660798b9ba13a2129a825cb72b0230dbc47` changes only implementer board and handoff; source/tests/docs delta to candidate is zero. The combined manager branch and its unrelated candidates are outside this verdict.

The retained-display sync follows every final On request in protocol order. The single deadline covers pending dispatch, flush/EAGAIN and poll retries. Every successful prepare-read path reads or cancels; callback destruction precedes private queue destruction and existing GUI proxy/queue/display teardown. Ordinary requests remain blocked after lineage revocation, while final restore uses the same admitted connection without reopening.

Primary [KWayland v6.6.6 ConnectionThread](https://raw.githubusercontent.com/KDE/kwayland/v6.6.6/src/client/connection_thread.cpp) and [EventQueue](https://raw.githubusercontent.com/KDE/kwayland/v6.6.6/src/client/event_queue.cpp) source confirms socket reads execute on the connection worker, while GUI queue dispatch is pending-only. During the blocking worker acknowledgement no other reader races callback queue assignment. Installed Wayland1.24 source confirms HANGUP precedes READABLE server handling, explaining why flush followed by immediate disconnect lost the original restore. Private callback dispatch avoids invoking GUI-owned wrapper callbacks on the worker.

## Actual independent gates

No compiler was invoked. Reused the implementer's immutable Debug/strict-warning/plugin-OFF tree at `/home/cabewse/work_SPaC3/container-wm.worktrees/pf-dpms-restore-20261001/build/dpms`. Direct qinda Git source/test/docs/CMake identity to candidate exited0; clean checkout is the operational-only handoff child. Inspected actual CMakeCache and final build log, then ran the independent tests under manager's private-runtime grant.

Exact command in that qinda worktree:

```sh
env -u WAYLAND_DISPLAY -u WAYLAND_SOCKET -u WAYLAND_DEBUG -u DBUS_SESSION_BUS_ADDRESS \
 DBUS_SYSTEM_BUS_ADDRESS=unix:path=/home/cabewse/work_SPaC3/container-wm.worktrees/pf-dpms-restore-20261001/build/dpms/unavailable-system-bus \
 QT_QPA_PLATFORM=offscreen \
 ctest --test-dir build/dpms --output-on-failure -V \
 -R '^qindaqt\.(kwayland-dpms-controller|session-idle-display-stage)$' -j1
```

Exit0; 2/2 CTests pass in1.31s. Raw Qt totals: DPMS8 and idle10 passed, zero failed/skipped. Verbose CTest confirms DPMS `QT_FATAL_WARNINGS=1`. Original count5/last-On assertions are unchanged and pass; delayed peer live/revoked rows, one-opener checks, stalled-peer elapsed bound and absent/disconnected teardown all pass. Raw independent log is `build/review-dpms/independent-ctest.log` in the review worktree, mirrored on qinda at `/home/cabewse/.cache/pf-dpms-restore-review-20261001/independent-ctest.log`.

Independent static/document gates on exact candidate: `mkdocs build --strict --site-dir build/review-dpms/site` exit0; `tools/validate-docs` exit0 with481 Markdown documents/navigation; `tools/check-source-shape --root build/review-dpms/shape --config tools/source-shape.json --warnings-as-errors` exit0 for the four changed source/header files (production344 nonblank lines); exact base-to-candidate `git diff --check` exit0.

## Caveats, next action and release

Restore remains best effort when the peer never dispatches or dies. Artificial buffer saturation was not induced separately; source handling for flush backpressure was reviewed. No host display, bus, input, packages, physical outputs or nested-compositor qualification was exercised. The implementer's original timing-perturbed trace remains separate from deterministic failing predecessor and repaired evidence; no fresh reproduction of rejected source was needed or claimed.

Manager should integrate exact accepted candidate, rebuild affected targets on its combined tree, and rerun the blocked original combined/native gates. Compiler/private runtime are released; reviewer is waiting. Read Platform queue and peer handoff; concrete help offer: one bounded read-only review/reproduction of a manager-supplied exact combined-tree DPMS regression, using a new private grant and no candidate edits. No new product outcome claimed.
