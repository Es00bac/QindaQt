# Rhea Calder — Display D0 exact candidate handoff

- Timestamp: `2026-08-28T05:39:04Z`
- Status: `handoff`; no live product/build/test/runtime process
- Exact commit: `f38453393ef2d10aaac1af27a4209b998fa8546e`
- Exact tree: `decfe17959650c123193a28007c5c77aefec86a5`
- Exact parent/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Branch/worktree: `worker/display-d0`, `/home/cabewse/work_SPaC3/container-wm-workers/display-d0`
- Repository state: clean; base is an ancestor; commit contains exactly 50 paths

## Outcome

The candidate publishes one validated, revisioned Compositor1 output inventory and uses that same generation/projection for `ShellVisibilitySnapshot`. It adds an exact KWin 6.6.5 `OutputBackend` adapter that may add/remove only its own virtual outputs, and only when the launcher has selected direct `--virtual` plus the explicit development test-scenario marker. Ordinary production sessions expose no output mutation capability and reject valid or hostile requests before parsing or backend access.

The isolated executable path proves five startup profiles plus a development add/remove transaction. Add and remove each advance the output generation once, deliver one `OutputsChanged`, and converge `Outputs` with `ShellVisibilitySnapshot` on the same generation/name set. The production row proves byte-identical `control-disabled` replies, unchanged inventory, and zero output signals.

## Exact changed paths

```text
compositor/dbus/org.qindaqt.Compositor1.xml
compositor/dbus/service.json
docs/wiki/architecture/compositor-session.md
docs/wiki/architecture/module-boundaries.md
docs/wiki/development/testing-harness.md
docs/wiki/index.md
docs/wiki/reference/compositor-control-v1.md
mkdocs.yml
src/compositor/CMakeLists.txt
src/compositor/include/qindaqt/compositor/controltypes.h
src/compositor/include/qindaqt/compositor/shellvisibilitysnapshot.h
src/compositor/kwin/kwincontrolendpoint.cpp
src/compositor/kwin/kwincontrolendpoint.h
src/compositor/kwin/kwindevelopmentoutputseam.cpp
src/compositor/kwin/kwindevelopmentoutputseam.h
src/compositor/kwin/kwinoutputinventory.cpp
src/compositor/kwin/kwinoutputinventory.h
src/compositor/kwin/kwinshellvisibilitypublisher.cpp
src/compositor/kwin/kwinshellvisibilitypublisher.h
src/compositor/kwin/managedwindowregistry.cpp
src/compositor/kwin/managedwindowregistry.h
src/compositor/kwin/mutationcontrol.cpp
src/compositor/kwin/mutationcontrol.h
src/compositor/kwin/qindaqtkwinplugin.cpp
src/compositor/kwin/qindaqtkwinplugin.h
src/compositor/src/shellvisibilitysnapshot.cpp
src/session/sessionenvironment.cpp
src/shell_visibility/include/qindaqt/shell_visibility/compositor_visibility_snapshot.h
src/shell_visibility/src/compositor_visibility_snapshot.cpp
tests/compositor/CMakeLists.txt
tests/compositor/test_dbus_contract.py
tests/compositor/tst_controlcodec.cpp
tests/compositor/tst_kwindevelopmentoutputseam.cpp
tests/compositor/tst_kwininputadapter.cpp
tests/compositor/tst_kwinoutputinventory.cpp
tests/compositor/tst_shellvisibilitysnapshot.cpp
tests/session/CMakeLists.txt
tests/session/compositoroutputworkflow.cpp
tests/session/compositoroutputworkflow.h
tests/session/compositorprobeclient.cpp
tests/session/compositorprobeclient.h
tests/session/compositorworkflow.cpp
tests/session/compositorworkflow.h
tests/session/sessionprobe.cpp
tests/session/test_nested_session.py
tests/session/tst_sessionenvironment.cpp
tests/shell_visibility/tst_compositor_visibility_snapshot.cpp
tests/shell_visibility/tst_compositor_visibility_state.cpp
tests/shell_visibility/tst_compositor_visibility_wire_roundtrip.cpp
tests/shell_visibility_client/tst_compositor_visibility_client.cpp
```

## Verification evidence

Fresh worktree-local roots used strict warnings, shared libraries, tests, KWin plugin, shell, and production shell, with host-uinput tests disabled. Every build used `--parallel 1`.

- Debug configure: exit 0.
- Debug focused compile: initial 12 D0 targets 230/230; required existing launcher 4/4 and decoration 5/5; repaired probe rebuild 3/3.
- Release configure: exit 0.
- Release focused compile: initial 12 D0 targets 230/230; required existing launcher 4/4 and decoration 5/5; repaired probe rebuild 3/3.
- Post-commit Debug focused CTest: 18/18, exit 0, 10.17 s.
- Post-commit Release focused CTest: 18/18, exit 0, 9.76 s.
- The 18 selectors comprise five output/control/snapshot C++ rows, four shell-visibility producer/consumer rows, session environment plus nested scenario parser, five virtual-output startup profiles, development nested hotplug, and production control/output containment.
- `./tools/validate-docs`: exit 0; 47 Markdown documents plus MkDocs navigation.
- `./tools/check-source-shape --largest 40`: exit 0; 839 files, 0 allowlist skips/issues.
- `python3 tests/compositor/test_dbus_contract.py compositor/dbus/org.qindaqt.Compositor1.xml compositor/dbus/service.json`: exit 0; protocol 1.1, 14 methods, 5 signals.
- `python3 -m py_compile ...`: exit 0 for the descriptor and nested-session Python sources.
- `git diff --check`, cached diff check, ancestry, exact 50-path scope, and clean status: exit 0.
- `mkdocs build --strict`: unavailable because `mkdocs` is not installed; repository-native navigation/link validation passed instead.

## Material qualification finding and repair

The first runtime command omitted the existing launcher and decoration targets, so it either stopped before KWin or selected Breeze; completing those build artifacts exposed one genuine D0 harness race. `awaitCoherentGeneration` could accept synchronous `Outputs`/`ShellVisibilitySnapshot` convergence immediately before the queued D-Bus `OutputsChanged` delivery, then falsely fail an immediate count check. The bounded repair in new D0-owned `tests/session/compositoroutputworkflow.cpp` retains exact-count enforcement but includes the expected signal count inside the existing event-loop convergence predicate and timeout diagnostic. Debug and Release both pass afterward.

## Isolation, cleanup, and caveats

The runtime used a fresh temporary HOME/all-XDG tree and private `dbus-run-session`; inherited session bus, `DISPLAY`, `WAYLAND_DISPLAY`, development/input markers, and dotool were removed. It launched direct `qindaqt-wm --virtual`, never a host or parent compositor. Exact post-test process inspection found no D0 launcher, probe, private KWin, or test bus. Two `/tmp/qindaqt-nested-test-*` roots found by name have Aug 26 mtimes and predate this Aug 28 run, so they were not touched.

This is deterministic KWin 6.6.5 VirtualBackend evidence only. It does not claim physical DRM/GPU/connector/monitor/lid/hotplug behavior, a Weston-parent whole-desktop graph, screenshots, PSS/CPU budgets, production-surface correctness, or synthetic-input coverage. Dorian's and Elara's ADR-0015 whole-desktop gaps remain unchanged; D0 is an additive production output-inventory/session seam for that later harness.

## Integration/review request

Dorian Vale or Elara Finch: independently review exact immutable commit `f38453393ef2d10aaac1af27a4209b998fa8546e`, especially marker isolation, KWin pointer/output ownership, signal/generation coalescing, production pre-parse containment, and the repaired signal-delivery convergence assertion. Report findings against this SHA, not prose.

Manager: after an independent pass, integrate this single commit. Preserve additive public-main/D1 hunks where shared `docs/wiki/development/testing-harness.md`, `tests/session/CMakeLists.txt`, MkDocs/navigation, compositor CMake, or protocol registries meet; D0 has no direct dependency on D1's pure transaction model. Rerun the same 18 selectors on the integrated tree. Only the manager integrates.
