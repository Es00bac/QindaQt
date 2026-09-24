# Independent review — W0 Wi-Fi secret-agent fix

**Verdict: REJECT**

**Exact candidate:** `548be376e3cdd98c25edcc8894336481a0c9dcdb`  
**Base:** `2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347`  
**Review checkout:** detached at the exact candidate in `.cache/claude-plan-20260923/review-luna-review-w0`.

## Blocking findings

### B1 — UTF-8 size checks leave VPN secret copies unwiped

In [secret_request_admission.cpp](/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/review-luna-review-w0/src/services/network_secret_agent/src/secret_request_admission.cpp:158), `walkStringMap` calls `text.toUtf8().size()` once through `boundedText()` and again when charging the aggregate budget. Both expressions create temporary encoded `QByteArray` storage that is destroyed without zeroing; the nearby `wipeStringValue(text)` only overwrites the UTF-16 `QString`. The helper at line 355 also encodes the complete string just to measure it.

This is reachable with a real secret: `a{ss}` is explicitly recognized here as NetworkManager's VPN-secrets shape. `promptFor()` runs `boundedConnection()` at `secret_request_policy.cpp:129` before the supported-setting branch at line 141, so an unsupported VPN request is traversed, copied, and later refused with NoSecrets only after these temporary secret buffers have been freed unwiped. Use a size calculation that does not allocate secret bytes, or create one encoded buffer with a guaranteed wipe on every path; add a regression that exercises a VPN-secret-shaped `a{ss}` value through admission and refusal.

### B2 — Newly decoded byte-array wire values are not scrubbed

`walkByteArrays()` decodes each `aay` item into `QByteArray bytes` and returns on aggregate-limit refusal at line 95 without wiping it; success also releases it unwiped at line 100. `walkIpv6Records()` does the same for its `address` and `nextHop` byte arrays, including the limit-refusal branch (`secret_request_admission.cpp:126`). The walker accepts these signatures based on type alone, not the containing property name, and the method's ordinary `wipeSettingsMap()` cannot reach these separate decoded copies. Thus any sensitive bytes carried in one of these accepted wire values remain in freed heap storage on both success and refusal. Add a byte-array wipe helper and a scope guard immediately after decode so both exits scrub every directly owned buffer.

## Non-blocking notes

- The `QDBusArgument` cursor copies are safe for this use: Qt's copy-on-write demarshaller detaches on read, and the caller does not reuse a locally advanced cursor after rejection. Unbalanced begin/end calls on refusal therefore do not corrupt the stored inbound argument.
- The wire dispatcher fails closed for unknown signatures, depth is checked against the deepest leaf, and the item/aggregate counters are bounded without an arithmetic overflow path found in review.
- The lifetime test launches the production agent executable through the CMake `$<TARGET_FILE:qindaqt-network-secret-agent>` definition and a target dependency. It drives the real private-bus registration and cancellation path. The controller removes a pending request before completing cancellation and wipes late prompt results; I found no stale-prompt or double-completion defect.
- I found no secret logging in the changed path.
- `tools/validate-docs` passed. `mkdocs build --strict` could not run because `mkdocs` is not installed in this environment; I did not install it, per the reviewer brief.

## Commands and evidence

- `cmake --preset dev` — passed in the fresh review worktree.
- `cmake --build build/dev --target qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_ip_config_tests qindaqt_network_secret_agent_lifetime_tests qindaqt-network-secret-agent -- -j8 -l20` — passed.
- Built the Network and Settings Network selector executables in the review worktree using the configured review limits `-- -j8 -l20`.
- `ctest --test-dir build/dev --output-on-failure --repeat until-fail:5 -R '^qindaqt\.network-secret-agent-(dbus|ip-config|lifetime)$'` — passed: 3 tests repeated five times, 15/15 executions.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.(network|settings-network)'` — passed: 39/39 tests.
- Added a temporary test data row, then restored the test source, to send a 257-entry nested `a{sv}` over the private bus: `build/dev/tests/services/network_secret_agent/qindaqt_network_secret_agent_ip_config_tests 'admitsBoundedWireFormsAndRefusesOversized:review-a{sv}-257'` — passed; the adversarial over-limit case was refused as asserted.
- `tools/validate-docs` — passed, 388 Markdown documents and navigation validated.
- `mkdocs build --strict` — unavailable, exit 127 (`mkdocs: command not found`).
- `git diff --check 2e415cad 548be376e3cdd98c25edcc8894336481a0c9dcdb` — passed.

The broad CTest run includes two existing scripts with hard-coded `--parallel 2`; I temporarily adjusted those invocations inside this review worktree to preserve the repository's configured build limits, then restored both files. The candidate's tracked product changes are otherwise untouched; the only remaining additions are this review handoff and the reviewer record/messages.
