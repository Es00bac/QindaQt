# Etta Falconer handoff — confined first-party network credential entry

- Time: 2026-09-03T07:21:02-06:00
- Feature: QQ-005.04 Network connectivity — confined first-party credential entry.
- Exact candidate commit: `f06d2fd88166e5648f092e44cb6190dfcf5f133e`.
- Candidate tree: `ce93594bb71145651cdc99bdc1922640faf8be64`.
- Exact base commit: `9033df8b8d469a9e072a898bf4a951582c481106`.
- Requested next action: independent exact review then manager integration.

## Outcome

The candidate adds the independently deployable `qindaqt-network-secret-agent`
process. It registers only through NetworkManager's standard AgentManager,
authenticates every standard SecretAgent call against the exact current owner,
admits bounded PSK/WEP/802.1X requests only with `ALLOW_INTERACTION`, and keeps
credential values confined to wiped prompt/reply buffers. Owner loss,
`CancelGetSecrets`, Escape, close, stop, and timeout cancel once. Remembered
values clear `AGENT_OWNED`; non-remembered values set `NOT_SAVED`; this process
has no storage.

The Settings Network route receives only the ownership fact for the
presence-only `org.qindaqt.NetworkSecretAgent1` session-bus name. Network1 is
unchanged and remains secret-free. ADR-0066 and the architecture/reference/test
pages record the resulting boundary.

## Changed paths

- `docs/wiki/adr/0066-confine-network-credential-entry.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/network-settings.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/network-secret-agent.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `docs/wiki/reference/network1-v1.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/apps/settings/network/CMakeLists.txt`
- `src/apps/settings/network/include/qindaqt/apps/settings_network/network_secret_agent_presence.h`
- `src/apps/settings/network/include/qindaqt/apps/settings_network/network_settings_model.h`
- `src/apps/settings/network/network_secret_agent_presence.cpp`
- `src/apps/settings/network/network_settings_model.cpp`
- `src/apps/settings/network/qml/NetworkPage.qml`
- `src/services/network_secret_agent/CMakeLists.txt`
- `src/services/network_secret_agent/app/main.cpp`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/connection_authority.h`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/prompt_port.h`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/qml_prompt_presenter.h`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/resident_secret_agent.h`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/secret_agent_controller.h`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/secret_agent_types.h`
- `src/services/network_secret_agent/qml/NetworkSecretPrompt.qml`
- `src/services/network_secret_agent/src/dbus_connection_authority.cpp`
- `src/services/network_secret_agent/src/dbus_connection_authority_p.h`
- `src/services/network_secret_agent/src/qml_prompt_presenter.cpp`
- `src/services/network_secret_agent/src/resident_secret_agent.cpp`
- `src/services/network_secret_agent/src/secret_agent_controller.cpp`
- `src/services/network_secret_agent/src/secret_agent_object.cpp`
- `src/services/network_secret_agent/src/secret_agent_object_p.h`
- `src/services/network_secret_agent/src/secret_agent_types.cpp`
- `src/services/network_secret_agent/src/secret_request_policy.cpp`
- `src/services/network_secret_agent/src/secret_request_policy_p.h`
- `tests/CMakeLists.txt`
- `tests/apps/settings/network/CMakeLists.txt`
- `tests/apps/settings/network/check_boundary.cmake`
- `tests/apps/settings/network/stub_network_settings_model.h`
- `tests/apps/settings/network/tst_network_page.cpp`
- `tests/apps/settings/network/tst_network_secret_agent_presence.cpp`
- `tests/services/network_secret_agent/CMakeLists.txt`
- `tests/services/network_secret_agent/check_boundary.cmake`
- `tests/services/network_secret_agent/check_boundary_negative.cmake`
- `tests/services/network_secret_agent/check_installed_agent.cmake`
- `tests/services/network_secret_agent/tst_network_secret_agent_dbus.cpp`
- `tests/services/network_secret_agent/tst_network_secret_prompt.cpp`
- `tests/services/network_secret_agent/tst_secret_agent_controller.cpp`

## Acceptance evidence

All commands ran from the assigned worktree. All build output stayed under
`/home/cabewse/work_SPaC3/builds/qindaqt/network-secret-agent`.

1. Debug configure, exit 0:

   ```sh
   cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/network-secret-agent/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
   ```

2. Release configure with the same flags and `-DCMAKE_BUILD_TYPE=Release`, exit
   0.

3. Focused strict Debug build, exit 0; focused strict Release build, exit 0:

   ```sh
   cmake --build <ROOT>/<config> --parallel 3 --target qindaqt-network-secret-agent qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests
   ```

4. Focused Debug tests, exit 0, 12/12 passed; focused Release tests, exit 0,
   12/12 passed:

   ```sh
   ctest --test-dir <ROOT>/<config> -R '^(qindaqt\.network-secret-agent-|qindaqt\.settings-network-secret-agent-presence$|qindaqt\.network-settings-(model|model-adversarial|boundary|boundary-poison)$|qindaqt\.network-page$)' --output-on-failure --no-tests=error
   ```

   These include the private-bus AgentManager/Settings fake, owner replacement,
   stale-caller refusal, standard delayed reply/cancel/save/delete calls,
   accessibility and keyboard paths under fatal QML warnings, Settings
   compatibility, positive/poison boundaries, and an independently staged
   relocated executable.

5. `./tools/validate-docs`, exit 0: 133 Markdown documents plus navigation
   validated.
6. `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/network-secret-agent/site`,
   exit 0.
7. `./tools/check-source-shape`, exit 0: 2,189 files checked. Its four
   decomposition-review warnings are unchanged pre-existing files outside this
   lane; no candidate source reaches the threshold.
8. `git diff --check`, exit 0.
9. No JSON file changed, so no JSON parser gate applied.

No nested compositor, host D-Bus, host NetworkManager, network device, uinput,
hardware, or product-test network call was run.

## Bounded caveats

- This candidate deliberately does not implement VPN secrets, certificate or
  private-key file selection, profile creation/editing, an agent-owned store,
  a shell applet, or physical network qualification.
- It installs an independent component but does not claim distribution
  autostart, systemd-user, or D-Bus activation policy.
- `SaveSecrets` and `DeleteSecrets` are authenticated typed acknowledgements
  only; NetworkManager owns persistence whenever the reply clears
  `AGENT_OWNED` and `NOT_SAVED`.
