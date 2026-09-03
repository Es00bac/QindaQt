# Ida Holz — Power PB-2 repair-descendant review

- **Persona:** Ida Holz, independent platform-adapter reviewer
- **Provider/model:** OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Candidate SHA:** `92d9dec8fd89e539802bf1f89223b1deae0614e6`
- **Tree SHA:** `d5179ffeb04528a1e36028663f0f84b94a29d9ac`
- **Parent SHA:** `95f428f9d6b4dc9c0e3ca14f2c741798ed0fe99b`
- **Base SHA:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Rejected ancestor:** `f93effea182abcb50dd3dfb9dd6b8906839d4e18`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/power-pb2-upower-codex-review`
- **Review diff:** `git diff 74da46345c7a5094d45c756ad8b23ca87591fcd3..92d9dec8fd89e539802bf1f89223b1deae0614e6`
- **Repair diff:** `git diff f93effea182abcb50dd3dfb9dd6b8906839d4e18..92d9dec8fd89e539802bf1f89223b1deae0614e6`

## Findings ledger

### P0

None.

### P1

#### P1-1 — A stale authorization reply can erase a restarted generation's operation and prevent terminal completion

Paths:

- `src/services/power_service/src/adapters/logind_action_authority.cpp:232`
- `src/services/power_service/src/adapters/logind_action_authority.cpp:235`
- `src/services/power_service/src/adapters/logind_action_authority.cpp:236`
- `tests/services/power_service/tst_power_logind_actions.cpp:210`

The repaired authorization callback tests `m_pendingAuthorizations.contains(operationId)` and removes that entry before checking `runningGeneration(generation)`. `start()` intentionally advances the generation and clears the seen-ID set, so the same operation ID is valid in a later generation. If generation 1 has a delayed `Can*` reply, `stop()` completes generation 1 and clears its pending entry, generation 2 reuses the ID, and then the delayed generation-1 callback runs, it mistakes generation 2's entry for its own and removes it. The generation-2 reply subsequently sees no entry and returns. The new operation is neither dispatched nor completed.

Exact reproduction (all source, binary, socket, and temporary data remain below the assigned build root):

```sh
/usr/lib64/qt6/libexec/moc \
  -I src/services/power_service/include \
  -I src/services/power_protocol/include \
  src/services/power_service/include/qindaqt/services/power_service/adapters/logind_action_authority.h \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/moc_logind_action_authority.cpp
c++ -std=c++20 -fPIC -pie \
  -I src/services/power_service/include \
  -I src/services/power_protocol/include \
  $(pkg-config --cflags Qt6Core Qt6DBus) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_restart_generation.cpp \
  src/services/power_service/src/adapters/logind_action_authority.cpp \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/moc_logind_action_authority.cpp \
  $(pkg-config --libs Qt6Core Qt6DBus) \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_restart_generation
/home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_restart_generation
```

Result: compilation exit `0`; execution exit `1`. The execution was repeated twice more and produced the same exit and output each time:

```text
firstGenerationFinishes=1 secondGenerationFinishes=0 actionCalls=0
```

Expected: generation 1 completes exactly once as uncertain on `stop()`; after restart, generation 2's reused operation ID completes exactly once as succeeded and produces one `PowerOff(false)` call. Observed: generation 1 completes once, but generation 2 never completes and no action reaches the still-current exact owner.

This violates the public class contract and the accepted wiki/ADR claim that authorization and action stages are generation-fenced and exactly-once. The registered tests cover duplicate IDs within one generation and stop only when no authorization is pending; they have no restart/reused-ID row. This is P1 because a normal lifecycle transition strands an admitted operation permanently.

### P2

None.

### P3

None.

## Repair-question evidence

1. **Prior P1-1 is closed.** The unchanged `upower_wire_contract` reproduction was recompiled against the descendant and exited `0`: `lineDecoded=1 acPresent=1`, `peripheralDecoded=1 hasSupply=0`. Source and registered fake rows additionally cover `Online=true/false`, a `PowerSupply=false` peripheral battery in a mixed inventory, and an UPS with `PowerSupply=true` and no battery-only `IsPresent`.
2. **Prior P1-2's stale-Can defect is closed, but authorization lineage has the new P1 above.** The unchanged `logind_stale_can` reproduction was rebuilt and exited `0` with `cachedPowerOff=1 dispatchedCalls=0`. The registered happy path also passes. The restart reproduction shows that the new authorization stage is not fully generation-fenced.
3. **Prior P2-1 and P3-1 are closed.** `PowerProductionActivationTests::constructingBusLossExitsAndReplacementStartsFresh()` runs from the build-root scratch fixture and the complete selector passes it in both profiles. The architecture table now marks `power_idle` as `Pending later slice`. `power_protocol`, `power_client`, and `shell/power_applet` are byte-identical to base `74da463`; each scoped `git diff --quiet` exited `0`. The repair commit changes no shared registry.
4. **No host contact.** Both complete Power selectors ran with `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`. A `bwrap` mount namespace redirected the legacy row's absolute `/tmp` fixture into the assigned build root. No host system/session service, host sysfs, hardware, polkit, uinput, network, compositor, or nested-session row was run.
5. **Documentation is otherwise proportional.** The Power page, ADR-0056, and testing-harness section now state the UPower properties, dispatch-time authorization, activation lifetime proof, and pending idle boundary accurately, apart from the exactly-once/generation-fencing overclaim exposed by P1-1.

## Commands and results

### Identity, ancestry, and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 74da463
git merge-base --is-ancestor f93effea182abcb50dd3dfb9dd6b8906839d4e18 HEAD
git status --porcelain
```

Result: exit `0`; path matched the assigned worktree; candidate/tree/parent/base matched the header; `f93effe` is an ancestor; porcelain was empty initially and after all review work.

The required `AGENTS.md`, wiki index, module-boundary and coding-practice pages, complete Power architecture page, ADR-0056, relevant testing-harness section, prior verdict, repair lane message, and exact implementer handoff were read before execution. `git show`, `git diff`, and numbered source inspection were used on the repair and affected implementations/tests.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: Debug exit `0`; Release exit `0`. Both generated successfully. CMake emitted the already-known dependency-prefix runtime-search-path warnings.

### Focused and adjacent builds

The following exact command was run for both `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile> --parallel 3 --target \
  qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests \
  qindaqt_power_aggregation_tests qindaqt_power_client_tests \
  qindaqt_power_qt_transport_tests qindaqt_power_activation_tests \
  qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests \
  qindaqt_power_service_residency_tests qindaqt_power_sysfs_backlight_tests \
  qindaqt_power_upstream_composition_tests qindaqt_power_upower_adapter_tests \
  qindaqt_power_profiles_adapter_tests qindaqt_power_logind_adapter_tests \
  qindaqt_power_logind_actions_tests qindaqt_power_production_activation_tests \
  qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests \
  qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests \
  qindaqt_power_applet_qml_tests qindaqt-shell
```

Result: Debug exit `0`; Release exit `0`.

### Complete Power selectors

Scratch mount points were first created below the assigned root:

```sh
mkdir -p \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/debug/ctest-tmp \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/release/ctest-tmp
```

Then this command was run for each profile:

```sh
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
bwrap --bind / / \
  --bind /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile>/ctest-tmp /tmp \
  --dev-bind /dev /dev --proc /proc -- \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile> \
  -R '^qindaqt\.power-' --output-on-failure --no-tests=error
```

Results:

- Debug: exit `0`, 26/26 passed, 0 failed, 16.53 seconds.
- Release: exit `0`, 26/26 passed, 0 failed, 14.88 seconds.

### Original finding reproductions

```sh
c++ -std=c++20 \
  -I src/services/power_service/src/adapters \
  -I src/services/power_protocol/include \
  $(pkg-config --cflags Qt6Core) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/upower_wire_contract.cpp \
  src/services/power_service/src/adapters/upower_device_decoder.cpp \
  src/services/power_service/src/adapters/upstream_identity.cpp \
  $(pkg-config --libs Qt6Core) \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/upower_wire_contract
/home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/upower_wire_contract
```

Result: compile and run exit `0`; `lineDecoded=1 acPresent=1`, `peripheralDecoded=1 hasSupply=0`.

```sh
/usr/lib64/qt6/libexec/moc \
  -I src/services/power_service/include \
  -I src/services/power_protocol/include \
  src/services/power_service/include/qindaqt/services/power_service/adapters/logind_action_authority.h \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/moc_logind_action_authority.cpp
c++ -std=c++20 -fPIC -pie \
  -I tests/services/power_service \
  -I src/services/power_service/include \
  -I src/services/power_protocol/include \
  $(pkg-config --cflags Qt6Core Qt6DBus) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_stale_can.cpp \
  tests/services/power_service/support/fake_logind_service.cpp \
  src/services/power_service/src/adapters/logind_action_authority.cpp \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/moc_logind_action_authority.cpp \
  $(pkg-config --libs Qt6Core Qt6DBus) \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_stale_can
/home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/logind_stale_can
```

Result: compile and run exit `0`; `cachedPowerOff=1 dispatchedCalls=0`.

The new restart-generation reproduction command and result are recorded under P1-1. Its first scratch compilation exited `1` because the reproduction used an ambiguous `QDBusMessage::createReply({...})` overload; the scratch source was corrected to use an explicit `QVariant`, after which compilation exited `0` and the candidate failure reproduced three consecutive times. This was a reviewer-scratch issue, not a product gate failure.

### Static and source gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/site
./tools/check-source-shape
git diff --check \
  74da46345c7a5094d45c756ad8b23ca87591fcd3..92d9dec8fd89e539802bf1f89223b1deae0614e6
git diff --name-only \
  74da46345c7a5094d45c756ad8b23ca87591fcd3..92d9dec8fd89e539802bf1f89223b1deae0614e6 \
  -- '*.json'
```

Results:

- `validate-docs`: exit `0`; 117 Markdown documents and navigation validated.
- `mkdocs build --strict`: exit `0`; built in 1.26 seconds.
- `check-source-shape`: exit `0`; 1,791 files checked, zero skipped; only the two pre-existing 500/539-line warnings were emitted.
- `git diff --check`: exit `0`.
- Changed JSON query: exit `0`, no output; no JSON file changed, so no `python3 -m json.tool` invocation applied.

### Byte-identity and final immutability checks

```sh
git diff --quiet 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- src/services/power_protocol
git diff --quiet 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- src/services/power_client
git diff --quiet 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- src/shell/power_applet
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
```

Result: all three byte-identity checks exited `0`; candidate/tree/parent remained those in the header; porcelain remained empty. No product path was edited, committed, amended, or rebased.

## Verdict

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
