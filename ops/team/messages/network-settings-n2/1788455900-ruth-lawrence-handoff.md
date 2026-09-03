# Ruth Lawrence — Network N3 handoff

- Time: `2026-09-03T11:18:20-06:00`
- Candidate commit: `afee87d54e8d8a3369ce21515ea173a3c19d8207`
- Candidate tree: `d0c3ac46998c02699bb2d10c7c1ee779ffb9cc2b`
- Exact base: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`

## Outcome

Network1 now admits the fixed `(epoch, revision, accessPointId)` visible-network intent only for an access point in authoritative snapshot truth and only with `VisibleNetworkControl`. The private libnm adapter creates and activates Open, WPA-PSK, or SAE profiles; secured profiles omit PSKs and mark them agent-owned. Hidden, WEP, enterprise, stale, absent, already-known, and capability-denied requests fail closed. Network Settings exposes an accessible Connect action for unsaved supported networks with exact secret-agent presence truth and no credential input.

## Changed paths

- `docs/wiki/adr/0055-compose-network-settings-through-network1.md`
- `docs/wiki/apps/network-settings.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/network1-v1.md`
- `src/apps/settings/network/CMakeLists.txt`
- `src/apps/settings/network/include/qindaqt/apps/settings_network/network_settings_model.h`
- `src/apps/settings/network/network_settings_actions.cpp`
- `src/apps/settings/network/network_settings_model.cpp`
- `src/apps/settings/network/qml/NetworkAccessPointSection.qml`
- `src/services/network_client/include/qindaqt/services/network_client/network_client.h`
- `src/services/network_client/src/network_client_admission.cpp`
- `src/services/network_manager_adapter/CMakeLists.txt`
- `src/services/network_manager_adapter/data/org.qindaqt.Network1.xml`
- `src/services/network_manager_adapter/include/qindaqt/services/network_manager_adapter/network_manager_facts.h`
- `src/services/network_manager_adapter/src/libnm_network_manager_facts.cpp`
- `src/services/network_manager_adapter/src/libnm_network_manager_port.cpp`
- `src/services/network_manager_adapter/src/libnm_network_manager_port_p.h`
- `src/services/network_manager_adapter/src/libnm_visible_network.cpp`
- `src/services/network_manager_adapter/src/network_manager_mapping.cpp`
- `src/services/network_model/include/qindaqt/services/network_model/network_intent_policy.h`
- `src/services/network_model/include/qindaqt/services/network_model/network_model.h`
- `src/services/network_model/src/network_intent_policy.cpp`
- `src/services/network_model/src/network_model.cpp`
- `src/services/network_protocol/include/qindaqt/services/network_protocol/network_identity.h`
- `src/services/network_protocol/include/qindaqt/services/network_protocol/network_intent.h`
- `src/services/network_protocol/include/qindaqt/services/network_protocol/network_types.h`
- `src/services/network_protocol/src/network_identity.cpp`
- `src/services/network_protocol/src/network_validation.cpp`
- `src/services/network_qt_transport/src/qt_network_transport.cpp`
- `src/services/network_service/src/network_service_coordinator.cpp`
- `src/services/network_service/src/network_service_object.cpp`
- `src/services/network_service/src/network_service_object_p.h`
- `tests/apps/settings/network/CMakeLists.txt`
- `tests/apps/settings/network/check_boundary.cmake`
- `tests/apps/settings/network/check_boundary_negative.cmake`
- `tests/apps/settings/network/network_settings_test_support.h`
- `tests/apps/settings/network/stub_network_settings_model.h`
- `tests/apps/settings/network/tst_network_page.cpp`
- `tests/apps/settings/network/tst_network_settings_model.cpp`
- `tests/apps/settings/network/tst_network_settings_model_adversarial.cpp`
- `tests/services/network_client/installed_cpp_consumer.cpp`
- `tests/services/network_client/tst_network_client.cpp`
- `tests/services/network_manager_adapter/CMakeLists.txt`
- `tests/services/network_manager_adapter/check_boundary.cmake`
- `tests/services/network_manager_adapter/tst_network_manager_adapter.cpp`
- `tests/services/network_manager_adapter/tst_network_visible_profile.cpp`
- `tests/services/network_model/tst_network_intent_policy.cpp`
- `tests/services/network_protocol/network_protocol_test_data.h`
- `tests/services/network_protocol/tst_network_codec.cpp`
- `tests/services/network_protocol/tst_network_identity.cpp`
- `tests/services/network_service/support/fake_network_backend.h`
- `tests/services/network_service/tst_network_residency.cpp`
- `tests/services/network_service/tst_network_service.cpp`

## Acceptance evidence

All commands below exited 0 unless explicitly noted.

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/network-join/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/network-join/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/network-join/debug --parallel 3 --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests qindaqt-network-service
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/network-join/debug --parallel 3 --target qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt-network-secret-agent
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/network-join/release --parallel 3 --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt-network-secret-agent qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests qindaqt-network-service
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/network-join/debug -R '^qindaqt\.(network-|settings-network-)' --output-on-failure --no-tests=error
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/network-join/release -R '^qindaqt\.(network-|settings-network-)' --output-on-failure --no-tests=error
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/network-join/site
./tools/check-source-shape
git diff --check
```

- Debug selector: 35/35 passed, exit 0.
- Release selector: 35/35 passed, exit 0.
- `qindaqt.network-page` ran with `QT_FATAL_WARNINGS=1` through its CTest environment.
- Documentation: 139 Markdown documents plus `mkdocs.yml` validated; strict MkDocs passed.
- Source shape: 2,410 files checked, zero errors (the command reports existing decomposition-review warnings outside owned paths).
- Two initial Release build invocations exited 1 before compilation because the requested target names `qindaqt_network_secret_prompt_tests` and `qindaqt_network_settings_adversarial_tests` do not exist; the corrected exact target command above completed all 327 steps successfully.

## Bounded caveats

- No host NetworkManager, hardware, radio, network, uinput, or nested compositor was contacted.
- The libnm profile test serializes the exact connection map in-process and the private-bus rows exercise Network1 dispatch; it deliberately does not claim a successful activation against a real or emulated NetworkManager daemon.
- Credential prompting remains exclusively the separately qualified secret-agent process; this candidate proves presence gating and secret-free handoff, not a live end-to-end password exchange.

Requested next action: independent exact review of `afee87d54e8d8a3369ce21515ea173a3c19d8207`, then Program Manager integration.
