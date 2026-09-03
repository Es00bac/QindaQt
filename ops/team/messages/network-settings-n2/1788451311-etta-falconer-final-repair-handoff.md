# Etta Falconer final repair handoff — exact Network secret-agent bounds proof

- Time: 2026-09-03T10:01:51-06:00
- Feature: QQ-005.04 Network connectivity — confined first-party credential entry.
- Exact candidate commit: `dbeab3fde389b0ed917c0e54e52db8e55d3f609e`.
- Candidate tree: `6d4f663a090c5c31b8445b3eca9ee0308ad31757`.
- Rejected descendant: `21871a4d143459d04df0542510251908c73e6807`.
- Exact base commit: `9033df8b8d469a9e072a898bf4a951582c481106`.
- Requested next action: Raman Parimala performs one independent exact review, then the Program Manager integrates the accepted descendant.

## Finding closure

Raman Parimala's P2-1 is closed in the already registered `qindaqt.network-secret-agent-controller` row. Its data-driven controls now prove both sides of every depth/item comparison in `secret_request_policy.cpp`: generic variant depth 8 accepted and 9 rejected, string-list leaf depth 7 accepted and 8 rejected, and exactly 256 accepted versus 257 rejected for `QVariantList`, `QVariantMap`, `QVariantHash`, and `QStringList`. Every rejected case returns `NoSecrets` before a prompt; every accepted case reaches a prompt and is canceled cleanly. The policy implementation itself was already correct and remains unchanged.

The registered source-boundary row requires each named accepted/rejected pair. Running that repaired check against exact `21871a4` exits 1 on missing `enforcesNestedContainerCountBounds`, while it exits 0 on this candidate. The testing-harness proof table now spells out the exact exercised boundaries rather than using the earlier broad claim.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `tests/services/network_secret_agent/check_boundary.cmake`
- `tests/services/network_secret_agent/tst_secret_agent_controller.cpp`

## Acceptance evidence

All generated output stayed under `/home/cabewse/work_SPaC3/builds/qindaqt/network-secret-agent`. No host desktop, host D-Bus service, NetworkManager daemon, hardware, uinput, compositor/session row, or product-test network call was used.

1. Exact mandated Debug and Release configure recipes, both exit 0:

   ```sh
   cmake -S . -B <ROOT>/<config> -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=<Debug-or-Release> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
   ```

2. Focused strict build, exit 0 in Debug and Release:

   ```sh
   cmake --build <ROOT>/<config> --parallel 3 --target qindaqt-network-secret-agent qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_settings_model_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt_network_secret_agent_presence_tests
   ```

3. Exact mandated selector under the hostile bus environment, exit 0 and 7/7 passed in Debug; exit 0 and 7/7 passed in Release:

   ```sh
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/<config> -R '^qindaqt\.(network-secret-agent-|settings-network-)' --output-on-failure --no-tests=error
   ```

4. Earlier dependency-adjacent closure selector under the same hostile bus environment, exit 0 and 12/12 passed in Debug; exit 0 and 12/12 passed in Release:

   ```sh
   env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/<config> -R '^(qindaqt\.network-secret-agent-|qindaqt\.settings-network-secret-agent-presence$|qindaqt\.network-settings-(model|model-adversarial|boundary|boundary-poison)$|qindaqt\.network-page$)' --output-on-failure --no-tests=error
   ```

5. Direct Debug controller executable, exit 0: 22 passed, 0 failed, 0 skipped. Its function listing includes `enforcesNestedContainerCountBounds()` and `enforcesVariantDepthBounds()`.
6. Repaired `check_boundary.cmake` against this worktree, exit 0. The same script against the clean exact-`21871a4` reviewer worktree exits 1 as expected with `Network secret-agent policy proof lost enforcesNestedContainerCountBounds`.
7. `./tools/validate-docs`, exit 0: 133 Markdown documents and MkDocs navigation validated.
8. `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site-final`, exit 0.
9. `./tools/check-source-shape`, exit 0: 2,189 source files checked, 0 skipped. Four warnings remain in unchanged paths outside this lane (583, 563, 539, and 500 non-blank lines); the edited controller test is 466 non-blank lines.
10. `git diff --check` and `git diff --check 9033df8..HEAD`, both exit 0. No JSON changed, so the JSON parser gate was not applicable.

## Bounded caveats

- This is a test-and-documentation repair for existing admission behavior; it deliberately makes no Network secret-agent production change.
- The candidate still does not claim VPN secrets, certificate/private-key selection, profile editing, an agent-owned store, autostart/activation policy, a shell applet, physical networking, or host-session qualification.
- Standard `SaveSecrets` and `DeleteSecrets` remain authenticated storage no-ops after recursively scrubbing their inputs; NetworkManager remains the only remembered-storage owner.

Requested next action: Raman Parimala performs one independent exact review, then the Program Manager integrates the accepted descendant.
