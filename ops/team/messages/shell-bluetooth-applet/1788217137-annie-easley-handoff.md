# Annie Easley — Bluetooth applet B1 exact candidate handoff

- Timestamp: 2026-08-31T16:58:57-06:00
- Exact candidate: `ecadc745fdea1e22cbbcfcbcbab1738507231b8c`
- Exact tree: `b89ac700456690812da0d7dc2efb8e221a94a3a9`
- Sole parent: `7c09625eb0fb40136651bab414a137867b943d12`
- Exact integrated-main ancestor: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Exact ordinary merge: `f23b61d91fdf76f6e4cecaa87808a16dd48f116b`, tree `89ae4a91648b24f54d3c0d2e02f8e65f3724716b`, parents `f8a85aeee56969e0a6e46970023247a6c70d4c52 74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Preserved source milestone ancestor: `c2cf9a0066e0175a99b1dfaea0735ae2569794b8`
- Branch/worktree: `worker/bluetooth-applet-b1` at `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-applet-b1`
- Requested action: independent immutable review of the exact candidate before manager integration.

## Outcome

The candidate adds the production built-in Bluetooth panel applet over only the
public Bluetooth1/BluetoothClient boundary. It provides bounded non-address
projection, exact-owner/epoch/revision operation admission, one applet-owned
discovery lease, compiled keyboard/accessibility QML, audited manifest/policy
and stock-profile placement, production-shell composition, and a relocated
installed-package/source-poison proof.

Manager-audit repairs are included. A malformed/stale `no-lease` completion can
no longer retire the only tracked release target. A validated successful
mutation retains a pending-equivalent control fence until same-owner/same-epoch
authoritative truth reaches the result's observed revision, preventing a second
dispatch against the stale initiating snapshot. Owner/state loss fails closed
and no result is replayed. The QML lease-close fixture supplies exact rev6/rev7
convergence truth. The installed test supplies the exact build-selected KF6
GlobalAccel artifact inside its disposable stage and clears ambient loader
paths, requiring the shell's relative install RUNPATH to resolve it locally.

## Exact changed paths relative to integrated main

Exactly 71 paths differ from `74da46345c7a5094d45c756ad8b23ca87591fcd3`:

```text
A data/applets/bluetooth.json
M data/profiles/qindaqt.json
M docs/wiki/architecture/bluetooth-service.md
M docs/wiki/architecture/module-boundaries.md
M docs/wiki/development/implementation-roadmap.md
M docs/wiki/development/testing-harness.md
M docs/wiki/index.md
M docs/wiki/reference/applet-manifest-schema-v1.md
M docs/wiki/shell/applet-runtime.md
A docs/wiki/shell/bluetooth-applet.md
M mkdocs.yml
A ops/team/messages/shell-bluetooth-applet/1788177628-annie-easley-claim.md
A ops/team/messages/shell-bluetooth-applet/1788179752-annie-easley-static-midpoint.md
A ops/team/messages/shell-bluetooth-applet/1788180923-annie-easley-static-audit.md
A ops/team/messages/shell-bluetooth-applet/1788213843-annie-easley-resume-merge-claim.md
A ops/team/messages/shell-bluetooth-applet/1788214188-annie-easley-release-lineage-repair.md
A ops/team/messages/shell-bluetooth-applet/1788214617-annie-easley-success-convergence-repair.md
A ops/team/messages/shell-bluetooth-applet/1788214846-annie-easley-compiler-lane-claim.md
A ops/team/messages/shell-bluetooth-applet/1788215125-annie-easley-debug-build-red.md
A ops/team/messages/shell-bluetooth-applet/1788215569-annie-easley-debug-fixture-build-red.md
A ops/team/messages/shell-bluetooth-applet/1788215664-annie-easley-debug-result-fixture-red.md
A ops/team/messages/shell-bluetooth-applet/1788215723-annie-easley-debug-controller-fixture-red.md
A ops/team/messages/shell-bluetooth-applet/1788215770-annie-easley-debug-qml-fixture-red.md
A ops/team/messages/shell-bluetooth-applet/1788215856-annie-easley-controller-convergence-red.md
A ops/team/messages/shell-bluetooth-applet/1788215983-annie-easley-controller-lineage-red.md
A ops/team/messages/shell-bluetooth-applet/1788216152-annie-easley-debug-selector-red.md
A ops/team/messages/shell-bluetooth-applet/1788216308-annie-easley-kf6-target-scope-red.md
A ops/team/messages/shell-bluetooth-applet/1788216398-annie-easley-debug-adjacent-manifest-red.md
A ops/team/messages/shell-bluetooth-applet/1788216456-annie-easley-debug-green-release-claim.md
A ops/team/messages/shell-bluetooth-applet/1788217008-annie-easley-debug-release-green.md
A ops/team/workers/annie-easley.md
M src/CMakeLists.txt
M src/applet_runtime/src/builtin_applet_registry.cpp
M src/shell/CMakeLists.txt
A src/shell/bluetooth_applet/CMakeLists.txt
A src/shell/bluetooth_applet/include/qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h
A src/shell/bluetooth_applet/include/qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h
A src/shell/bluetooth_applet/include/qindaqt/shell/bluetooth_applet/bluetooth_request_state.h
A src/shell/bluetooth_applet/qml/BluetoothAdapterRow.qml
A src/shell/bluetooth_applet/qml/BluetoothApplet.qml
A src/shell/bluetooth_applet/qml/BluetoothDeviceRow.qml
A src/shell/bluetooth_applet/src/bluetooth_applet_controller.cpp
A src/shell/bluetooth_applet/src/bluetooth_applet_controller.h
A src/shell/bluetooth_applet/src/bluetooth_applet_presentation.cpp
A src/shell/bluetooth_applet/src/bluetooth_request_state.cpp
M src/shell/qml/AppletChip.qml
M src/shell/qml/BuiltinAppletContent.qml
M src/shell/qml/PanelAppletColumn.qml
M src/shell/qml/PanelAppletRow.qml
M src/shell/qml/PanelContent.qml
M src/shell/qml/RuntimePanel.qml
A src/shell/runtime/bluetoothappletcomposition.cpp
A src/shell/runtime/bluetoothappletcomposition.h
M src/shell/runtime/runtimepanelwindowfactory.cpp
M src/shell/runtime/runtimepanelwindowfactory.h
M src/shell/runtime/shellruntimeapplication.cpp
M src/shell/runtime/shellruntimeapplication.h
M tests/CMakeLists.txt
M tests/applet_runtime/tst_applet_instance_resolver.cpp
M tests/applets/tst_catalog.cpp
M tests/applets/tst_manifest.cpp
A tests/shell/bluetooth_applet/CMakeLists.txt
A tests/shell/bluetooth_applet/check_boundary.cmake
A tests/shell/bluetooth_applet/check_runtime_boundary.cmake
A tests/shell/bluetooth_applet/run_installed_bluetooth_applet.cmake
A tests/shell/bluetooth_applet/tst_bluetooth_applet_controller.cpp
A tests/shell/bluetooth_applet/tst_bluetooth_applet_presentation.cpp
A tests/shell/bluetooth_applet/tst_bluetooth_applet_qml.cpp
A tests/shell/bluetooth_applet/tst_bluetooth_request_state.cpp
A tests/shell/qml/imports/QindaQt/Shell/BluetoothApplet/BluetoothApplet.qml
A tests/shell/qml/imports/QindaQt/Shell/BluetoothApplet/qmldir
```

Manager-owned `docs/HANDOFF.md`, `docs/TASK_LIST.md`, `ops/team/queues/**`, and
`ops/team/features.json` are unchanged relative to integrated main.

## Exact executable evidence

Both profiles used `BUILD_TESTING=ON`, production shell and shell `ON`, KWin
plugin and host-uinput tests `OFF`, `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, the
declared Qt/KF/LayerShellQt prefixes, and `--parallel 1` throughout.

- Fresh Debug root `/tmp/qindaqt-bluetooth-b1-debug-5714b2f`: configure exit 0
  with GCC 15.3.0; all nine requested production/focused targets ultimately
  built after every intermediate strict-warning/test red was recorded and
  repaired in non-amended commits.
- Debug exact controller replay: registered row 1/1 and nine QtTest cases
  passed after the two manager-audit repairs.
- Debug `ctest --parallel 1 --stop-on-failure -R
  '^qindaqt\.bluetooth-applet-'`: exit 0, 7/7.
- Debug adjacent `ctest --parallel 1 --stop-on-failure -R
  '^(qindaqt\.(bluetooth-client|applet-manifest|applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|shell-runtime-catalog))$'`:
  exit 0, 6/6.
- Fresh Release root `/tmp/qindaqt-bluetooth-b1-release-cedb13d`: configure
  exit 0 with GCC 15.3.0; the same exact requested target graph built 378/378.
- Release identical B1 selector: exit 0, 7/7.
- Release identical adjacent selector: exit 0, 6/6.

The B1 selector covers pure projection/request state, exact-owner controller,
compiled offscreen keyboard/accessibility and deferred lease close, two
mutation-sensitive source boundaries with poison controls, and the relocated
installed package. The adjacent selector covers the public client and every
manifest/catalog/resolver/dispatcher seam touched by the built-in addition.

## Exact static, docs, and provenance evidence

- Direct `check_boundary.cmake`: exit 0, five-file pure boundary plus poison
  rejection.
- Direct `check_runtime_boundary.cmake`: exit 0, seven-file runtime boundary
  plus service/pairing poison rejection.
- `jq` attempt: exit 127 because the tool is absent; no source gate ran.
  Available fallback `python3 -m json.tool` parsed `bluetooth.json`, the stock
  profile, and `ops/team/features.json`: exit 0, 3/3.
- `tools/validate-docs`: exit 0, 117 Markdown documents and MkDocs navigation.
- Pinned `/tmp/qindaqt-mkdocs-venv2/bin/mkdocs build --strict`: exit 0.
- `python3 tools/check-source-shape --largest 15`: exit 0, 1,779 files; only
  the two pre-existing unrelated 500/539-line warnings remain. Every B1 source
  stays below the threshold.
- `git diff --check`, cached diff check, conflict-marker scan, exact 71-path
  inventory, integrated-main merge-base, preserved-milestone ancestry, and
  prohibited-manager-path audit: exit 0.
- Candidate worktree was clean at freeze. Final process inspection found no
  compiler, CMake, Ninja, CTest, KWin, Weston, production shell, or Bluetooth
  test process beyond the inspection shell.

## Bounded caveats and requested review

This is deterministic public-client/fake-transport, offscreen, compiled-QML,
source-poison, and relocated-package evidence. It does not claim a production
BluezQt adapter, live BlueZ, host or physical Bluetooth discovery/connection,
hotplug, suspend/resume, pairing, trust, keys, Agent1 prompts, Bluetooth audio
correlation/routing, host session-bus behavior, AT-SPI bridge behavior,
nested-compositor interaction, multi-user policy, or physical/hardware
qualification. No private D-Bus, nested compositor, BlueZ, host Bluetooth,
hardware, network, or input lane was used.

Please assign independent workers to review exact immutable candidate
`ecadc745fdea1e22cbbcfcbcbab1738507231b8c`, report P0/P1/P2/P3 findings, and
replay at least the manager-audit controller mutations, focused B1 selector,
adjacent selector, boundary poisons, and installed-stage proof. Per current
Program Manager direction, the requested independent set is Kimi K3-256k,
Kimi K2.7, Claude Sonnet, and Claude Opus. Return any blocking reproduction to
this worktree; otherwise route the accepted exact candidate to manager
integration without rebasing or cherry-picking its preserved merge lineage.
