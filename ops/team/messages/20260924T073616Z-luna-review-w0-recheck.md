# Independent recheck — W0 Wi-Fi secret-agent fix

**Verdict: REJECT**

**Exact candidate:** `5ac3325e9e92cb50a1af283205940fbea5af7e78` (product commit `b6fb166b10ffb5476f841eeaff1feb7def7f4251`)
**Base:** `2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347`
**Repair parent reviewed:** `548be376e3cdd98c25edcc8894336481a0c9dcdb`
**Review checkout:** detached at the exact candidate in `.cache/claude-plan-20260923/review-luna-review-w0-recheck`.

## Blocking findings

### B3 — Wiping one implicitly shared buffer corrupts its surviving owner

In `src/services/network_secret_agent/src/secret_agent_types.cpp:20-29`, `wipeByteArray()` overwrites `constData()` whenever capacity permits, without checking whether the `QByteArray` is detached. `wipeString()` at lines 32-40 has the same behavior. A second live Qt owner therefore sees its shared bytes changed when the first owner is wiped. The new test at `tests/services/network_secret_agent/tst_network_secret_agent_ip_config.cpp:235-268` currently asserts that result for both byte arrays and strings.

I reproduced this with a separate adversarial probe: it copied a `QByteArray`, verified the two values shared storage, wiped one, and observed the still-live alias become zero-filled. The probe exited 1 because the retained owner was not preserved. This violates the requested shared-buffer ownership check and can corrupt a consumer that still uses an alias.

Make the wipe operation preserve other live owners: only overwrite exclusive storage, or detach the instance before overwriting and make sure remaining owners scrub their own storage. Update the regression to verify that a retained alias stays intact while the wiped owner is cleared; handle map-key/value cleanup through an ownership-safe path as well.

## Non-blocking notes

- Prior blockers B1 and B2 are fixed in the reviewed repair. Admission uses `utf8ByteCount()` without allocating encoded strings, and scope guards scrub decoded `a{ss}` strings, `a{sv}` keys/values, `aay` arrays, and IPv6 address/next-hop arrays on success and early refusal. The unsupported VPN-shaped request reaches admission and returns `NoSecrets`; the private-bus tests also cover exact and over-limit arrays and an unknown signature. I found no missing early-return cleanup in these walkers.
- `utf8ByteCount()` handles ASCII, multibyte BMP text, valid surrogate pairs, and lone surrogates correctly. Counting a lone surrogate as three bytes while Qt 6.11.1's `toUtf8()` produces zero is conservative: it overcounts and cannot undercount later UTF-8 storage. An exhaustive probe passed for all 65,536 UTF-16 code units and all 1,048,576 high/low surrogate pairs.
- The rechecked candidate has no other blocking issue in the previously reviewed process lifetime, admission, module-boundary, or documentation changes. No live NetworkManager hardware prompt check was run.

## Commands and evidence

- `cmake --preset dev` — passed, exit 0. CMake emitted non-fatal dependency/QML-path warnings in this environment.
- `cmake --build build/dev --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_ip_config_tests qindaqt_network_secret_agent_lifetime_tests qindaqt_network_secret_agent_presence_tests qindaqt_network_settings_model_tests qindaqt_network_radio_outcomes_tests qindaqt_network_settings_agent_gate_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt-network-secret-agent -- -j8 -l20` — passed, exit 0.
- `ctest --test-dir build/dev --output-on-failure --repeat until-fail:5 -R '^qindaqt\.network-secret-agent-(dbus|ip-config|lifetime)$'` — passed: 3 cases × 5 repetitions, 15/15 executions.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.(network|settings-network)'` — passed: 39/39 tests. To honor the requested build limits, I temporarily changed the two nested install-test scripts from `--parallel 2` to `-- -j8 -l20`, then restored both files; the review worktree was clean afterward.
- `./tools/validate-docs` — passed; 388 Markdown documents and navigation validated.
- `mkdocs build --strict` — unavailable, exit 127 (`mkdocs: command not found`); nothing was installed.
- `git diff --check 2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347 5ac3325e9e92cb50a1af283205940fbea5af7e78` — passed.
- `c++ -std=c++20 -fPIC -I src/services/network_secret_agent/src -I src/services/network_secret_agent/include -I /usr/include/qt6 -I /usr/include/qt6/QtCore -I /usr/include/qt6/QtDBus build/dev/review_shared_wipe.cpp build/dev/src/services/network_secret_agent/CMakeFiles/qindaqt_network_secret_agent.dir/src/secret_agent_types.cpp.o -L/usr/lib64 -lQt6DBus -lQt6Core -o build/dev/review_shared_wipe && build/dev/review_shared_wipe` — detected that wiping one owner zeroes the live alias (expected-preservation probe exit 1).
- `c++ -std=c++20 -fPIC -I src/services/network_secret_agent/src -I src/services/network_secret_agent/include -I /usr/include/qt6 -I /usr/include/qt6/QtCore -I /usr/include/qt6/QtDBus build/dev/review_utf8_adversarial.cpp src/services/network_secret_agent/src/secret_request_admission.cpp src/services/network_secret_agent/src/secret_agent_types.cpp $(pkg-config --cflags --libs Qt6Core Qt6DBus) -o build/dev/review_utf8_adversarial && build/dev/review_utf8_adversarial` — passed for 65,536 single code units and 1,048,576 surrogate pairs; lone high and low surrogates each count as three bytes.
