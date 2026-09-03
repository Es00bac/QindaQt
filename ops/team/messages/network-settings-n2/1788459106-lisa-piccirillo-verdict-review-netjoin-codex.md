# Lisa Piccirillo — independent exact-candidate review

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `afee87d54e8d8a3369ce21515ea173a3c19d8207`
- Tree SHA: `d0c3ac46998c02699bb2d10c7c1ee779ffb9cc2b`
- Parent SHA: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Base SHA: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-join-codex-review`

## Findings ledger

### P0

None.

### P1

None.

### P2

1. **The route action can bypass its own absent-agent admission truth.** `src/apps/settings/network/network_settings_model.cpp:352-373` correctly projects a secured unsaved AP as `connectAvailable=false` when the presence name is absent, but `src/apps/settings/network/network_settings_actions.cpp:54-62` does not revalidate that route-owned fact and dispatches through the public client. Scratch reproduction (outside the worktree):

   ```sh
   cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate -B /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate/build -G Ninja
   cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate/build --parallel 3
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate/build/repro_agent_gate
   ```

   Observed: exit 1 with `secretAgentRegistered=false connectAvailable=false invokableReturned=true operations=1`. Expected: the invokable rejects and emits zero operations whenever the same route projection disables a secured first-use action for absent agent presence. The shipped disabled button is the bounded workaround.

2. **The required fake-NetworkManager `AddAndActivateConnection` negative control is absent.** `tests/services/network_manager_adapter/tst_network_visible_profile.cpp:51-91` directly calls and serializes `buildVisibleWifiProfile`; it never exercises the production call at `src/services/network_manager_adapter/src/libnm_visible_network.cpp:134-157`. Reproduction:

   ```sh
   rg -n "add_and_activate|buildVisibleWifiProfile" tests/services/network_manager_adapter src/services/network_manager_adapter/src/libnm_visible_network.cpp
   ```

   Observed: `add_and_activate` occurs only in production; the test references only the helper. Expected from the funded lane/review brief: a fake NetworkManager captures the exact settings map sent through `AddAndActivateConnection`, with recursive absence checks for `psk`/password-shaped keys plus security/agent-owned assertions. The helper row proves construction but not the dispatch boundary, leaving that required negative control uncovered.

### P3

None.

## Commands and results

- `git rev-parse HEAD`; `git status --porcelain`; `git show -s --format='%H%n%T%n%P' HEAD`; `git rev-parse 196e69d` — exit 0; exact SHA/tree/parent/base above; status empty before review.
- Debug and Release configure, each exit 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/debug --parallel 3 --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt-network-secret-agent qindaqt-network-service` — exit 0, 327/327 steps.
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/release --parallel 3 --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt-network-secret-agent qindaqt-network-service` — exit 0, 327/327 steps.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/debug -R '^qindaqt\.(network-|settings-network-)' --output-on-failure --no-tests=error` — exit 0, 35/35.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/release -R '^qindaqt\.(network-|settings-network-)' --output-on-failure --no-tests=error` — exit 0, 35/35.
- `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/debug -N -V -R '^qindaqt\.network-page$'` — exit 0; confirms `QT_QPA_PLATFORM=offscreen`, software rendering, and `QT_FATAL_WARNINGS=1`.
- `./tools/validate-docs` — exit 0; 139 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/site` — exit 0.
- `./tools/check-source-shape` — exit 0; 2,410 files, zero errors; nine pre-existing warnings outside the candidate diff.
- `git diff --check 196e69d..afee87d`; `git diff --check` — exit 0. No changed JSON, so `json.tool` was not applicable.
- Final `git status --porcelain`; `git rev-parse HEAD` — exit 0; status empty; HEAD unchanged.

## Verdict

VERDICT REJECT P0/P1/P2/P3=0/0/2/0
