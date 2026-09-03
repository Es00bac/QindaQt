# Power PB-2 restart-generation repair handoff

- **Worker:** Anita Borg (OpenAI Codex `gpt-5.6-sol`, reasoning high)
- **Feature:** QQ-005.03 Power status/actions and coherent brightness (PB-2)
- **Exact candidate commit:** `6cef8b582aeb33522829d6ae838269f31aad7611`
- **Exact candidate tree:** `cc525f21716fa2f491dc7b0306d05526eae5ad50`
- **Candidate parent:** `28abb738051fa975b901585ade4485e237ad12b5`
- **Rejected ancestor:** `92d9dec8fd89e539802bf1f89223b1deae0614e6`
- **Exact base:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Branch:** `worker/power-pb2-upower`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/power-pb2-upower`

## Outcome

The dispatch-time authorization callback now rejects its captured generation
before it consults or removes the operation-ID-keyed current-run map. A delayed
generation-1 `Can*` reply is therefore inert after stop/start and cannot erase
a generation-2 operation that legitimately reuses the ID. The registered
private-bus regression proves generation 1 completes exactly once as
`Uncertain` on `stop()`, generation 2 completes exactly once as `Succeeded`,
and exactly one `PowerOff(false)` call reaches the fake authority.

The adjacent pending-state audit found no analogous ordering defect. UPower
device replies mutate only an identity-unique `RefreshCycle` after both
current-cycle identity and generation checks. Power Profiles refresh replies
check generation and refresh serial before active-state mutation, and hold
acquisition/release replies check generation before changing the cookie map.

## Repair-commit paths

- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `src/services/power_service/src/adapters/logind_action_authority.cpp`
- `tests/services/power_service/support/fake_logind_service.cpp`
- `tests/services/power_service/support/fake_logind_service.h`
- `tests/services/power_service/tst_power_logind_actions.cpp`

## Sorted paths changed from exact base through candidate

- `docs/wiki/adr/0056-confine-production-power-upstreams.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `mkdocs.yml`
- `ops/team/messages/platform-power-brightness/1788408840-anita-borg-pb2-resume-claim.md`
- `ops/team/messages/platform-power-brightness/1788411291-anita-borg-pb2-midpoint.md`
- `ops/team/messages/platform-power-brightness/1788412016-anita-borg-handoff.md`
- `ops/team/messages/platform-power-brightness/1788413265-anita-borg-pb2-repair-claim.md`
- `ops/team/messages/platform-power-brightness/1788413975-anita-borg-pb2-repair-handoff.md`
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

Ida Holz's exact standalone reproduction was compiled and run before the
repair under the assigned build root. Compilation exited `0`; execution exited
`1` with:

```text
firstGenerationFinishes=1 secondGenerationFinishes=0 actionCalls=0
```

The same source was recompiled against the repair. Compilation and execution
both exited `0` with:

```text
firstGenerationFinishes=1 secondGenerationFinishes=1 actionCalls=1
```

The exact compile/run shape was:

```text
/usr/lib64/qt6/libexec/moc -I src/services/power_service/include -I src/services/power_protocol/include src/services/power_service/include/qindaqt/services/power_service/adapters/logind_action_authority.h -o /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/moc_logind_action_authority_repro.cpp
c++ -std=c++20 -fPIC -pie -I src/services/power_service/include -I src/services/power_protocol/include $(pkg-config --cflags Qt6Core Qt6DBus) /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_restart_generation.cpp src/services/power_service/src/adapters/logind_action_authority.cpp /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/moc_logind_action_authority_repro.cpp $(pkg-config --libs Qt6Core Qt6DBus) -o /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/logind_restart_generation_<before-or-after>
/home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/logind_restart_generation_<before-or-after>
```

The full strict focused and dependency-adjacent target command exited `0` in
both Debug and Release:

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<debug-or-release> --parallel 3 --target qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests qindaqt_power_aggregation_tests qindaqt_power_client_tests qindaqt_power_qt_transport_tests qindaqt_power_activation_tests qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests qindaqt_power_sysfs_backlight_tests qindaqt_power_upstream_composition_tests qindaqt_power_upower_adapter_tests qindaqt_power_profiles_adapter_tests qindaqt_power_logind_adapter_tests qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests qindaqt-shell
```

The required isolated selector passed 26/26 in Debug and 26/26 in Release,
each with exit `0`:

```text
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent bwrap --bind / / --bind /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<profile>/ctest-tmp /tmp --dev-bind /dev /dev --proc /proc -- ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<profile> -R '^qindaqt\.power-' --output-on-failure --no-tests=error
```

The mount namespace redirects the legacy row's absolute `/tmp` fixture into
the assigned build root. No product test wrote host `/tmp` or contacted an
ambient system bus.

The exact logind-action row was additionally repeated until failure 10 times
per profile; both Debug and Release commands exited `0` after all 10 runs:

```text
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/<profile> -R '^qindaqt\.power-service-logind-actions$' --repeat until-fail:10 --output-on-failure --no-tests=error
```

Static gates on the product tree:

- `./tools/validate-docs` — exit `0`; 117 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-pb2-upower/site` — exit `0`; built in 1.27 seconds.
- `./tools/check-source-shape` — exit `0`; 1,791 files checked, zero skipped; only the two pre-existing unrelated 500/539-line warnings were emitted.
- `git diff --check` and `git diff --cached --check` — exit `0`.
- `git diff --name-only -- '*.json'` — exit `0`, no output; no JSON parser gate applied.

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
`6cef8b582aeb33522829d6ae838269f31aad7611`, then the Program Manager should
integrate it if accepted.
