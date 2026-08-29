# Rhea Calder — Interactive Virtual Desktop S0+S1 exact candidate handoff

- Timestamp: `2026-08-28T11:01:41Z`
- Candidate: `fd9faab5ab79017be903dafc6f0587d09c511f49`
- Tree: `faf4f4327e7c4e352d28b11ded1d24f5ce518e3a`
- Exact parent/base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Branch/worktree: `worker/virtual-desktop-s0-s1`; `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`
- State: clean exact descendant; compiler/test lane released; no private runtime was entered

## Outcome

This 20-path candidate is the smallest complete S0+S1 source boundary accepted in the prior consumption decision. It adds a typed bubblewrap namespace/environment/mount contract, sentinel-authenticated private roots, a per-user cross-worktree private-lane lock plus manager token, exact staged executable/plugin/service resolution, direct Settings1/Audio1 starts that ignore descriptor `Exec`, PID/executable/start-time authenticated TERM-to-KILL cleanup, deterministic process/service/output/input/dock/application evidence, a public-D-Bus probe, focused package/unit rows, and the registered `desktop.virtual.boot.1080p` row.

The final review caught and repaired one teardown defect: a still-live direct child whose executable could not be authenticated now fails the row after all authenticated groups are reaped instead of silently relying on namespace collapse. A focused unit covers that path.

## Exact changed paths

```text
docs/wiki/adr/0026-contain-virtual-desktop-qualification.md
docs/wiki/adr/index.md
docs/wiki/development/testing-harness.md
mkdocs.yml
tests/session/CMakeLists.txt
tests/session/DesktopSessionTests.cmake
tests/session/desktop_session_measure.py
tests/session/desktop_session_process.py
tests/session/desktop_session_runtime.py
tests/session/desktop_session_sandbox.py
tests/session/desktop_session_stage.py
tests/session/desktop_session_topology.py
tests/session/desktopsessionprobe.cpp
tests/session/fixtures/desktop_session/proc_stat.txt
tests/session/fixtures/desktop_session/smaps_rollup.txt
tests/session/test_desktop_session_contract_unit.py
tests/session/test_desktop_session_nested.py
tests/session/test_desktop_session_package.py
tests/session/test_desktop_session_process_unit.py
tests/session/test_desktop_session_topology_unit.py
```

No Notification Live-owned, D0/D1-owned, manager dashboard/task/handoff, host configuration, or other product path was edited.

## Acceptance evidence

Both roots use `BUILD_TESTING=ON`, `QINDAQT_BUILD_KWIN_PLUGIN=ON`, and `QINDAQT_BUILD_PRODUCTION_SHELL=ON`.

```text
cmake --build build/virtual-desktop-debug-1787899125 --parallel 1 \
  --target qindaqt-desktop-session-probe
exit 0; fresh target/dependency closure 501/501

cmake --build build/virtual-desktop-release-1787899125 --parallel 1 \
  --target qindaqt-desktop-session-probe
exit 0; cached interruption resumed and completed the remaining 292/292

ctest --test-dir build/virtual-desktop-debug-1787899125 --parallel 1 \
  --output-on-failure -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
2/2 passed; exit 0

ctest --test-dir build/virtual-desktop-release-1787899125 --parallel 1 \
  --output-on-failure -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
2/2 passed; exit 0

PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tests/session -p 'test_desktop_session_*_unit.py'
21/21 passed; exit 0

PYTHONPYCACHEPREFIX=build/virtual-desktop-python-cache python3 -m py_compile \
  tests/session/desktop_session_*.py tests/session/test_desktop_session_*.py
exit 0

./tools/check-source-shape --largest 20
962 source files checked; zero issues; exit 0

./tools/validate-docs
58 Markdown documents plus mkdocs navigation validated; exit 0

git diff HEAD --check
git diff --exit-code HEAD --
clean exact tree; exit 0
```

`ctest -N -R '^desktop\.virtual\.'` lists exactly the three intended rows in both roots: sandbox unit, package contract, and boot 1080p. `mkdocs build --strict` could not run because `mkdocs` is not installed; repository documentation validation passed.

## Deliberate remaining boundary

The live row was not executed. Dorian owns the sole private-runtime lane, and the assigned base does not yet contain Notification Live's development-only compositor surface inventory broadened to production `scope=dock`. The probe calls `DevelopmentShellSurfaces` explicitly and the topology requires a mapped and committed dock on current/desired `Virtual-1`; an absent method is intentionally a real dependency failure, never inferred from ordinary Windows or ShellVisibility evidence. No nested compositor, private bus, Wayland/XWayland session, UI, host display/input/cursor/session/config/hardware endpoint, or runtime process was started by this candidate.

Public `origin/main` advanced to `b62e132e067842b51f95aeaa377efef1dfda9bc5` after this exact-base worktree was assigned and allocated Power ADRs 0023–0025. The candidate was therefore renumbered to ADR-0026 before handoff. Manager integration must preserve the additive Power entries alongside this candidate's `docs/wiki/adr/index.md` and `mkdocs.yml` additions; no behavioral source collision was found.

## Requested next action

Assign an independent reviewer to exact commit `fd9faab5ab79017be903dafc6f0587d09c511f49` for source/security/package review. After D0/D1 and Notification Live are integrated and the manager explicitly transfers the sole private-runtime lane, run the one registered boot row in the integrated tree with the exact acknowledgement documented in the testing harness. Do not upgrade this source/package handoff into a live-desktop claim before that row passes and proves zero survivors/root cleanup.
