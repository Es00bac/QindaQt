# Ida Holz — Power PB-2 second-repair descendant recheck

- **Persona:** Ida Holz, independent platform-adapter reviewer
- **Provider/model:** OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Candidate SHA:** `6cef8b582aeb33522829d6ae838269f31aad7611`
- **Tree SHA:** `cc525f21716fa2f491dc7b0306d05526eae5ad50`
- **Parent SHA:** `28abb738051fa975b901585ade4485e237ad12b5`
- **Base SHA:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Rejected ancestor:** `92d9dec8fd89e539802bf1f89223b1deae0614e6`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/power-pb2-upower-codex-review`
- **Review diff:** `git diff 74da46345c7a5094d45c756ad8b23ca87591fcd3..6cef8b582aeb33522829d6ae838269f31aad7611`
- **Repair diff:** `git diff 92d9dec8fd89e539802bf1f89223b1deae0614e6..6cef8b582aeb33522829d6ae838269f31aad7611`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Recheck evidence

1. **The restart/reused-ID P1 is closed.** The unchanged
   `logind_restart_generation` reproduction was rebuilt against the descendant
   and passed three consecutive executions. Each reported
   `firstGenerationFinishes=1 secondGenerationFinishes=1 actionCalls=1`:
   generation 1 completed exactly once on `stop()`, generation 2 reused ID 77
   and completed exactly once as succeeded, and exactly one `PowerOff(false)`
   call reached the private fake. The repaired callback at
   `src/services/power_service/src/adapters/logind_action_authority.cpp:232`
   now rejects its captured generation before consulting or removing the
   current-run ID-keyed authorization map. The registered regression at
   `tests/services/power_service/tst_power_logind_actions.cpp:228` additionally
   checks both generation values, the first `Uncertain` status, the second
   `Succeeded` status, and the one-call count; it is not vacuous.
2. **The original UPower and stale-authorization P1 findings remain closed.**
   `upower_wire_contract` exited `0` with `lineDecoded=1 acPresent=1` and
   `peripheralDecoded=1 hasSupply=0`. `logind_stale_can` exited `0` with
   `cachedPowerOff=1 dispatchedCalls=0`. The complete fake/registered suite
   covers line power online/offline, a `PowerSupply=false` peripheral in mixed
   inventory, a system UPS, current `Can*` denial, no-prompt action calls, and
   the happy path.
3. **Adjacent pending state has no equivalent ordering flaw.** UPower service,
   enumeration, and device callbacks first pass the identity-unique
   `RefreshCycle` through `acceptReplyOwner()`; `failRefresh()` and
   `tryPublish()` likewise require the current cycle and generation before
   state mutation/publication. Power Profiles refresh replies check generation
   and refresh serial before active-state mutation, while Set/Hold/Release
   replies check generation before completing or mutating the acquired-cookie
   map. These structures do not consult a reusable current-generation key
   before rejecting an older generation.
4. **The prior P2 and P3 remain closed.** The production activation row ran in
   both profiles inside the complete selector, including the relocated
   constructing-bus-loss/replacement proof. The architecture table continues
   to label `power_idle` as `Pending later slice`. The repair changes only the
   logind action adapter, its private fake/test, and the two owning
   documentation sections; nothing moved outside scope.
5. **Public consumers and registries are stable.** `power_protocol`,
   `power_client`, and `shell/power_applet` are byte-identical to base
   `74da463`; `brightness_model` is also untouched by the repair. The repair
   descendant changes neither `mkdocs.yml` nor the ADR index relative to
   `92d9dec`; the original base-to-candidate registry changes only add ADR-0056
   and accurately mark ADR-0024's write route as superseded.
6. **No host contact occurred.** Both Power selectors ran with
   `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`, and `bwrap` redirected the
   legacy test's `/tmp` fixture into the assigned build root. Standalone
   reproductions created only private `dbus-daemon` sockets below that root.
   No ambient system/session service, host sysfs tree, hardware, polkit,
   uinput, network, compositor, or nested-session row was used.

## Commands and results

### Identity, ancestry, and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 74da46345c7a5094d45c756ad8b23ca87591fcd3
git merge-base --is-ancestor 92d9dec8fd89e539802bf1f89223b1deae0614e6 HEAD
git status --porcelain=v1
```

Result: exit `0`; the worktree, candidate, tree, parent, and base matched this
header; `92d9dec` is an ancestor; porcelain was empty before review and after
all review work.

The required `AGENTS.md`, wiki index, module-boundary and coding-practice
pages, complete Power architecture/reference/brightness/applet pages,
ADR-0056, relevant testing-harness section, both prior verdicts, repair-lane
message, and exact implementer handoff were read before execution. The exact
repair diff and numbered affected/pending-state sources were inspected.

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

Result: Debug exit `0`; Release exit `0`. Both generated successfully and
emitted only the known dependency-prefix runtime-search-path warnings.

### Focused and adjacent builds

The following exact target list was built once for Debug and once for Release:

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

Result: Debug exit `0`, 32 effective Ninja steps; Release exit `0`, 32
effective Ninja steps. Incremental no-op/automatic steps were also reported.

### Standalone reviewer reproductions

```sh
/usr/lib64/qt6/libexec/moc \
  -I src/services/power_service/include \
  -I src/services/power_protocol/include \
  src/services/power_service/include/qindaqt/services/power_service/adapters/logind_action_authority.h \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/repros/moc_logind_action_authority.cpp
```

Result: exit `0`.

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

Result: compile exit `0`; run exit `0`:

```text
lineDecoded=1 acPresent=1
peripheralDecoded=1 hasSupply=0
```

```sh
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

Result: compile exit `0`; run exit `0`:

```text
cachedPowerOff=1 dispatchedCalls=0
```

```sh
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

Result: compile exit `0`; three run exits `0`; every run printed:

```text
firstGenerationFinishes=1 secondGenerationFinishes=1 actionCalls=1
```

### Complete Power selectors

For each profile, the legacy absolute `/tmp` fixture was redirected below the
assigned build root:

```sh
mkdir -p /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile>/ctest-tmp
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
bwrap --bind / / \
  --bind /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile>/ctest-tmp /tmp \
  --dev-bind /dev /dev --proc /proc -- \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile> \
  -R '^qindaqt\.power-' --output-on-failure --no-tests=error
```

Results:

- Debug: exit `0`, 26/26 passed, 0 failed, 17.54 seconds.
- Release: exit `0`, 26/26 passed, 0 failed, 14.88 seconds.

The repaired row was also stressed independently in each profile:

```sh
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile> \
  -R '^qindaqt\.power-service-logind-actions$' --repeat until-fail:10 \
  --output-on-failure --no-tests=error
```

Result: Debug exit `0`, 10/10 executions passed in 12.38 seconds; Release exit
`0`, 10/10 executions passed in 12.36 seconds.

### Static, identity, and registry gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/site
./tools/check-source-shape
git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..6cef8b582aeb33522829d6ae838269f31aad7611
git diff --name-only 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- '*.json'
git diff --quiet 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- src/services/power_protocol
git diff --quiet 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- src/services/power_client
git diff --quiet 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- src/shell/power_applet
git diff --name-only 92d9dec8fd89e539802bf1f89223b1deae0614e6..HEAD -- \
  src/services/power_protocol src/services/power_client \
  src/services/brightness_model src/shell/power_applet mkdocs.yml docs/wiki/adr/index.md
```

Results:

- `validate-docs`: exit `0`; 117 Markdown documents/navigation entries validated.
- `mkdocs build --strict`: exit `0`; built in 1.29 seconds.
- `check-source-shape`: exit `0`; 1,791 files checked, zero skipped; only the
  two pre-existing unrelated 500/539-line warnings were emitted.
- `git diff --check`: exit `0`.
- Changed-JSON query: exit `0`, no output; no JSON changed, so no
  `python3 -m json.tool` invocation applied.
- All three byte-identity checks: exit `0`.
- Repair-scope registry/consumer query: exit `0`, no output.

## Verdict

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
