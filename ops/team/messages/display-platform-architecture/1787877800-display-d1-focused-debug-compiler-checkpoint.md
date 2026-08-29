# Display D1 focused Debug compiler checkpoint

- **Timestamp:** 2026-08-27T18:03:20-06:00
- **From:** Display D1 lead
- **To:** Manager, Iris Hale, Elara Finch, Mina Shah, Kai Mercer
- **Base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`

## Terminal evidence

The isolated strict-warning Debug configure completed with exit 0 using GNU
16.1.1. The eleven declared Display test targets then built serially with exit
0. The final serial, stop-on-first-failure selector completed 11/11 with zero
failures:

```text
ctest --test-dir build/display-debug --parallel 1 --stop-on-failure \
  -R '^qindaqt\.display-' --output-on-failure
100% tests passed, 0 tests failed out of 11
Total Test time (real) = 0.09 sec
```

The strict compile loop exposed and repaired 14 root-cause diagnostic sites
across nine files. They were complete aggregate initialization, an explicit
Qt JSON integer conversion, and missing test-only include/namespace setup;
there is no remaining warning in the focused graph.

## Behavioral stops consumed

The serial CTest loop stopped at exact reproducible boundaries before the
clean run:

1. 2/11: a locally marshalled `QDBusArgument` is write-only and cannot be used
   as a fabricated inbound demarshaller. The wrapper now rejects non-inbound
   arguments before extraction without warning or destination mutation. D1
   tests the static signatures and fail-closed boundary; positive inbound
   demarshalling is explicitly D2 private-bus evidence.
2. 4/11: the alias test indexed a registry after the public canonical sorter
   reordered it. It now follows the stable ID and proves the assigned alias.
3. 9/11: the disabled-snapshot fixture used default `QSize(-1,-1)`, which is
   intentionally invalid protocol data. It now uses the legal empty logical
   extent `QSize(0,0)` and reaches the survivor rollback contract.

No KWin, Wayland, shell, settings implementation, filesystem persistence,
real clock/timer, host state, D-Bus service/name/call, or compositor mutation
was introduced. Broad Debug, strict Release, sanitizer, docs/source/static,
and exact-candidate independent review remain before commit/handoff.
