# Rhea Calder — Interactive Virtual Desktop S0+S1 source-review repair handoff

- Timestamp: `2026-08-28T11:32:16Z`
- Repaired candidate: `f28f443b7aae2d635481f49e847a7e1e1a3b573b`
- Tree: `cf840061b9680df539a150d28db09a6f97a00c6c`
- Exact parent/reviewed FAIL: `fd9faab5ab79017be903dafc6f0587d09c511f49`
- Original exact base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- State: clean non-amended descendant; no Rhea process remains live

## Bounded finding disposition

1. **P1 one-shot readiness — repaired.** `await_complete_snapshot()` enforces one monotonic 15-second hard deadline and reacquires complete public-D-Bus probe documents. It accepts only a single document whose exact services, output/generation, input, dock, and observed applications are simultaneously valid. Missing startup ownership/app/dock may retry; malformed service evidence or any public-method error fails on that sample. Units cover services before apps/dock, never-ready timeout, a sample completing after deadline, and immediate method/service failures.
2. **P1 manufactured application identity — repaired.** `observed_applications()` matches and retains the compositor's exact `applicationId`, nonempty window ID, and title; a matching title with another ID fails. Evidence also consumes the topology's declared `processRole`. The current public Windows inventory has no client PID, so the code explicitly does not invent a PID binding. Positive observed-ID/role and negative wrong-ID/role units pass.
3. **P1 stale/incomplete provenance — repaired.** Every invocation reserves a never-reused `desktop-session-results/<run-id>` root with its own build/run sentinel. Before private-root deletion, `finally` authenticates the source run sentinel and bounded output directories, authenticates the fresh destination, copies every regular artifact and every process log, then writes exclusive sandbox output and exact result metadata. Success, stale root, symlink root, tampered source, timeout, and inner cleanup-failure outcomes are covered. A result document is written last, so a partial copy cannot look complete.
4. **P2 missing PSS schema — repaired.** Complete evidence requires exactly integer `residentPssKiB` plus the fixed 1,048,576 KiB ceiling, nonnegative and not over limit. Missing, malformed, wrong-ceiling, and over-limit cases fail.
5. **P2 absent exit-mode evidence — repaired.** `terminate_processes()` returns one record per authenticated role with PID, process group, exact executable path, kernel start ticks, and terminal observation phase `already-exited`, `term`, or `kill`. The complete validator binds every record to the process topology and rejects missing, mismatched, or invented phases. Source, tests, ADR, and testing authority explicitly state that no phase claims graceful exit.

## Exact repair paths

```text
docs/wiki/adr/0026-contain-virtual-desktop-qualification.md
docs/wiki/development/testing-harness.md
tests/session/desktop_session_process.py
tests/session/desktop_session_runtime.py
tests/session/desktop_session_sandbox.py
tests/session/desktop_session_topology.py
tests/session/test_desktop_session_contract_unit.py
tests/session/test_desktop_session_nested.py
tests/session/test_desktop_session_process_unit.py
tests/session/test_desktop_session_topology_unit.py
```

These are ten paths already present in the reviewed 20-path candidate. No new file, Notification Live path, D0/D1 path, manager dashboard/task/handoff, or unrelated product path was added or edited.

## Exact-tree evidence

```text
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tests/session -p 'test_desktop_session_*_unit.py'
37/37 passed; exit 0

PYTHONPYCACHEPREFIX=build/virtual-desktop-python-cache python3 -m py_compile \
  tests/session/desktop_session_*.py tests/session/test_desktop_session_*.py
exit 0

ctest --test-dir build/virtual-desktop-debug-1787899125 --parallel 1 \
  --output-on-failure -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
2/2 passed; exit 0

ctest --test-dir build/virtual-desktop-release-1787899125 --parallel 1 \
  --output-on-failure -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
2/2 passed; exit 0

./tools/check-source-shape --largest 20
962 source files; zero skips, warnings, or issues; exit 0
desktop_session_runtime.py: 374 nonblank lines

./tools/validate-docs
58 documents/navigation validated; exit 0

git diff HEAD --check
git diff --exit-code HEAD --
clean exact tree; exit 0
```

No CMake build/compiler, bubblewrap, compositor, private bus, nested session, live boot row, UI, host display/input/cursor/session/config, or hardware endpoint was started or accessed. MkDocs remains unavailable. The original Notification `scope=dock` integration dependency and sole private-runtime allocation requirement are unchanged.

## Requested next action

Dorian: perform a bounded source-safe rereview of exact commit `f28f443b7aae2d635481f49e847a7e1e1a3b573b`, comparing its ten-path delta directly to your five findings in `1787915475`. Do not run the private boot row. If source review passes, return the exact commit/tree verdict to the manager; live maturity still requires later integrated-tree private qualification.
