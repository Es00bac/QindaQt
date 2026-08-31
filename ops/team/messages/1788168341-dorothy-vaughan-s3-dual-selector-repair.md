# S3 dual primary-selector root cause and bounded repair

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T03:25:41-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: selector repair focused-green; no nested rerun started

The preserved pre-selector probe in result
`ed38c78149a62da48562fda041d691ff` already proves both required 1920x1080
outputs, the exact horizontal arrangement, mapped per-output docks, required
apps/services, and private input. The timeout is therefore not general topology
or shell readiness.

Read-only backend discovery with the failed runtime's exact private Qt root:

```text
env -i PATH=/usr/bin \
  LD_LIBRARY_PATH=/tmp/qindaqt-arch-665/root/usr/lib:/tmp/qindaqt-arch-665/root/usr/lib/weston \
  QT_PLUGIN_PATH=/tmp/qindaqt-arch-665/root/usr/lib/qt6/plugins \
  QT_QPA_PLATFORM=offscreen LANG=C.UTF-8 LC_ALL=C.UTF-8 \
  timeout 10 /usr/bin/kscreen-doctor --info
```

reports zero available KScreen backends. The same read-only command with
`/usr/lib64/qt6/plugins` appended reports the installed
`/usr/lib64/qt6/plugins/kf6/kscreen/KSC_KWayland.so`. This diagnostic uses the
offscreen QPA, starts no nested compositor, and reaches no host display.

Official KDE libkscreen v6.6.6 source establishes both sides of the symptom:

- `Doctor::parseOutputArgs()` accepts `.primary` and calls `setPrimary`, which
  assigns priority 1; the requested semantics are supported.
- `BackendManager::listBackends()` searches only
  `QCoreApplication::libraryPaths()/kf6/kscreen`.
- On a missing in-process backend, `GetConfigOperation` emits a null config.
  `Doctor::configReceived()` warns and returns without quitting the event loop.
  This exactly accounts for the five-second parent timeout and empty selector
  artifact without implicating KWin's output-policy acknowledgement path.

Source evidence SHA-256: doctor `745d45b0…`, backend manager `7eacdf7a…`.
Preserved failure: result JSON `c442d4f2…`, probe 004 `a4be7372…`, empty
selector log `e3b0c442…`, full CTest log `515035d1…`. The archive is unchanged
and fresh process evidence remains zero-survivor.

I applied only the bounded tests/session repair:

- construct a selector-local environment;
- retain the private Arch Qt plugin root first;
- append the exact installed host KScreen plugin root only for
  `/usr/bin/kscreen-doctor`;
- fail before launch if `KSC_KWayland.so` is absent.

The general desktop environment, five-second deadline, exact
`output.WL-1.primary` command, topology, interaction, and primary-transfer
assertion are unchanged. Focused regressions prove private-first ordering,
caller-environment immutability, and fail-closed missing-backend behavior.
`PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3
tests/session/test_desktop_session_contract_unit.py` passes 22/22;
`git diff --check` passes.

Per the repair checkpoint requirement, no nested rerun has started. The next
action is the already-authorized full contained-session unit gate, followed by
exactly one serialized dual-row rerun if green.
