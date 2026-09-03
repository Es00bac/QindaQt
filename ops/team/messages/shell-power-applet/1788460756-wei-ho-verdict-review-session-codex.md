# Wei Ho — independent session/shell review

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate: `437064325e7aa29b14a7dab230382ab947e71253`
- Tree: `5316d4a6a1b880ea5c9a691f9b12e76c65cb3e56`
- Parents: `6bffe7996a6ab44c6435311106d51c6a09b9c33c`, `f6ba3f8acfb38e9a8e9b8c75889138867a9271f5`
- Review base: `f6ba3f8acfb38e9a8e9b8c75889138867a9271f5`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/session-actions-codex-review` (detached, clean before and after)

## Findings ledger

### P0

None.

### P1

1. **An owned ScreenSaver bus name is treated as a usable lock interface.** `src/services/session_actions/src/session_actions_client.cpp:200-209` sets `lock=true` from the name owner alone; it never proves that `/ScreenSaver` implements `org.freedesktop.ScreenSaver.Lock`. Reproduction:

   ```text
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-session-codex/repro-client-fences/build/repro missing-interface
   exit 1: service-owned=1 object-registered=0 observed-canLock=1 request-accepted=1 observed-status=4 (Unavailable=2, Uncertain=4)
   ```

   Observed: the absent interface is advertised, the request is accepted, and its error becomes `Uncertain`. Expected: typed unavailable truth and local refusal when the ScreenSaver interface is absent.

2. **A former authority's delayed success is accepted after owner replacement.** `src/services/session_actions/src/session_actions_client.cpp:411-457` fences only before dispatch; the reply callback at lines 443-456 does not recheck the pinned owner. Reproduction:

   ```text
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-session-codex/repro-client-fences/build/repro owner-replaced
   exit 1: old-lock-calls=1 new-lock-calls=0 owner-replaced=1 observed-status=0 (Succeeded=0, Uncertain=4)
   ```

   Observed: `Succeeded` after `org.freedesktop.ScreenSaver` moved from the replying process to another owner. Expected: `Uncertain` with no replay, as required by `docs/wiki/adr/0070-confine-session-actions-behind-authenticated-boundaries.md:45-46`.

### P2

None.

### P3

1. `docs/wiki/adr/0070-confine-session-actions-behind-authenticated-boundaries.md:37-38` says direct login1/ScreenSaver symbols anywhere outside this client are boundary-test failures, but `tests/services/session_actions/check_boundary.cmake:9-15` scans only the session-action consumer roots. `rg -n --glob '!src/services/session_actions/**' 'org\.freedesktop\.login1|org/freedesktop/login1' src` finds the intentionally separate platform roles at `src/services/display_runtime/src/qt_session_safety_port.cpp:26-28` and `src/services/power_service/src/adapters/logind_action_authority.cpp:18-20`, while the registered boundary test passes. The ADR should scope the claim to those consumers.

## Executed evidence

`ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-session-codex`.

- Immutable pre/post checks: `git rev-parse HEAD HEAD^{tree} HEAD^1 HEAD^2 f6ba3f8`; `git status --porcelain`. Exit 0; SHA values are recorded above and status was empty both times. `git diff --stat f6ba3f8..4370643`: 57 files, 2,447 insertions, 100 deletions.
- Configure, once for each of `debug`/`Debug` and `release`/`Release`:

  ```text
  cmake -S . -B $ROOT/<config> -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=<type> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

  Both exit 0.

- Focused builds in both configurations:

  ```text
  cmake --build $ROOT/<config> --parallel 3 --target qindaqt-session qindaqt-network-secret-agent qindaqt-shell qindaqt-shell-preview qindaqt_global_menu_qmlplugin qindaqt_shell_launcher_qmlplugin qindaqt-desktop-session-probe qindaqt_session_supervisor_tests qindaqt_session_actions_tests qindaqt_shell_runtime_options_tests qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests qindaqt_power_aggregation_tests qindaqt_power_client_tests qindaqt_power_qt_transport_tests qindaqt_power_activation_tests qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests qindaqt_power_sysfs_backlight_tests qindaqt_power_upstream_composition_tests qindaqt_power_upower_adapter_tests qindaqt_power_profiles_adapter_tests qindaqt_power_logind_adapter_tests qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests qindaqt_power_settings_model_tests qindaqt_power_settings_slider_tests qindaqt_power_page_tests qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests
  ```

  Debug and Release exit 0, 1,690/1,690 Ninja edges. The first exact selector exposed three registered but unbuilt adjacent executables (39 passed, 3 Not Run; exit 8), so I built them in each configuration with `cmake --build $ROOT/<config> --parallel 3 --target qindaqt_session_lock_authentication_tests qindaqt_session_lock_transition_tests qindaqt_qt_session_lock_transport_tests` (12/12, exit 0) and reran the unchanged selector.

- Exact selector, Debug and Release:

  ```text
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir $ROOT/<config> -R '^qindaqt\.(session|power-|settings-power-|shell-runtime-)' --output-on-failure --no-tests=error
  ```

  Debug: 42/42 passed, exit 0, 65.26 s. Release: 42/42 passed, exit 0, 60.22 s. This includes the `QT_FATAL_WARNINGS=1` offscreen row via its registered environment.

- Installed rows, Debug and Release:

  ```text
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir $ROOT/<config> -R '^desktop\.virtual\.(package-contract|stage-closure)$' --output-on-failure --no-tests=error
  ```

  Debug 2/2 passed (8.92 s); Release 2/2 passed (7.56 s); both exit 0.

- Scratch probe configure/build: `cmake -S $ROOT/repro-client-fences -B $ROOT/repro-client-fences/build -G Ninja`; `cmake --build $ROOT/repro-client-fences/build --parallel 3`. Both exit 0. Probe results are in the P1 entries. Scratch stayed under the assigned build root.
- Static gates: `./tools/validate-docs` (141 documents, exit 0); `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir $ROOT/site` (exit 0); `./tools/check-source-shape` (2,420 files, 0 skipped, warnings only, exit 0); `git diff --check f6ba3f8..4370643` and `git diff --check` (exit 0). `git diff --name-only f6ba3f8..4370643 -- '*.json'` returned no changed JSON, so no `json.tool` invocation applied.
- No nested compositor row, host D-Bus service, hardware, uinput, or network was invoked.

VERDICT REJECT P0/P1/P2/P3=0/2/0/1
