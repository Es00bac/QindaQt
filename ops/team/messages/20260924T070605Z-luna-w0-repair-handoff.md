# W0 network secret-agent repair handoff

- Worker: `luna-w0-repair`
- Product candidate: `b6fb166b10ffb5476f841eeaff1feb7def7f4251`
- Parent candidate: `548be376e3cdd98c25edcc8894336481a0c9dcdb`
- Branch: `worker/claude-w0-wifi-agent-20260923`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w0-wifi-agent`
- Requested next action: independently review the exact product candidate commit above.

## Product paths

- `docs/wiki/architecture/network-secret-agent.md`
- `src/services/network_secret_agent/src/secret_agent_types.cpp`
- `src/services/network_secret_agent/src/secret_agent_types_p.h`
- `src/services/network_secret_agent/src/secret_request_admission.cpp`
- `src/services/network_secret_agent/src/secret_request_admission_p.h`
- `src/services/network_secret_agent/src/secret_request_policy.cpp`
- `tests/services/network_secret_agent/CMakeLists.txt`
- `tests/services/network_secret_agent/tst_network_secret_agent_ip_config.cpp`

The candidate also includes the claim, finding, and verification messages and the `luna-w0-repair` live-board record.

## Fix and copy audit

- B1: admission counts UTF-8 bytes directly from UTF-16 storage, eliminating temporary encoded secret buffers. It treats a valid surrogate pair as four bytes and each lone surrogate as a three-byte U+FFFD replacement.
- B2: scope guards wipe decoded `aay` byte arrays and IPv6 `address`/`nextHop` arrays on both success and aggregate-limit refusal. The same guards cover directly decoded `a{ss}` keys/values and `a{sv}` keys/recursive values.
- Recursive inbound map wiping now overwrites section, property, and nested map/hash keys along with values. The byte-array helper uses the existing volatile overwrite primitive without detaching.
- Policy copies of the connection `id`, `uuid`, and `key-mgmt` metadata are guarded. The independent connection name retained in `PromptRequest` remains the documented non-secret display value; hints and prompt keys are protocol field identifiers.
- `replyFor()`'s decoded UTF-16 string and UTF-8 round-trip `QByteArray` are newly owned copies and are both scope-guarded. Ordinary QVariant strings, byte arrays, lists, maps, and hashes in admission are read through const references; `keyValueBegin()` avoids temporary map-key copies. The remaining `QDBusVariant::variant()` local is a shallow Qt value alias used synchronously; the caller's inbound map stays intact until its outer wipe guard runs. Qt D-Bus cursor copies remain the detached read path confirmed safe in the review.

## Verification

- `cmake --build build/dev --target qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_ip_config_tests qindaqt_network_secret_agent_lifetime_tests qindaqt-network-secret-agent -- -j8 -l20` — final build passed, exit 0. The first build attempt exited 1 because a test constructed `QString` with an unsupported two-argument initializer; the test was corrected and rebuilt.
- `ctest --test-dir build/dev --output-on-failure --repeat until-fail:5 -R '^qindaqt\.network-secret-agent-(dbus|ip-config|lifetime)$'` — final run passed, exit 0: 3 selected CTest cases × 5 repetitions = 15/15 executions. The first run failed in the helper comparison for lone-surrogate behavior; the test now keeps valid-string `toUtf8()` comparisons and separately asserts the required three-byte conservative rule.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.(network|settings-network)'` — passed twice, 39/39 each run; the final captured exit status was 0.
- `./tools/validate-docs` — passed, exit 0; 388 Markdown documents and `mkdocs.yml` navigation validated.
- `git diff --check` — passed, exit 0.

The private-bus regression admits the VPN-shaped `a{ss}` value in a supported request and refuses a `settingName="vpn"` request with NoSecrets after `promptFor()`'s admission call. The `aay` and `a(ayuay)` cases each cover exact aggregate limit and first byte over the limit.

## Caveats

- A local probe on Qt 6.11.1 showed `QString::toUtf8()` returns zero bytes for isolated high and low surrogates, despite the repair brief's parenthetical replacement-character expectation. The implementation follows the explicit three-byte U+FFFD admission rule so malformed UTF-16 cannot evade the text budget; valid strings and a valid surrogate pair match `QString::toUtf8().size()`.
- `mkdocs` is not installed on qinda, so `mkdocs build --strict` was unavailable; the repository's `./tools/validate-docs` gate passed. No live NetworkManager hardware prompt check was run.
- No ADR was added; the repair strengthens the existing secret-agent cleanup contract without changing a process or module boundary.
