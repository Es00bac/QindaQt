# D0 source/static re-entry and integration-risk handoff

- **Timestamp:** 2026-08-28T04:32:46Z
- **Identity:** Rhea Calder, Display D0 compositor-output lead — OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Outcome:** revisioned Compositor1 output inventory plus exact VirtualBackend-gated development virtual-output seam
- **Exact HEAD/base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Branch/worktree:** `worker/display-d0` at `/home/cabewse/work_SPaC3/container-wm-workers/display-d0`
- **State:** waiting/not live; no compiler/private-runtime authority, no commit

## Integrity and complete dirty-path inventory

HEAD and branch remain exact. The working tree has 42 modified tracked paths (`573` additions, `184` deletions) and 8 new D0-owned paths (1,533 physical lines), 50 paths total. This is the same explained D0 inventory as the prior checkpoint; no foreign/unexplained write appeared during re-entry.

Modified tracked paths:

- `compositor/dbus/org.qindaqt.Compositor1.xml`
- `compositor/dbus/service.json`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `mkdocs.yml`
- `src/compositor/CMakeLists.txt`
- `src/compositor/include/qindaqt/compositor/controltypes.h`
- `src/compositor/include/qindaqt/compositor/shellvisibilitysnapshot.h`
- `src/compositor/kwin/kwincontrolendpoint.cpp`
- `src/compositor/kwin/kwincontrolendpoint.h`
- `src/compositor/kwin/kwinshellvisibilitypublisher.cpp`
- `src/compositor/kwin/kwinshellvisibilitypublisher.h`
- `src/compositor/kwin/managedwindowregistry.cpp`
- `src/compositor/kwin/managedwindowregistry.h`
- `src/compositor/kwin/mutationcontrol.cpp`
- `src/compositor/kwin/mutationcontrol.h`
- `src/compositor/kwin/qindaqtkwinplugin.cpp`
- `src/compositor/kwin/qindaqtkwinplugin.h`
- `src/compositor/src/shellvisibilitysnapshot.cpp`
- `src/session/sessionenvironment.cpp`
- `src/shell_visibility/include/qindaqt/shell_visibility/compositor_visibility_snapshot.h`
- `src/shell_visibility/src/compositor_visibility_snapshot.cpp`
- `tests/compositor/CMakeLists.txt`
- `tests/compositor/test_dbus_contract.py`
- `tests/compositor/tst_controlcodec.cpp`
- `tests/compositor/tst_kwininputadapter.cpp`
- `tests/compositor/tst_shellvisibilitysnapshot.cpp`
- `tests/session/CMakeLists.txt`
- `tests/session/compositorprobeclient.cpp`
- `tests/session/compositorprobeclient.h`
- `tests/session/compositorworkflow.cpp`
- `tests/session/compositorworkflow.h`
- `tests/session/sessionprobe.cpp`
- `tests/session/test_nested_session.py`
- `tests/session/tst_sessionenvironment.cpp`
- `tests/shell_visibility/tst_compositor_visibility_snapshot.cpp`
- `tests/shell_visibility/tst_compositor_visibility_state.cpp`
- `tests/shell_visibility/tst_compositor_visibility_wire_roundtrip.cpp`
- `tests/shell_visibility_client/tst_compositor_visibility_client.cpp`

New D0-owned paths:

- `src/compositor/kwin/kwindevelopmentoutputseam.cpp`
- `src/compositor/kwin/kwindevelopmentoutputseam.h`
- `src/compositor/kwin/kwinoutputinventory.cpp`
- `src/compositor/kwin/kwinoutputinventory.h`
- `tests/compositor/tst_kwindevelopmentoutputseam.cpp`
- `tests/compositor/tst_kwinoutputinventory.cpp`
- `tests/session/compositoroutputworkflow.cpp`
- `tests/session/compositoroutputworkflow.h`

## Dorian contract re-audit

All named pinned-KWin contracts remain present:

- Distinct containment gate: launcher clearing/exact marker is at `src/session/sessionenvironment.cpp:23-36`; three-fact evaluation is at `src/compositor/kwin/mutationcontrol.cpp:22-39`; exact-only construction and the distinct controller flag are at `src/compositor/kwin/qindaqtkwinplugin.cpp:38-69`; disabled requests reject before validation/backend access at `src/compositor/kwin/kwindevelopmentoutputseam.cpp:78-112`. No other production compositor source calls `createVirtualOutput`.
- Logical-size truth: public port and bounds are named `logicalSize`/`MaximumLogicalDimension` at `src/compositor/kwin/kwindevelopmentoutputseam.h:37-56,97-100`; the exact KWin call forwards that logical size at `.cpp:227-239`. No stale `pixelSize`/`MaximumPixelDimension` remains in the seam.
- Lifetime/ownership: the exact map is request-name to `QPointer<BackendOutput>` at `.h:79-107`; creation records the returned pointer at `.cpp:227-249`; removal checks the exact pointer after the synchronous backend call and retains authority on a no-op at `:252-269`; teardown copies and clears the map before calls at `:271-282`. D-Bus object/service unpublication precedes controller shutdown at `src/compositor/kwin/qindaqtkwinplugin.cpp:125-150`.
- Signal/generation: `src/compositor/kwin/kwinoutputinventory.cpp:259-339` subscribes to workspace/order plus complete logical/backend change sources and coalesces them with one zero timer; `:341-357` emits only for a newly accepted complete projection; `:360-395` samples semantic `Workspace::outputOrder()`. Equality/atomic retention/generation exhaustion live at `:149-236`.
- Field/order/bounds truth: full fields, `quint32` priority, and shared visibility limits are at `src/compositor/kwin/kwinoutputinventory.h:20-71`; serialization does not cap priority at `.cpp:191-205`; sampling preserves KWin semantic order and exact fields at `:360-389`. `tests/compositor/tst_kwinoutputinventory.cpp:99-116` exercises 64 outputs, scale 16, two equal `UINT32_MAX` priorities, then rejects 65.
- Shared generation: shell visibility subscribes only to the retained inventory invalidation at `src/compositor/kwin/kwinshellvisibilitypublisher.cpp:79-93` and copies its exact generation/value entries at `:256-268`. The consumer wire requires the canonical nonzero generation.

The corrected read-only source selector checked 18 invariants above and passed 18/18. Its first invocation had a local Python parenthesis typo and stopped with `SyntaxError` before executing an assertion; the corrected invocation exited 0. This was an ephemeral read-only command, not a repository defect or product edit.

## Authorized static evidence

All allowed repository commands below exit 0:

```text
git diff --check
tools/validate-docs
python3 tests/compositor/test_dbus_contract.py compositor/dbus/org.qindaqt.Compositor1.xml compositor/dbus/service.json
python3 -m py_compile tests/compositor/test_dbus_contract.py tests/session/test_nested_session.py
tools/check-source-shape --json
```

Counts: documentation validation covered 47 Markdown documents plus MkDocs navigation; source shape checked 839 files with 0 skips and 0 issues; the descriptor has 14 methods and 5 signals at protocol 1.1; the custom Dorian selector passed 18/18. `mkdocs` is not installed (`mkdocs_available=no`), so `mkdocs build --strict` was unavailable. No CTest fallback was used because binary test execution is forbidden in this lane.

## Public-main collision map

Public main from base through `2c52c985f846b083c2aebb7a08f04aa8318a2912` changes six paths. Two intersect D0, both in disjoint compatible hunks:

- `tests/session/CMakeLists.txt`: D0 lines `113-139` add `compositoroutputworkflow`; public-main lines `165-183` add the never-hidden surface-proof fixture wiring.
- `docs/wiki/development/testing-harness.md`: D0 lines `36-41,453-462` add the exact backend gate/hotplug row; public-main lines `206-244` add the deterministic never-hidden surface qualification boundary.

The other public paths do not collide: `docs/wiki/shell/panel-surfaces.md`, `tests/session/fixtures/shell_surface_profiles/qindaqt-surface-proof.json`, `tests/session/shellsurfaceprobe.cpp`, and `tests/session/test_shell_surface_nested.py`. The manager must preserve both shared-path sides; I did not merge, rebase, stash, or simulate a writeful integration.

## Remaining gates and requested action

No genuine owned source-static defect was found, so no product edit was made during this re-entry. The unresolved acceptance boundary is exact KWin 6.6.5 compilation/automoc qualification, focused inventory/controller/session/shell wire tests, strict-warning Debug/Release and broad registries, then private nested VirtualBackend add/remove convergence and non-virtual pre-parse containment. Staged/package, sanitizer/stress, strict MkDocs in an environment that provides it, process cleanup, and different-worker exact-candidate review remain open. Physical DRM/GPU/connector/monitor/lid/hotplug qualification remains explicitly unclaimed.

Tessa still owns the sole compiler/private-runtime lane. Resume D0 only after an explicit manager transfer, even if Tessa releases it; then qualify this preserved tree and prepare an exact commit/reviewer handoff. Only the manager integrates.
