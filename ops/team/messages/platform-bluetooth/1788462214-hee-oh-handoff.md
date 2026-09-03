# Hee Oh — Bluetooth pairing bounded-repair handoff

- Timestamp: `2026-09-03T13:03:34-06:00`
- Candidate commit: `7025a1cab90419baf07431e5880bd40ebee2afac`
- Candidate tree: `f1cef09db3fc97bbf91756948c3e05fa3e3f8bcb`
- Rejected candidate repaired: `43a7cb16d4d053b1e05ba4351d986b235676cbde`
- Exact base: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Branch: `worker/bluetooth-pairing`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-pairing`

## Outcome

Bluetooth1 retains its original schema-1 structures, methods, object path, and
interface. The same activated owner now also registers additive Bluetooth2,
whose schema-2 prompt contains a nonzero identity and whose four reply methods
require that identity. Service, model, client, Settings, and BlueZ backend all
carry and reject mismatched identities, including a private-bus prompt-A versus
prompt-B regression.

Agent1 registration is conditional on an exact BlueZ owner plus an adapter and
is explicitly unregistered on final-adapter loss and shutdown. Settings and the
applet dispatch cancellation from a window-scoped Escape shortcut. Stable
cancel reasons now use `pairing-cancelled` and `prompt-cancelled`, and the
production direct-QtDBus maturity documentation is current.

## Changed paths

- `docs/wiki/adr/0037-keep-pairing-and-trust-authority-in-bluez.md`
- `docs/wiki/adr/0057-reach-bluez-through-direct-qtdbus-behind-adapter-backend.md`
- `docs/wiki/apps/bluetooth-settings.md`
- `docs/wiki/architecture/bluetooth-service.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `docs/wiki/reference/bluetooth1-v1.md`
- `docs/wiki/reference/bluetooth2-v2.md`
- `docs/wiki/shell/bluetooth-applet.md`
- `mkdocs.yml`
- `src/apps/settings/bluetooth/bluetooth_settings_pairing.cpp`
- `src/apps/settings/bluetooth/qml/BluetoothPairingSection.qml`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_backend.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_publication.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_pairing_agent.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_pairing_agent.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_pairing_operations.cpp`
- `src/services/bluetooth_client/src/bluetooth_client_pairing.cpp`
- `src/services/bluetooth_client/src/qt_bluetooth_transport.cpp`
- `src/services/bluetooth_model/include/qindaqt/services/bluetooth_model/adapter_backend.h`
- `src/services/bluetooth_model/src/bluetooth_model.cpp`
- `src/services/bluetooth_model/src/bluetooth_model_publication.cpp`
- `src/services/bluetooth_model/src/deterministic_adapter_backend.cpp`
- `src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_dbus.h`
- `src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_limits.h`
- `src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_types.h`
- `src/services/bluetooth_protocol/src/bluetooth_dbus.cpp`
- `src/services/bluetooth_protocol/src/bluetooth_validation.cpp`
- `src/services/bluetooth_service/CMakeLists.txt`
- `src/services/bluetooth_service/data/org.qindaqt.Bluetooth1.xml`
- `src/services/bluetooth_service/data/org.qindaqt.Bluetooth2.xml`
- `src/services/bluetooth_service/include/qindaqt/services/bluetooth_service/resident_bluetooth_service.h`
- `src/services/bluetooth_service/src/bluetooth1_service_object.cpp`
- `src/services/bluetooth_service/src/bluetooth1_service_object_p.h`
- `src/services/bluetooth_service/src/bluetooth_service_object.cpp`
- `src/services/bluetooth_service/src/bluetooth_service_object_p.h`
- `src/services/bluetooth_service/src/resident_bluetooth_service.cpp`
- `src/shell/bluetooth_applet/qml/BluetoothApplet.qml`
- `tests/apps/settings/bluetooth/tst_bluetooth_page.cpp`
- `tests/apps/settings/bluetooth/tst_bluetooth_settings_model.cpp`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez.cpp`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez.h`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez_pairing.cpp`
- `tests/services/bluetooth_bluez_adapter/tst_bluez_pairing.cpp`
- `tests/services/bluetooth_client/tst_bluetooth_client.cpp`
- `tests/services/bluetooth_client/tst_qt_bluetooth_transport.cpp`
- `tests/services/bluetooth_protocol/tst_bluetooth_protocol.cpp`
- `tests/services/bluetooth_service/installed_consumer/installed_consumer.cpp`
- `tests/services/bluetooth_service/run_staged_install.cmake`
- `tests/services/bluetooth_service/tst_bluetooth_service.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_controller.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_qml.cpp`

## Acceptance evidence

- Debug and Release configuration with
  `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake`,
  strict warnings, testing, KWin plugin, shell, production shell, and host-uinput
  disabled: exit 0 in both profiles.
- The brief's exact 24-target focused build command under
  `/home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/{debug,release}`
  with `--parallel 3`: final exit 0 in both profiles.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/debug -R 'bluetooth' --output-on-failure --no-tests=error`:
  exit 0, 31/31 passed.
- The identical command against `release`: exit 0, 31/31 passed.
- `./tools/validate-docs`: exit 0, 140 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0, 2,421 files checked; all touched
  production/test sources remain below the hard limits. Warnings are confined
  to pre-existing review-threshold files, with touched Bluetooth model and fake
  files at 499 nonblank lines.
- `git diff --check` before commit and
  `git diff --check 43a7cb16d4d053b1e05ba4351d986b235676cbde..7025a1cab90419baf07431e5880bd40ebee2afac`:
  exit 0 with no output.
- Kathrin's five `contract_checks.py` modes, run unchanged except for replacing
  the reviewed worktree path with this repair worktree: all five exit 0.
- No JSON changed; the JSON parser gate is not applicable.

During repair, an initial directly affected Debug subset exposed and then
repaired eager prompt clearing before `Device1.CancelPairing`; its rerun passed
8/8. A later experimental integer `Qt.Key_Escape` shortcut compiled but caused
the Settings and applet offscreen rows to fail; it was replaced by the verified
QKeySequence form, the focused rerun passed 2/2 in both profiles, and the final
full results above supersede those diagnostic iterations. One intermediate
aggregate-initializer edit also failed the strict Debug build on
`-Wmissing-field-initializers`; the final focused builds above compile its
complete replacement with warnings as errors.

## Bounded caveats

This candidate deliberately claims only deterministic model/offscreen/package
and injected private-bus fake-BlueZ evidence. It does not claim host D-Bus,
physical-radio or real-device pairing interoperability, a distribution BlueZ
build, hotplug/suspend behavior, Bluetooth audio routing, uinput, AT-SPI, or a
nested compositor. None of those rows or resources was touched.

Requested next action: **independent exact review then manager integration**.
