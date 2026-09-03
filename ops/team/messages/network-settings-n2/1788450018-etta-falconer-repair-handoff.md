# Etta Falconer repair handoff — hardened Network secret-agent

- Time: 2026-09-03T09:40:18-06:00
- Feature: QQ-005.04 Network connectivity — confined first-party credential entry.
- Exact repaired candidate commit: `21871a4d143459d04df0542510251908c73e6807`.
- Candidate tree: `d41b16a8f949b0690096d536b91277495c811193`.
- Rejected candidate: `f06d2fd88166e5648f092e44cb6190dfcf5f133e`.
- Exact base commit: `9033df8b8d469a9e072a898bf4a951582c481106`.
- Requested next action: independent exact review by Raman Parimala, then manager integration.

## Finding closure

- **P1-1 — ineffective shared-allocation clearing and ignored standard inputs:** closed by `21871a4d143459d04df0542510251908c73e6807`. `wipesSharedSecretAllocations` in `qindaqt.network-secret-agent-controller` proves byte, direct/nested UTF-16, nested byte, and QML-editor aliases are zeroed rather than detached. The QML handoff now carries editor pointers instead of a JavaScript secret map. `GetSecrets`, `SaveSecrets`, and `DeleteSecrets` own mutable method copies and recursively scrub them on every return; the D-Bus row sends a secret-bearing Save/Delete map, and the registered boundary row requires all three input scrub guards.
- **P1-2 — nested request-budget bypass:** closed by `21871a4d143459d04df0542510251908c73e6807`. `acceptsAccountedNestedConnectionValues` is the positive control and `refusesNestedOverBudgetConnectionBeforePrompt` is the hostile regression in `qindaqt.network-secret-agent-controller`. Lists, string lists, maps, hashes, keys, container overhead, scalar payloads, depth, and item counts are now accounted; unknown metatypes fail closed.
- **P1-3 — vacuous diagnostic canary:** closed by `21871a4d143459d04df0542510251908c73e6807`. `capturesDiagnosticsAndNeverLogsCanary` first injects a diagnostic canary and proves the installed handler captured it, clears only captured channel output, then executes the credential flow and scans that real channel. `qindaqt.network-secret-agent-boundary` rejects Raman's exact empty-handler pattern; applying that boundary script to the detached `f06d2fd` review worktree exits 1 with `Network secret-agent diagnostics proof discards the captured channel`.
- **P3-1 — prompt proof precision:** closed by `21871a4d143459d04df0542510251908c73e6807`. This was reproduced as Raman described (the old row did not directly exercise the cited behavior, not as a product failure). `traversesControlsAndExposesCheckboxStates` and `enterSubmitsAndWindowCloseCancels` now directly cover Tab/Shift+Tab, Enter, close cancellation, show/hide echo state, remember state, and accessible checkbox roles/states under fatal QML warnings. The boundary row requires those named proof cases.

## Changed paths

- `docs/wiki/architecture/network-secret-agent.md`
- `docs/wiki/development/testing-harness.md`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/qml_prompt_presenter.h`
- `src/services/network_secret_agent/include/qindaqt/services/network_secret_agent/secret_agent_types.h`
- `src/services/network_secret_agent/qml/NetworkSecretPrompt.qml`
- `src/services/network_secret_agent/src/qml_prompt_presenter.cpp`
- `src/services/network_secret_agent/src/secret_agent_object.cpp`
- `src/services/network_secret_agent/src/secret_agent_object_p.h`
- `src/services/network_secret_agent/src/secret_agent_types.cpp`
- `src/services/network_secret_agent/src/secret_request_policy.cpp`
- `tests/services/network_secret_agent/check_boundary.cmake`
- `tests/services/network_secret_agent/tst_network_secret_agent_dbus.cpp`
- `tests/services/network_secret_agent/tst_network_secret_prompt.cpp`
- `tests/services/network_secret_agent/tst_secret_agent_controller.cpp`

## Reproduction evidence

1. Raman's three exact scratch modes against `f06d2fd`, each exit 1:

   ```sh
   /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros wipe
   /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros bounds
   /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros diagnostics
   ```

   Outputs respectively retained `shared_copy_after_wipe="canary-UTF16-S3cr3t"`, accepted and prompted for `aggregate_bytes=81920`, and reported `actual_qt_messages_captured=0` while the candidate assertion passed.

2. The new controller regressions built before the product repair against the old implementation. Direct QtTest exit 2: 7 passed, 2 failed. `refusesNestedOverBudgetConnectionBeforePrompt` observed acceptance; `wipesSharedSecretAllocations` observed the byte alias still beginning with `c`.

3. The repaired registered boundary script applied to `/home/cabewse/work_SPaC3/container-wm-workers/network-secret-agent-codex-review` at exact `f06d2fd` exited 1 on the empty diagnostic handler. The same script against the repair exited 0.

## Acceptance evidence

All output stayed under `/home/cabewse/work_SPaC3/builds/qindaqt/network-secret-agent`. No host desktop, host bus, hardware, uinput, session/compositor row, or product-test network call was used.

1. Exact mandated Debug and Release configure recipes, exit 0 in both configurations:

   ```sh
   cmake -S . -B <ROOT>/<config> -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=<Debug-or-Release> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
   ```

2. Focused strict build, exit 0 in Debug and Release:

   ```sh
   cmake --build <ROOT>/<config> --parallel 3 --target qindaqt-network-secret-agent qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests
   ```

3. Exact requested selector under the hostile bus environment, exit 0 and 7/7 passed in Debug; exit 0 and 7/7 passed in Release. The prompt row's registered environment includes `QT_FATAL_WARNINGS=1`, offscreen QPA, software Quick, and empty display variables:

   ```sh
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/<config> -R '^qindaqt\.(network-secret-agent-|settings-network-)' --output-on-failure --no-tests=error
   ```

4. Dependency-adjacent selector on the exact candidate, exit 0 and 12/12 passed in Debug; exit 0 and 12/12 passed in Release:

   ```sh
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/<config> -R '^(qindaqt\.network-secret-agent-|qindaqt\.settings-network-secret-agent-presence$|qindaqt\.network-settings-(model|model-adversarial|boundary|boundary-poison)$|qindaqt\.network-page$)' --output-on-failure --no-tests=error
   ```

5. Direct Debug prompt executable under empty display/bus variables and `QT_FATAL_WARNINGS=1`, exit 0: 6 passed, 0 failed, 0 skipped.
6. `./tools/validate-docs`, exit 0: 133 Markdown documents and MkDocs navigation validated.
7. `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/network-secret-agent/site`, exit 0.
8. `./tools/check-source-shape`, exit 0: 2,189 files checked, 0 skipped. Its four warnings are unchanged files outside this lane (583, 563, 539, and 500 non-blank lines); no repaired file reaches the threshold.
9. `git diff --check`, exit 0. No JSON changed, so the JSON parser gate was not applicable.

## Bounded caveats

- This repairs allocations reachable through the agent's typed Qt value graph. It does not claim forensic control over allocator bookkeeping or opaque third-party transport internals after their public values have been released.
- The candidate deliberately does not add VPN secrets, certificate/private-key selection, profile editing, an agent-owned store, autostart/activation policy, a shell applet, or physical networking qualification.
- `SaveSecrets` and `DeleteSecrets` remain authenticated storage no-ops after recursively scrubbing their inputs; NetworkManager remains the only remembered-storage owner.

Requested next action: independent exact review by Raman Parimala, then manager integration.
