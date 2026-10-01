# DPMS tail repair claim and exact write scope

Manager assigned selective recovery of five absent DPMS/per-source tail commits over ab764094, following audit commit 2442e8de. Retain integrated Power1/lock/privacy contracts; no unrelated fork/nightlight/shortcuts changes.

Exact paths from the five commits:

- `docs/wiki/architecture/desktop-controls.md`
- `docs/wiki/architecture/idle-policy.md`
- `docs/wiki/architecture/power-policy.md`
- `docs/wiki/architecture/power-service.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/apps/settings/power/CMakeLists.txt`
- `src/apps/settings/power/include/qindaqt/apps/settings_power/source_idle_display_settings.h`
- `src/apps/settings/power/power_route_composition.cpp`
- `src/apps/settings/power/qml/PowerIdleDisplaySection.qml`
- `src/apps/settings/power/source_idle_display_settings.cpp`
- `src/session/desktop_controls/CMakeLists.txt`
- `src/session/desktop_controls/app/main.cpp`
- `src/session/idle_policy/CMakeLists.txt`
- `src/session/idle_policy/include/qindaqt/session/idle_policy/attached_display_power_port.h`
- `src/session/idle_policy/include/qindaqt/session/idle_policy/display_off_stage.h`
- `src/session/idle_policy/include/qindaqt/session/idle_policy/source_preferences.h`
- `src/session/idle_policy/production/kwayland_dpms_controller.cpp`
- `src/session/idle_policy/production/kwayland_dpms_controller.h`
- `src/session/idle_policy/src/display_off_stage.cpp`
- `src/session/idle_policy/src/source_preferences.cpp`
- `src/session_supervisor/CMakeLists.txt`
- `src/session_supervisor/app/main.cpp`
- `src/session_supervisor/src/native_lock_composition.cpp`
- `src/session_supervisor/src/session_process_supervisor.cpp`
- `src/settings/include/qindaqt/settings/settings_migration.h`
- `src/settings/src/settings_migration.cpp`
- `tests/CMakeLists.txt`
- `tests/apps/settings/power/CMakeLists.txt`
- `tests/apps/settings/power/stub_power_settings_model.h`
- `tests/apps/settings/power/tst_power_page.cpp`
- `tests/apps/settings/power/tst_source_idle_display_settings.cpp`
- `tests/services/settings_service/tst_powerdevil_import.cpp`
- `tests/session/idle_policy/CMakeLists.txt`
- `tests/session/idle_policy/dpms_wayland_fixture.cpp`
- `tests/session/idle_policy/dpms_wayland_fixture.h`
- `tests/session/idle_policy/tst_display_off_stage.cpp`
- `tests/session/idle_policy/tst_kwayland_dpms_controller.cpp`
- `tests/settings/tst_settings_migration.cpp`

Additive documentation boundary updates may include module-boundaries and native-session-lock/Power Settings primary pages as required to make the new consumer truthful. Shared build/registration edits stay minimal. No deeper AGENTS.md exists under these source/test/docs paths (rg --files audit).

Read qinda machine policy directly: configured MAKEOPTS is -j24 -l24; builds must preserve it and await manager resource slot. Warm build exists at qinda ~/work_SPaC3/container-wm.worktrees/native-lock-runtime/build/native-lock-runtime; its mutable artifacts will not be copied or changed. Build new isolated source/build from inspected cache configuration.
