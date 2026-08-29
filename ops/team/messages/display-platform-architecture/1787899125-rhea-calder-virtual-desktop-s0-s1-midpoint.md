# Rhea Calder — Interactive Virtual Desktop S0+S1 midpoint

- Timestamp: `2026-08-28T06:38:45Z`
- Exact base/HEAD: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Branch/worktree: `worker/virtual-desktop-s0-s1`; `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`
- Runtime lane: prohibited; Soren remains the sole owner

The live source candidate now adds only the owned session harness boundary plus one additive `tests/session/CMakeLists.txt` include/list edit. Implemented responsibilities are split across typed sandbox argv/environment/root ownership, exact staged artifact and Settings1/Audio1 direct-executable resolution, PID/executable/start-time authenticated teardown, proc PSS/CPU parsing, an immutable `1920x1080@1` topology/evidence contract, inner process orchestration, a public-D-Bus C++ probe, package/boot CTest rows, fixtures, and focused unit tests.

Direct safe evidence on the current tree:

```text
python3 -m unittest discover -s tests/session -p 'test_desktop_session_*_unit.py'
20/20 passed

python3 -m py_compile tests/session/desktop_session_*.py tests/session/test_desktop_session_*.py
exit 0

./tools/check-source-shape --largest 20
962 checked, zero issues

git diff --check
exit 0
```

The first shape audit correctly rejected a 518-nonblank combined runner. It is now decomposed into a 204-line outer containment driver and 331-line inner runtime module, with each responsibility below repository limits.

Public main does not yet contain Notification Live's development surface inventory. The topology requires a mapped/committed `scope=dock` record on `Virtual-1`, and the probe calls that future integrated method explicitly; an unknown method is a real boot dependency failure, never an inferred panel pass. No Notification-owned source/worktree was edited or copied. Next action is to land ADR-0023/testing authority, then perform fresh source/unit/configure/serial build/package-only gates without entering the private runtime.
