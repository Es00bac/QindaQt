# Independent recheck — W0 Wi-Fi secret-agent fix

**Verdict: ACCEPT**

**Exact candidate:** `35af19a6dfc4221d35e32e1bd2df0a3cff0e44d3` (product commit `58e71c99988866e7f996b073a8f405f470aeaff3`)

**Base:** `2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347`

**Rechecked delta:** `5ac3325e9e92cb50a1af283205940fbea5af7e78` → candidate. Detached review checkout: `.cache/claude-plan-20260923/review-luna-review-w0-recheck2`.

## Findings

No blocking defect found. The shared-storage wipe remains intentional and pre-existing. The candidate now states its ownership contract in the public type header, private helpers, recursive wipe implementation, admission cursor walkers, and architecture page: aliases must be dead or be secret-bearing copies that are also being scrubbed; a caller must not wipe data a live owner still needs.

I traced the live call paths. In `secret_request_admission.cpp`, guards wipe byte arrays, IPv6 fields, string-map keys/values, variant-map keys, and each decoded nested variant. These values are extracted into iteration-local storage from a `QDBusArgument`; the `QDBusVariant` and extracted `QVariant` can share with each other, but both are disposable locals. For supported wire forms, recursively decoded string and byte-array leaves are scrubbed before scope exit, including on early refusal. The cursor walker reads a value copy of the stored `QDBusArgument`, not the request map's variant storage.

The `connectionName()` and `wifiFields()` temporaries in `secret_request_policy.cpp` may share storage with values in the request map. Their values are consumed before wiping: the display name is independently copied before the source ID is scrubbed, UUID is validation-only, and key-management is used only to choose the requested field list. `requestSecrets()` retains only a `PromptRequest`, never the input map; `GetSecrets()` wipes the input map after that synchronous decision returns. On refusal, no prompt is retained. I found no later use of the scrubbed map values.

The metadata repair closes the concrete alias paths. `promptFor()` allocates an independent setting-name string for the retained prompt; `replyFor()` allocates independent reply setting-name and field-key strings before `sendCompletion()` wipes its temporary map. Therefore map-key/value scrubbing cannot zero the still-live prompt setting name or field key. The new private-bus regression exercises admission with `aa{sv}`, refusal with the VPN-shaped `a{ss}`, retained prompt metadata, and successful reply serialization. I also ran that regression slot directly; it passed.

## Non-blocking notes

- The retained `aa{sv}` and `a{ss}` map assertions inspect the original caller-side `QDBusMessage` after a real D-Bus call; D-Bus serialization makes this sender-side storage independent from the agent's decoded input. The test's direct live-owner assertions are the retained prompt metadata and reply checks.
- `mkdocs build --strict` could not run because `mkdocs` is not installed (`command not found`, exit 127). `./tools/validate-docs` passed. No physical NetworkManager secured-network prompt cycle was run; private-bus coverage passed.
- CMake configuration completed with pre-existing non-fatal Qt/QML dependency and path warnings.

## Commands and evidence

- `cmake --preset dev` — passed, exit 0.
- `cmake --build build/dev --target qindaqt_network_identity_tests qindaqt_network_validation_tests qindaqt_network_codec_tests qindaqt_network_redaction_tests qindaqt_network_snapshot_gate_tests qindaqt_network_scan_lease_tests qindaqt_network_intent_policy_tests qindaqt_network_model_tests qindaqt_network_client_tests qindaqt_network_client_admission_tests qindaqt_network_adversarial_tests qindaqt_network_qt_transport_tests qindaqt_network_activation_tests qindaqt_network_service_tests qindaqt_network_residency_tests qindaqt_network_manager_adapter_tests qindaqt_network_manager_visible_profile_tests qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_ip_config_tests qindaqt_network_secret_agent_lifetime_tests qindaqt_network_secret_agent_presence_tests qindaqt_network_settings_model_tests qindaqt_network_radio_outcomes_tests qindaqt_network_settings_agent_gate_tests qindaqt_network_settings_model_adversarial_tests qindaqt_network_page_tests qindaqt-network-secret-agent -- -j8 -l20` — passed, exit 0.
- `ctest --test-dir build/dev --output-on-failure --repeat until-fail:5 -R '^qindaqt\.network-secret-agent-(dbus|ip-config|lifetime)$'` — passed, 3 cases × 5 repetitions (15/15 executions).
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.(network|settings-network)'` — passed, 39/39 tests. For the two nested installed-consumer builds, I temporarily changed their script arguments from `--parallel 2` to `-- -j8 -l20`, then restored both scripts; the checkout was clean afterward.
- `build/dev/tests/services/network_secret_agent/qindaqt_network_secret_agent_ip_config_tests preservesRequestDataAcrossAdmissionAndRefusal` — passed, 3/3 QtTest cases including init and cleanup.
- `./tools/validate-docs` — passed; 388 Markdown documents and `mkdocs.yml` navigation validated.
- `mkdocs build --strict` — unavailable, exit 127 (`mkdocs: command not found`); nothing was installed.
- `git diff --check 2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347 35af19a6dfc4221d35e32e1bd2df0a3cff0e44d3` — passed.
