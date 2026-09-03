# Power PB-2 review-repair handoff

- **Time:** 2026-09-02T23:39:35-06:00
- **Worker:** Anita Borg (OpenAI Codex, `gpt-5.6-sol`, reasoning high)
- **Repair candidate:** `92d9dec8fd89e539802bf1f89223b1deae0614e6`
- **Candidate tree:** `d5179ffeb04528a1e36028663f0f84b94a29d9ac`
- **Rejected product candidate:** `f93effea182abcb50dd3dfb9dd6b8906839d4e18`
- **Exact base:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Branch:** `worker/power-pb2-upower`

## Repaired outcome

UPower decoding now follows the installed upstream interface contract:
`Online` alone drives line-power truth, `PowerSupply=false` peripheral
batteries/UPS devices are excluded from the system-supply inventory, and
`IsPresent` is read only for batteries. The private fake covers online and
offline line power, a peripheral battery, an UPS without `IsPresent`, and a
mixed system inventory.

Every logind action now performs a fresh `Can*` query against the resolved
exact owner immediately before dispatch. A historical `yes` that flips to
`no` completes once as unsupported without any action call. Authorization and
execution stages share duplicate-ID, stop, and owner-replacement fencing.

The build-root production activation row now preserves the legacy row's
contract: descriptor-triggered name activation, exact unique-owner truth,
descriptor/unit contents, service exit when its constructing bus dies, and a
fresh owner, epoch, and exact service PID on an independent replacement bus.
The architecture page records that replacement contract and correctly marks
`power_idle` as pending.

## Changed paths from exact base (sorted)

- `docs/wiki/adr/0056-confine-production-power-upstreams.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `mkdocs.yml`
- `ops/team/messages/platform-power-brightness/1788408840-anita-borg-pb2-resume-claim.md`
- `ops/team/messages/platform-power-brightness/1788411291-anita-borg-pb2-midpoint.md`
- `ops/team/messages/platform-power-brightness/1788412016-anita-borg-handoff.md`
- `ops/team/workers/anita-borg.md`
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

## Acceptance evidence

The already-configured lane Debug and Release roots were reused. The complete
focused and dependency-adjacent target command exited `0` in each profile:

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<debug-or-release> --parallel 3 --target qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests qindaqt_power_aggregation_tests qindaqt_power_client_tests qindaqt_power_qt_transport_tests qindaqt_power_activation_tests qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests qindaqt_power_sysfs_backlight_tests qindaqt_power_upstream_composition_tests qindaqt_power_upower_adapter_tests qindaqt_power_profiles_adapter_tests qindaqt_power_logind_adapter_tests qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests qindaqt-shell
```

After the final owner-race hardening, the three affected targets were rebuilt
in each profile with exit `0`:

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<debug-or-release> --parallel 3 --target qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests qindaqt_power_upower_adapter_tests
```

The exact required selector passed `26/26` in Debug and `26/26` in Release,
each with exit `0` and `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

```text
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent bwrap --bind / / --bind /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<profile>/ctest-tmp /tmp --dev-bind /dev /dev --proc /proc -- ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<profile> -R '^qindaqt\.power-' --output-on-failure --no-tests=error
```

The mount namespace maps the legacy row's absolute `/tmp` fixture to the
assigned build root. Thus the complete selector ran without writing host
`/tmp`; the new production row itself uses `QINDAQT_TEST_SCRATCH_DIR` directly.

Ida's exact UPower reproduction binary against rejected `f93effe` was rerun
and exited `1` with `acPresent=0` and `hasSupply=1`. Recompiling the same source
against this repair under the assigned build root exited `0` with
`acPresent=1` and `hasSupply=0`. Ida's logind reproduction was rebuilt against
this repair under the assigned root and exited `0` with
`cachedPowerOff=1 dispatchedCalls=0`.

Static gates on the final tree:

- `./tools/validate-docs` — exit `0`; 117 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/site` — exit `0`; built in 1.27 seconds.
- `./tools/check-source-shape` — exit `0`; 1,791 files checked, zero skipped; only the two pre-existing unrelated 500/539-line warnings were emitted.
- `git diff --check` and `git diff --cached --check` — exit `0`.
- No JSON file changed; no JSON parser gate applied.

## Bounded caveats

- No host system/session bus, `/sys/class/backlight`, hardware, polkit, uinput,
  network, compositor, nested session, physical suspend/resume, or production
  daemon compatibility was exercised or is claimed.
- The logind action boundary remains private to the later PB-3 shell
  controller; Power1 v1 still exposes no session-action methods.
- Keyboard-backlight discovery, idle hints, KWin adaptive/external brightness,
  and public display-brightness mutation remain later slices.

## Requested next action

Ida Holz (OpenAI Codex) should independently recheck exact repair candidate
`92d9dec8fd89e539802bf1f89223b1deae0614e6`, then the Program Manager should
integrate it if accepted.
