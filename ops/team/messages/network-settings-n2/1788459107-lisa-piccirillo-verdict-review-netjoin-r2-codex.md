# Lisa Piccirillo — independent exact-candidate recheck

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `beef29eb3efb59d0e06204ef978a40d0181e3035`
- Tree SHA: `014268987b3d4981a6b44795eb5e8d6dae691066`
- Parent SHA: `bd305c2789065cc843dd7dbacbc970fd0fb8eabb`
- Base SHA: `afee87d54e8d8a3369ce21515ea173a3c19d8207`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-join-codex-review`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

The prior two P2 findings are closed. The absent-agent scratch reproduction now reports `secretAgentRegistered=false connectAvailable=false invokableReturned=false operations=0`. The registered agent-gate row asserts that rejection and zero dispatch. The visible-profile row drives the production libnm adapter against an isolated fake NetworkManager, captures the `AddAndActivateConnection` map, checks exact section sets, recursive secret-property absence, Open/WPA-PSK/SAE security, absent PSKs, agent-owned PSK flags, and zero calls for hidden/WEP/enterprise inputs.

## Commands and results

- Identity/cleanliness: `git rev-parse HEAD`; `git rev-parse HEAD^{tree}`; `git rev-parse HEAD^`; `git rev-parse afee87d`; `git status --porcelain` — exit 0; exact SHAs above; status empty before and after review.
- Debug configure — exit 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- Release configure — exit 0: same command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/release -DCMAKE_BUILD_TYPE=Release`.
- Debug and Release focused builds — exit 0 each:

  ```sh
  cmake --build <ROOT>/<debug-or-release> --parallel 3 --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_settings_model_tests qindaqt_network_settings_agent_gate_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt-network-secret-agent qindaqt-network-service
  ```

- Prior scratch reproduction — configure/build/run each exit 0; observed fixed state above:

  ```sh
  cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate -B /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate/build -G Ninja
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate/build --parallel 3
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/repro-agent-gate/build/repro_agent_gate
  ```

- Debug selector — exit 0, 36/36 passed:

  ```sh
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/debug -R '^qindaqt\.(network-|settings-network-)' --output-on-failure --no-tests=error
  ```

- Release selector — exit 0, 36/36 passed: same command with `--test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/release`.
- Direct Debug `qindaqt_network_settings_agent_gate_tests -v2` — exit 0, 3/3 QtTest functions passed; direct `qindaqt_network_manager_visible_profile_tests -v2` under the same absent-bus environment — exit 0, 8/8 QtTest functions/data rows passed.
- `./tools/validate-docs` — exit 0; 139 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-netjoin-codex/site` — exit 0.
- `./tools/check-source-shape` — exit 0; 2,413 files checked, zero errors; nine pre-existing warnings outside the candidate diff.
- `git diff --check afee87d..beef29e`; `git diff --check` — exit 0. No JSON changed, so `python3 -m json.tool` was not applicable.

## Verdict

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
