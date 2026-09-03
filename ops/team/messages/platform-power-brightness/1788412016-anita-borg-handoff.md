# Power PB-2 production-upstream candidate handoff

- **Time:** 2026-09-02T23:06:56-06:00
- **Worker:** Anita Borg (OpenAI Codex, `gpt-5.6-sol`, reasoning high)
- **Candidate commit:** `f93effea182abcb50dd3dfb9dd6b8906839d4e18`
- **Candidate tree:** `b742207b9d4e3d0a651c5d15cd0b56a540009934`
- **Exact base:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Preserved outage WIP:** `661ce14b5ef2a5194b2c1eecf54271281789793b`
- **Branch:** `worker/power-pb2-upower`

## User-visible outcome

The resident Power1 service now has explicit production collaborators for
UPower, modern and legacy power-profiles-daemon, logind session/action truth,
and an injected-root sysfs backlight source. D-Bus refreshes are atomic and
pinned to resolved unique owners; malformed input, daemon loss, replacement,
and stale replies fail closed. The packaged activation files opt into
production while a bare executable remains unavailable by default. Tests use
private D-Bus daemons and build-root fixtures only.

## Changed paths (sorted)

- `docs/wiki/adr/0056-confine-production-power-upstreams.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `mkdocs.yml`
- `src/services/power_service/CMakeLists.txt`
- `src/services/power_service/app/main.cpp`
- `src/services/power_service/data/org.qindaqt.Power1.service.in`
- `src/services/power_service/data/qindaqt-power-service.service.in`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/logind_action_authority.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/logind_session_collaborator.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/power_profiles_collaborator.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/production_battery_collaborator.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/sysfs_backlight_source.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/upower_battery_collaborator.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/upstream_composition.h`
- `src/services/power_service/include/qindaqt/services/power_service/power_collaborators.h`
- `src/services/power_service/src/adapters/logind_action_authority.cpp`
- `src/services/power_service/src/adapters/logind_session_collaborator.cpp`
- `src/services/power_service/src/adapters/power_profiles_collaborator.cpp`
- `src/services/power_service/src/adapters/production_battery_collaborator.cpp`
- `src/services/power_service/src/adapters/sysfs_backlight_source.cpp`
- `src/services/power_service/src/adapters/upower_battery_collaborator.cpp`
- `src/services/power_service/src/adapters/upower_device_decoder.cpp`
- `src/services/power_service/src/adapters/upower_device_decoder_p.h`
- `src/services/power_service/src/adapters/upstream_composition.cpp`
- `src/services/power_service/src/adapters/upstream_dbus_util.cpp`
- `src/services/power_service/src/adapters/upstream_dbus_util.h`
- `src/services/power_service/src/adapters/upstream_identity.cpp`
- `src/services/power_service/src/adapters/upstream_identity.h`
- `src/services/power_service/src/power_service_assembly.cpp`
- `tests/services/power_service/CMakeLists.txt`
- `tests/services/power_service/check_boundary.cmake`
- `tests/services/power_service/support/fake_logind_service.cpp`
- `tests/services/power_service/support/fake_logind_service.h`
- `tests/services/power_service/support/fake_ppd_service.cpp`
- `tests/services/power_service/support/fake_ppd_service.h`
- `tests/services/power_service/support/fake_upower_service.cpp`
- `tests/services/power_service/support/fake_upower_service.h`
- `tests/services/power_service/support/private_bus.h`
- `tests/services/power_service/tst_power_logind_actions.cpp`
- `tests/services/power_service/tst_power_logind_adapter.cpp`
- `tests/services/power_service/tst_power_production_activation.cpp`
- `tests/services/power_service/tst_power_profiles_adapter.cpp`
- `tests/services/power_service/tst_power_sysfs_backlight.cpp`
- `tests/services/power_service/tst_power_upower_adapter.cpp`
- `tests/services/power_service/tst_power_upstream_composition.cpp`

The candidate also removes the root-level `probe2`-`probe7` binaries, sources,
and generated moc files introduced only as disposable debugging artifacts in
the preserved outage WIP. They are absent from the base-to-candidate path list
and remain recoverable from `661ce14`.

## Acceptance evidence

The preserved unrepaired tree first ran the seven new service rows with:

```text
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/debug -R '^qindaqt\.power-service-(sysfs-backlight|upstream-composition|upower-adapter|profiles-adapter|logind-adapter|logind-actions|production-activation)$' --output-on-failure --no-tests=error
```

Exit `8`: 3/7 passed and the UPower, profiles, logind, and production-activation
rows failed or timed out. This is the negative baseline for the repaired
hostile/integration coverage.

Both exact lane configure recipes completed with exit `0`:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

The final focused/dependency-adjacent build command completed with exit `0` in
both Debug and Release (final invocation rebuilt one stale transport link in
each configuration):

```text
cmake --build <ROOT>/<debug-or-release> --parallel 3 --target qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests qindaqt_power_aggregation_tests qindaqt_power_client_tests qindaqt_power_qt_transport_tests qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests qindaqt_power_sysfs_backlight_tests qindaqt_power_upstream_composition_tests qindaqt_power_upower_adapter_tests qindaqt_power_profiles_adapter_tests qindaqt_power_logind_adapter_tests qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests qindaqt-shell
```

The broadest safe power selection completed with exit `0`, 25/25 passed in
Debug and 25/25 passed in Release:

```text
ctest --test-dir <ROOT>/<debug-or-release> -R '^qindaqt\.power-' -E '^qindaqt\.power-activation$' --output-on-failure --no-tests=error
```

This includes all 11 `qindaqt.power-service-*` rows, production activation on
an explicitly addressed private bus, protocol/model/client transports,
installed-package rows, and all power-applet consumers.

Static gates:

- `./tools/validate-docs` — exit `0`; 117 Markdown documents validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/site` — exit `0`.
- `./tools/check-source-shape` — exit `0`; 1,791 source files checked, zero skipped; only pre-existing unrelated threshold warnings.
- `git diff --check` — exit `0`.
- No JSON file changed, so no JSON parser gate applied.

## Bounded caveats

- No host system/session D-Bus, `/sys/class/backlight`, hardware, polkit,
  uinput, network, compositor, or nested desktop session was used or is claimed.
- `qindaqt.power-activation` was deliberately excluded because its read-only,
  out-of-lane source hardcodes `QTemporaryDir` under `/tmp`; the lane contract
  prohibits any `/tmp` use. The owned production-activation replacement row
  passed in both configurations using the assigned build root.
- Runtime compatibility is proved against exact private-bus fake contracts,
  including owner churn and malformed input, not against daemons or hardware
  on this host.
- PB-3 still owns shell session-action presentation. Keyboard-backlight work,
  KWin adaptive/external-brightness authority, and a public display-brightness
  mutation remain intentionally outside PB-2.

## Requested next action

Independent exact review of candidate `f93effea182abcb50dd3dfb9dd6b8906839d4e18`,
then Program Manager integration.
