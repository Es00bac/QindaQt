# Thelma Estrin — QQ-005.05 Bluetooth B1 candidate handoff

- Candidate commit: `f44919a52f67515779f887b8d54a9bb2a57b3c4b`
- Candidate tree: `33376e05a4174024c0cd240b1408b183d2c10ed4`
- Exact lane base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Preserved WIP ancestor: `f081a86`
- Branch: `worker/bluetooth-bluez-b1`

## Delivered outcome

Production `AdapterBackend` over an injected, exact-owner-fenced direct QtDBus
BlueZ transport. It maps bounded ObjectManager/Adapter1/Device1 truth, shares
one BlueZ discovery session across bounded caller-scoped references, executes
only power/discovery/paired connect-disconnect operations, retires truth and
pending work on owner replacement, and keeps pairing/trust/records in BlueZ.
The packaged service defaults to production; only exact environment mode
`deterministic` selects the B0 empty backend. ADR-0056 supersedes only
ADR-0037's BluezQt library choice, not its authority decision.

## Changed paths

- `docs/wiki/adr/0056-reach-bluez-through-direct-qtdbus-behind-adapter-backend.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/bluetooth-service.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/services/bluetooth_bluez_adapter/CMakeLists.txt`
- `src/services/bluetooth_bluez_adapter/include/qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h`
- `src/services/bluetooth_bluez_adapter/include/qindaqt/services/bluetooth_bluez_adapter/bluez_backend_mode.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_backend.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_backend_p.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_publication.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_backend_mode.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_object_store.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_object_store.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_transport.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_transport.h`
- `src/services/bluetooth_service/CMakeLists.txt`
- `src/services/bluetooth_service/app/main.cpp`
- `tests/CMakeLists.txt`
- `tests/services/bluetooth_bluez_adapter/CMakeLists.txt`
- `tests/services/bluetooth_bluez_adapter/check_boundary.cmake`
- `tests/services/bluetooth_bluez_adapter/check_boundary_negative.cmake`
- `tests/services/bluetooth_bluez_adapter/check_installed_boundary.cmake`
- `tests/services/bluetooth_bluez_adapter/support/bluez_compose.h`
- `tests/services/bluetooth_bluez_adapter/support/bluez_harness.h`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez.cpp`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez.h`
- `tests/services/bluetooth_bluez_adapter/support/private_bus.h`
- `tests/services/bluetooth_bluez_adapter/tst_bluez_adapter_backend.cpp`
- `tests/services/bluetooth_bluez_adapter/tst_bluez_adapter_operations.cpp`
- `tests/services/bluetooth_bluez_adapter/tst_bluez_backend_mode.cpp`

## Acceptance evidence

All commands ran from the lane worktree. No host bus, BlueZ, radio, rfkill,
hardware, uinput, network, or nested compositor was contacted.

- Exact Debug configure command from the worker brief: exit 0.
- Exact Release configure command from the worker brief: exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_bluez_adapter_tests qindaqt_bluez_adapter_operations_tests qindaqt_bluez_backend_mode_tests qindaqt_bluetooth_protocol_tests qindaqt_bluetooth_model_tests qindaqt_bluetooth_deterministic_backend_tests qindaqt_bluetooth_client_tests qindaqt_bluetooth_qt_transport_tests qindaqt_bluetooth_activation_tests qindaqt_bluetooth_service_tests qindaqt_bluetooth_lease_owner_loss_tests qindaqt-bluetooth-service`: exit 0; final incremental build completed 16/16 actions.
- Same focused target command under `<ROOT>/release`: exit 0; final incremental build completed 10/10 actions (the preceding complete Release build compiled the target closure under `-O3 -DNDEBUG` and strict warnings).
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.bluetooth-' -E '^qindaqt\.bluetooth-staged-install$' --output-on-failure --no-tests=error`: exit 0, 14/14 passed.
- Same selector under `<ROOT>/release`: exit 0, 14/14 passed.
- The included production-adapter subset `^qindaqt\.bluetooth-bluez-` is 6/6 in each final profile run, including the private-bus fake, operations/failure replies, explicit mode, staged B1 component, boundary, and hostile poison rows.
- `./tools/validate-docs`: exit 0, 117 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,780 files checked; largest owned production file is 499 nonblank lines. Two warnings refer only to pre-existing non-owned files.
- `git diff --check`: exit 0.
- No JSON changed, so no `python3 -m json.tool` row applied.

The initial Release build correctly failed on duplicate unnecessary Qt container
metatype declarations in the fake; those declarations were removed and the
complete focused Release build then passed. This failed intermediate command
is not acceptance evidence but is recorded for auditability.

## Bounded caveats

- The pre-existing `qindaqt.bluetooth-staged-install` row performs an
  unscoped whole-repository install. In both focused profiles it exits 8 at the
  first unrelated unbuilt artifact, `src/profiles/libqindaqt_profiles.a`.
  Building the default whole repository is prohibited by this lane and the
  script is outside owned paths. The new B1-owned
  `qindaqt.bluetooth-bluez-installed-boundary` stages only
  `QindaQtBluetoothB1` and passes in both profiles, proving the exact adapter
  archive and two-header public surface.
- Evidence is private-bus fake qualification only. This candidate does not
  claim a physical adapter, host/distribution BlueZ, suspend/resume, hardware
  hotplug, pairing Agent1 UX, Bluetooth audio routing, integrated session, or
  resource-budget qualification.

## Requested next action

Independent exact review of candidate `f44919a52f67515779f887b8d54a9bb2a57b3c4b`,
then Program Manager integration.
