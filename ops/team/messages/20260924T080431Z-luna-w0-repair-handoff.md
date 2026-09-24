# W0 shared-scrub contract handoff

- Worker: `luna-w0-repair`
- Product candidate: `58e71c99988866e7f996b073a8f405f470aeaff3`
- Parent candidate: `5ac3325e9e92cb50a1af283205940fbea5af7e78`
- Branch: `worker/claude-w0-wifi-agent-20260923`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w0-wifi-agent`
- Requested next action: independently review the exact product candidate above, then route it for integration.
- ADR: none; this documents and tests the existing shared-storage wipe contract without changing an accepted boundary.

## Product paths

- `docs/wiki/architecture/network-secret-agent.md`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/secret_agent_types.h`
- `src/services/network_secret_agent/src/secret_agent_types.cpp`
- `src/services/network_secret_agent/src/secret_agent_types_p.h`
- `src/services/network_secret_agent/src/secret_request_admission.cpp`
- `src/services/network_secret_agent/src/secret_request_policy.cpp`
- `tests/services/network_secret_agent/tst_network_secret_agent_ip_config.cpp`

The candidate also contains this worker's claim and finding messages and the `luna-w0-repair` live-board record.

## Contract and regression

- `wipeByteArray()`, `wipeString()`, `wipeVariant()`, `wipeSettingsMap()`, and their private/public wrappers now state that scrubbing intentionally writes through implicitly shared storage. Callers may pass a value only when every alias is dead or another secret-bearing copy that must also be scrubbed.
- Each `qScopeGuard` in `secret_request_admission.cpp` now documents that its value came from a `QDBusArgument` cursor into disposable local storage. The nested `QDBusVariant` case documents its only additional alias, the same short-lived decoded local.
- The private-bus regression checks the full retained `aa{sv}` IP request map after admission, verifies the prompt's retained connection name, setting name, and field key after reply-map cleanup, and checks the exact secret reply. It then checks a VPN-shaped `a{ss}` request's key/value bytes remain intact after `NoSecrets` refusal.
- Those assertions exposed live aliases that the earlier review did not cover: `PromptRequest.settingName`, then the reply's setting name and field key. The prompt and reply metadata now use independent allocations before temporary maps are scrubbed.
- The existing shared-buffer test is now named `scrubsSharedSecretAliasesByDesign` and identifies its retained aliases as secret copies whose shared allocations must also be wiped.

## Verification

- `cmake --build build/dev --target qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_ip_config_tests qindaqt_network_secret_agent_lifetime_tests qindaqt-network-secret-agent -- -j8 -l20` — final build passed, exit 0. The first build attempt failed, exit 1, because a braced `QStringList` comma split a `QCOMPARE` macro argument; the expectation was moved into a named variable and rebuilt.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.network-secret-agent-ip-config$'` — final focused run passed 1/1, exit 0. One earlier run failed 0/1 because the admitted test agent was still registered when the refusal case tried to start a second agent; the test now stops the first agent between cases.
- `ctest --test-dir build/dev --output-on-failure --repeat until-fail:5 -R '^qindaqt\.network-secret-agent-(dbus|ip-config|lifetime)$'` — final run passed 15/15 executions, exit 0 (3 CTest cases × 5). Two earlier runs each failed on the new prompt-retention assertion (10 passed, 1 failed execution): first the setting name exposed the settings-map alias, then the field key exposed the reply-map alias. Both aliases are now separated.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.(network|settings-network)'` — passed 39/39, exit 0.
- `./tools/validate-docs` — passed, exit 0; 388 Markdown documents and `mkdocs.yml` navigation validated.
- `git diff --check` and `git diff --cached --check` — passed, exit 0.
- `ctest --test-dir build/dev --output-on-failure -R 'docs|links'` — exit 0, but no tests with those names are registered in this build.
- `mkdocs build --strict` — unavailable, exit 127 (`mkdocs: command not found`); no package was installed.

## Caveats

- No live NetworkManager hardware prompt check was run. The source code and private-bus prompt/reply flow are covered; package installation and joining a new physical Wi-Fi network remain outside this pass.
- `mkdocs build --strict` could not run on qinda; `./tools/validate-docs` passed.
