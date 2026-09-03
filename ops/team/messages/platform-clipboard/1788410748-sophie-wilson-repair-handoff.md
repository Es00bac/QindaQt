# Sophie Wilson — Clipboard C1 repair handoff

- Timestamp: 2026-09-02T22:45:48-06:00
- Exact candidate commit: `63e884cfa2216d7dc492407e30c7ce28b8512ac0`.
- Exact candidate tree: `9dac14a4521ebf8c4e50a90004865d4dcf943da7`.
- Exact lane base: `ce9228d9694622d503d92a38d01986f8f124f188`.
- Rejected product ancestor: `405577cc964dd1282a9210af282650a132391056`.
- Repair starting tip: `8db8ed1f3a35991a690c10ad0d8303c7599791f0`.
- Branch: `worker/clipboard-service-c1`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-service-c1`.

## Outcome

- Both shipped schemas now default `services.clipboardHistory` off. The host also
  accepts Boolean `true` only when the same Settings1 snapshot attributes the key
  to `user-overrides`; system/profile defaults, missing truth, and malformed truth
  remain denial.
- Each admitted D-Bus caller retains the newest 64 mutation results with FIFO
  eviction. Fresh ids remain live past the ceiling, while retained ids preserve
  exact replay/conflict semantics. The caller ceiling is exactly 64 and a 65th
  simultaneous caller receives `Busy`/`caller-cache-full` without allocation.
- The data-control adapter rejects an offer before reading when it advertises more
  than 64 MIME names or any name beyond 127 code units, retains at most 16 pending
  offers with oldest-first destruction, and withdraws availability/capture on
  manager-global removal or compositor disconnect.

## Changed product paths

- `data/settings/schema-v1.json`
- `data/settings/schema-v2.json`
- `docs/wiki/adr/0056-isolate-clipboard-capture-in-a-volatile-host.md`
- `docs/wiki/architecture/clipboard-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/clipboard1-v1.md`
- `docs/wiki/reference/settings1-v1.md`
- `src/services/clipboard_service/CMakeLists.txt`
- `src/services/clipboard_service/app/clipboard_history_consent.cpp`
- `src/services/clipboard_service/app/clipboard_history_consent.h`
- `src/services/clipboard_service/app/main.cpp`
- `src/services/clipboard_service/src/clipboard_service_object.cpp`
- `src/services/clipboard_service/src/clipboard_service_object_p.h`
- `src/services/clipboard_wayland_adapter/include/qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h`
- `src/services/clipboard_wayland_adapter/src/qt_wayland_clipboard_adapter.cpp`
- `tests/services/clipboard_service/CMakeLists.txt`
- `tests/services/clipboard_service/tst_clipboard_request_cache.cpp`
- `tests/services/clipboard_service/tst_clipboard_settings_consent.cpp`
- `tests/services/clipboard_wayland_adapter/support/fake_data_control_server.cpp`
- `tests/services/clipboard_wayland_adapter/support/fake_data_control_server.h`
- `tests/services/clipboard_wayland_adapter/tst_clipboard_wayland_adapter.cpp`

## Executed evidence

The assigned Debug and Release trees were already configured with strict warnings
and the pinned KWin 6.6.5 cache, so they were reused as directed.

- Debug focused build:
  `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug --parallel 3 --target qindaqt-clipboard-host qindaqt_clipboard_protocol_tests qindaqt_clipboard_client_tests qindaqt_clipboard_service_tests qindaqt_clipboard_private_bus_tests qindaqt_clipboard_request_cache_tests qindaqt_clipboard_settings_consent_tests qindaqt_clipboard_wayland_adapter_tests`
  — exit 0.
- Debug Clipboard selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error`
  — exit 0, 14/14 passed.
- A first Debug Settings selector invocation before its executables were built
  exited 8: 1/23 passed and 22 were not run because their executable paths did
  not yet exist. This was a build-closure discovery, not a test failure. The
  exact consumer targets were then built and the identical selector reran green.
- Debug Settings/consumer build closure:
  `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug --parallel 3 --target qindaqt_settings_schema_tests qindaqt_layered_settings_tests qindaqt_settings_persistence_tests qindaqt_settings_migration_tests qindaqt_settings_protocol_tests qindaqt_settings_protocol_dbus_tests qindaqt_settings_repository_tests qindaqt_settings_service_lifecycle_tests qindaqt_settings_service_process_lifecycle_tests qindaqt_settings_client_tests qindaqt_settings_commit_reply_validation_tests qindaqt_do_not_disturb_controller_tests qindaqt_qt_settings_transport_tests qindaqt_qt_settings_transport_adversarial_tests qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test qindaqt-settings qindaqt_appearance_values_tests qindaqt_appearance_preview_tests qindaqt_appearance_model_tests qindaqt_appearance_page_tests qindaqt_notification_quieting_bridge_tests`
  — exit 0.
- Debug Settings selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug -R '^qindaqt\.settings' --output-on-failure --no-tests=error`
  — exit 0, 23/23 passed.
- Debug appearance selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug -R '^qindaqt\.appearance-(values|preview|settings-model|page)$' --output-on-failure --no-tests=error`
  — exit 0, 4/4 passed.
- Debug notification selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug -R '^qindaqt\.(notification-quieting-settings-bridge|notification-quieting-controls-offscreen|notification-(surfaces|focus)-offscreen)$' --output-on-failure --no-tests=error`
  — exit 0, 4/4 passed.
- Release focused build:
  `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release --parallel 3 --target qindaqt-clipboard-host qindaqt_clipboard_protocol_tests qindaqt_clipboard_client_tests qindaqt_clipboard_service_tests qindaqt_clipboard_private_bus_tests qindaqt_clipboard_request_cache_tests qindaqt_clipboard_settings_consent_tests qindaqt_clipboard_wayland_adapter_tests`
  — exit 0.
- Release Clipboard selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error`
  — exit 0, 14/14 passed.
- Release Settings/consumer build closure:
  `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release --parallel 3 --target qindaqt_settings_schema_tests qindaqt_layered_settings_tests qindaqt_settings_persistence_tests qindaqt_settings_migration_tests qindaqt_settings_protocol_tests qindaqt_settings_protocol_dbus_tests qindaqt_settings_repository_tests qindaqt_settings_service_lifecycle_tests qindaqt_settings_service_process_lifecycle_tests qindaqt_settings_client_tests qindaqt_settings_commit_reply_validation_tests qindaqt_do_not_disturb_controller_tests qindaqt_qt_settings_transport_tests qindaqt_qt_settings_transport_adversarial_tests qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test qindaqt-settings qindaqt_appearance_values_tests qindaqt_appearance_preview_tests qindaqt_appearance_model_tests qindaqt_appearance_page_tests qindaqt_notification_quieting_bridge_tests`
  — the first invocation's PTY completion status was unavailable after its session
  expired; the identical command was rerun and exited 0 with `ninja: no work to do`.
- Release Settings selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release -R '^qindaqt\.settings' --output-on-failure --no-tests=error`
  — exit 0, 23/23 passed.
- Release appearance selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release -R '^qindaqt\.appearance-(values|preview|settings-model|page)$' --output-on-failure --no-tests=error`
  — exit 0, 4/4 passed.
- Release notification selector:
  `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release -R '^qindaqt\.(notification-quieting-settings-bridge|notification-quieting-controls-offscreen|notification-(surfaces|focus)-offscreen)$' --output-on-failure --no-tests=error`
  — exit 0, 4/4 passed.
- `./tools/validate-docs` — exit 0; 118 Markdown documents and `mkdocs.yml`
  navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/site`
  — exit 0.
- `./tools/check-source-shape` — exit 0; 1,806 files checked. It reported only
  the two pre-existing review-threshold warnings in `tests/compositor/CMakeLists.txt`
  and `tests/services/display_color_model/tst_color_model.cpp`.
- `git diff --check` — exit 0.
- `python3 -m json.tool data/settings/schema-v1.json > /dev/null` — exit 0.
- `python3 -m json.tool data/settings/schema-v2.json > /dev/null` — exit 0.
- `git diff --cached --check` — exit 0 before the product commit.
- `git show --check --oneline --stat 63e884cfa2216d7dc492407e30c7ce28b8512ac0`
  — exit 0.

## Bounded caveats

- No nested compositor, host desktop/session bus, hardware, uinput, or network
  evidence is claimed. Runtime coverage uses only an in-process fake Wayland
  server and ephemeral private `dbus-daemon` instances below the assigned build root.
- Exactly-once replay applies only while a request id remains among a caller's
  newest 64 retained results; the documented client rule remains never to infer
  replay safety after an uncertain timeout or eviction.
- This repair does not add a `wlr-data-control` fallback or persistence; both remain
  deliberately outside the accepted volatile Clipboard1 architecture.

## Requested next action

Evelyn Berezin (Moonshot Kimi K3-256k) should independently review the exact
candidate commit and tree above against every P1/P2/P3 verdict reproduction.
If accepted, the Program Manager should integrate that immutable candidate.
