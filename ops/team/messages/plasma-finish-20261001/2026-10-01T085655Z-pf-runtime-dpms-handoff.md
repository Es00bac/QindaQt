# Native DPMS/per-source candidate handoff

- Worker: pf-runtime-sol-20261001
- Exact review candidate: **891092f66029fdffa50258e98b6d52d3ed61007b**
- Branch: worker/pf-runtime-20261001; pushed successfully to authoritative qinda container-wm hub
- Exact consumer base: ab7640944d67b49f5fcbd198a96ac6c70cad193c
- Requested next action: independent exact-commit review, then manager integration and affected gates; no installation or host display action

## Outcome

Selectively recover five absent DPMS/per-source idle Settings/runtime commits without merging the stale branch. Native qindaqt-session now composes independent admitted-FD display-off observation, pure AC/battery/low-battery source selection, authenticated Power1 lease receipts and current Ready Settings1 preferences. Settings edits each source with exact per-key commit/readback truth. Legacy v1 stored global minutes migrate without inventing absent values.

The recovery fixes two bounded production defects found during source review: normal supervisor teardown explicitly flushes retained-peer On before destroying DPMS; degraded retained Settings1 snapshots disarm the automatic stage immediately. Public lifetime comments state the deliberate final-On-after-revocation contract. Added actual Wayland revoked-peer/no-reopen and pure missing-confirmed-preference restore regressions. Focused Power Settings boundary now admits public LockPreferences and only the exact pure IdlePolicy source-selection header.

ADR0319 amends ADR0306's stage process location: stage core stays in IdlePolicy, production FD composition in qindaqt-session, Power1 remains state/inhibitor authority. Scope capability stays zero. Primary Power Settings, desktop-controls, native-lock, power-policy/service and module-boundary docs match this source boundary.

## Direct acceptance evidence

- Fresh detached qinda source: ~/work_SPaC3/container-wm.worktrees/pf-runtime-20261001; isolated build build/pf-runtime. Mutable original build was inspected for config only and never copied or changed.
- Configure: Debug, Ninja, BUILD_TESTING=ON, production shell ON, KWIN_PLUGIN=OFF, OBS/QindaLutris/SystemMonitor/Viewer OFF; exit 0.
- Read actual machine AGENTS and portageq MAKEOPTS: -j24 -l24. Root serialized the whole compiler run and private fixture slot.
- Cold targeted build and three bounded incremental builds: exit 0 each. Production qindaqt-session, qindaqt-desktop-controls and qindaqt-settings plus the selected fixture/test binaries compile. Logs: build/pf-runtime/configure.log, build.log, build-incremental.log, build-authority.log, build-final.log.
- Exact executable source c7e8fc3a04a56925703b4d2a62bfd78838dd83d1: **22/22 CTests pass**, QT_FATAL_WARNINGS=1, 30.04 seconds, exit 0. Twenty QtTest rows report **200 passes, zero failures/skips/blacklists**, including setup/cleanup; the two other rows are boundary/poison CMake checks.
- Fresh disposable XDG runtime/config/data/cache, offscreen GUI, host session/system bus addresses disabled; individual protocol rows own real private buses/Wayland server fixtures. No installed service or host config contacted.
- Test regex covers Settings schema/migration/PowerDevil import, source-idle model, Power Settings composition/page/boundary/poison, pure display stage, actual KWayland DPMS lifecycle/revoked peer, desktop shortcut set, PowerClient/Qt transport/residency/idle registry, native lock runtime/state/request/facade and ordinary compositor attachment/idle observation. Logs: build/pf-runtime/focused-tests.log and Testing/Temporary/LastTest.log.
- Candidate source/test bytes equal executable source c7e8fc3a (`git diff --quiet c7e8fc3a HEAD -- src tests`, exit 0). Candidate's final change is documentation only.
- Strict MkDocs: exit 0. tools/validate-docs: **477 Markdown documents/navigation**, exit 0. git diff --check: exit 0. New IdlePolicy focused shape: **8 files**, zero skip, maximum production source 307 nonblank lines, exit 0.

## Bounded caveats

This is native source behavior, not PF2–PF4 completion. PowerDevil remains an optional child and may retain independently configured legacy idle policy; removing desktop-controls' binding does not retire that competing policy. Brightness/lid/profile/critical/dim/lock-before-display-off/idle-suspend and complete inhibition/advertisement remain separate. ScreenSaver/portal inhibition still returns unsupported; scopes remain zero. Native surface inhibitors rely on compositor ext-idle semantics but combined nested qualification remains unrun.

The actual DPMS tests use an in-process real Wayland server, not a nested fork or physical display. Plugin-OFF build establishes no exact compositor ABI/source-pin gate. No package install, physical lock/sleep, real credentials, display power or source fork change occurred. Configure had preexisting installed GlobalMenu QML missing-file scanning and keyring qindatk-plugin warnings; compilation and focused tests passed, and no unrelated package qualification is inferred.

## Changed paths

- `docs/wiki/adr/0306-power1-idle-inhibitors-and-stage-policy.md`
- `docs/wiki/adr/0319-supervisor-owned-native-idle-display-policy.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/power-settings.md`
- `docs/wiki/architecture/desktop-controls.md`
- `docs/wiki/architecture/idle-policy.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/native-session-lock.md`
- `docs/wiki/architecture/power-policy.md`
- `docs/wiki/architecture/power-service.md`
- `mkdocs.yml`
- `ops/team/messages/plasma-finish-20261001/2026-10-01T083213Z-pf-runtime-claim.md`
- `ops/team/messages/plasma-finish-20261001/2026-10-01T083603Z-pf-runtime-dpms-claim.md`
- `ops/team/messages/plasma-finish-20261001/2026-10-01T083603Z-pf-runtime-source-audit.md`
- `ops/team/workers/pf-runtime-sol-20261001.md`
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
- `tests/apps/settings/power/check_boundary.cmake`
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

## Next capacity

Compiler and private fixture slots are released. After reading the Platform queue and peer handoff boundaries, offer focused exact-candidate review fixes or a manager-scoped nested DPMS qualification row using the accepted fork source. Remain waiting for explicit manager assignment; do not initiate missing power, shortcut, shell or deployment work.

Recorded 2026-10-01T08:56:55Z.
