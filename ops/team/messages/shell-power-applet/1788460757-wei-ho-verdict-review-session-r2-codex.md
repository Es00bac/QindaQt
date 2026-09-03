# Wei Ho — independent session-actions recheck

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate: `23d99f5b3ec5df65b4525552197f744c382f3640`
- Tree: `8fde42060eb32d620ea2dfc494fa62d98b90b9c7`
- Parent: `f78695f05218ca1e01df0d0c6d6cbcbd3f1ecaa9`
- Review base: `437064325e7aa29b14a7dab230382ab947e71253`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/session-actions-codex-review` (detached and clean before/after)

## Findings ledger

### P0

None.

### P1

None. The prior two P1s are closed: `GetActive` probes the pinned `/ScreenSaver` interface for publication and immediate pre-dispatch admission (`session_actions_client.cpp:219-237,377-400`), while every mutation reply is re-fenced by unique owner and authority epoch and becomes `Uncertain/authority-replaced` without replay after replacement (`session_actions_client.cpp:323-327,456-507`).

### P2

None.

### P3

None. The ADR now limits the boundary claim to named consumers, and the registered boundary poison also rejects direct ScreenSaver symbols.

## Executed evidence

`ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-session-codex`.

- Identity/cleanliness: `git rev-parse HEAD HEAD^{tree} HEAD^ 437064325e7aa29b14a7dab230382ab947e71253`; `git status --porcelain`. Exit 0; SHAs are above and status was empty before and after. `git diff --stat 4370643..23d99f5`: 10 files, 403 insertions, 89 deletions (including the prior handoff/worker-record commit).
- Debug/Release configure, each exit 0:

  ```text
  cmake -S . -B $ROOT/<debug|release> -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=<Debug|Release> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- Focused/adjacent build in each configuration, exit 0:

  ```text
  cmake --build $ROOT/<debug|release> --parallel 3 --target qindaqt-session qindaqt-network-secret-agent qindaqt-shell qindaqt-shell-preview qindaqt_global_menu_qmlplugin qindaqt_shell_launcher_qmlplugin qindaqt-desktop-session-probe qindaqt_session_supervisor_tests qindaqt_session_actions_tests qindaqt_session_lock_authentication_tests qindaqt_session_lock_transition_tests qindaqt_qt_session_lock_transport_tests qindaqt_shell_runtime_options_tests qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests qindaqt_power_aggregation_tests qindaqt_power_client_tests qindaqt_power_qt_transport_tests qindaqt_power_activation_tests qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests qindaqt_power_sysfs_backlight_tests qindaqt_power_upstream_composition_tests qindaqt_power_upower_adapter_tests qindaqt_power_profiles_adapter_tests qindaqt_power_logind_adapter_tests qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests qindaqt_power_settings_model_tests qindaqt_power_settings_slider_tests qindaqt_power_page_tests qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests
  ```

- Exact poisoned-bus selector:

  ```text
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir $ROOT/<debug|release> -R '^qindaqt\.(session|power-|settings-power-|shell-runtime-)' --output-on-failure --no-tests=error
  ```

  Debug: 42/42 passed, exit 0, 59.78 s. Release: 42/42 passed, exit 0, 56.31 s.

- Candidate focused row: the same poisoned environment with `ctest --test-dir $ROOT/debug -R '^qindaqt\.session-actions-client$' -V --no-tests=error`: exit 0; 8/8 QtTest cases passed, including missing-interface, immediate lock re-probe, and delayed replaced-owner cases.
- Exact-base negative control stayed under `$ROOT/base-regression.tCTWuQ`: `git archive 4370643` supplied product source and `git archive 23d99f5 tests/services/session_actions/tst_session_actions_client.cpp` supplied the candidate test source; configure/build exited 0. The registered command `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir $ROOT/base-regression.tCTWuQ/build -R '^qindaqt\.session-actions-client$' --output-on-failure --no-tests=error` exited 8 with 5 passed/3 failed. On `4370643`, `ownedScreenSaverWithoutInterfaceIsUnavailable` observed `canLock()==true`, the immediate `GetActive` re-probe assertion failed, and `delayedSuccessFromReplacedOwnerIsUncertain` observed `Succeeded` instead of `Uncertain`.
- Regression row: the poisoned environment with `ctest --test-dir $ROOT/debug -R '^qindaqt\.session-supervisor$' -V --no-tests=error`: exit 0; 16/16 QtTest cases passed, including shell-PID authentication/fixed shutdown order and optional secret-agent restart-once/nonblocking behavior. `git diff --exit-code 4370643..23d99f5 -- src/session_supervisor tests/session_supervisor src/services/network_secret_agent tests/services/network_secret_agent`: exit 0.
- Static gates: `./tools/validate-docs` (141 documents, exit 0); `mkdocs build --strict --site-dir $ROOT/site` (exit 0); `./tools/check-source-shape` (2,421 files, 0 skipped, warnings only, exit 0); `git diff --check` and `git diff --check 4370643..23d99f5` (exit 0). No JSON changed, so `python3 -m json.tool` did not apply.
- No nested compositor row, host D-Bus service, hardware, uinput, or network was invoked.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
