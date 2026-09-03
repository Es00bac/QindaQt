# Ida Holz — independent platform-adapter review

- **Persona:** Ida Holz, independent platform-adapter reviewer
- **Provider/model:** OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Candidate SHA:** `f93effea182abcb50dd3dfb9dd6b8906839d4e18`
- **Tree SHA:** `b742207b9d4e3d0a651c5d15cd0b56a540009934`
- **Parent SHA:** `661ce14b5ef2a5194b2c1eecf54271281789793b`
- **Base SHA:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/power-pb2-upower-codex-review`
- **Review diff:** `git diff 74da46345c7a5094d45c756ad8b23ca87591fcd3..f93effea182abcb50dd3dfb9dd6b8906839d4e18`

## Findings ledger

### P0

None.

### P1

#### P1-1 — The production UPower decoder does not implement UPower's line-power or system-supply semantics

Paths:

- `src/services/power_service/src/adapters/upower_device_decoder.cpp:99`
- `src/services/power_service/src/adapters/upower_device_decoder.cpp:106`
- `src/services/power_service/src/adapters/upower_device_decoder.cpp:112`
- `tests/services/power_service/tst_power_upower_adapter.cpp:55`

`decodeUpowerDevice()` requires and reads `IsPresent` before it branches on device type, then uses that battery-only property as `acPresent` for a line-power device. The installed UPower 1.90.9 interface contract instead defines `Online` as “whether power is currently being provided through line power”; it says `IsPresent` is valid for battery devices. The decoder also ignores UPower's `PowerSupply` flag, even though that flag distinguishes a system battery/UPS from peripheral batteries. Consequently an online AC source is published as absent whenever its irrelevant `IsPresent` value is false, and a mouse or other peripheral with `Type=Battery, PowerSupply=false` is admitted into PB-0 aggregation as a system supply. The candidate fake encodes `IsPresent=true` and omits `Online`/`PowerSupply`, so its positive row proves the implementation's invented shape rather than the upstream protocol.

Exact reproduction (scratch source and binary are under the assigned build root):

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

Result: exit `1` (the reproduction returns nonzero when either contract is violated):

```text
lineDecoded=1 acPresent=0
peripheralDecoded=1 hasSupply=1
REPRO_EXIT=1
```

Expected: `Online=true` produces `acPresent=1`, and `PowerSupply=false` produces `hasSupply=0`. Observed: the exact opposites.

Independent expected-contract evidence, without contacting a host bus:

```sh
sed -n '420,490p' /usr/share/dbus-1/interfaces/org.freedesktop.UPower.Device.xml
sed -n '640,680p' /usr/share/dbus-1/interfaces/org.freedesktop.UPower.Device.xml
```

Result: exit `0`; the local UPower 1.90.9 XML defines `PowerSupply` at line 432, `Online` at line 464 for line power, and `IsPresent` at line 653 for batteries.

This is P1 because the advertised production adapter reports wrong AC truth and corrupts the aggregate on ordinary systems with peripheral batteries; the fake-only tests cannot establish the claimed outcome.

#### P1-2 — A historical `Can* == yes` remains executable after logind changes it to `no`

Paths:

- `src/services/power_service/src/adapters/logind_action_authority.cpp:124`
- `src/services/power_service/src/adapters/logind_action_authority.cpp:186`
- `src/services/power_service/src/adapters/logind_action_authority.cpp:207`
- `tests/services/power_service/tst_power_logind_actions.cpp:77`

`refreshAdmittedActions()` queries `Can*`, but `submitAction()` only checks the cached `m_admitted` bits. There is no refresh timer or logind signal that invalidates a changed `Can*` answer. The nearby `AGENT-GUARD` says admission is checked “at dispatch”, yet dispatch performs no current query. A previously admitted action can therefore reach logind indefinitely after the actual `Can*` answer becomes `no`. The existing change test explicitly calls `refreshAdmittedActions()` itself and has no negative row for an action submitted after upstream policy changes.

Exact reproduction (the private bus socket and all scratch files are under the assigned build root):

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

Result: exit `1` (the reproduction returns nonzero if a dispatch occurs):

```text
cachedPowerOff=1 dispatchedCalls=1
REPRO_EXIT=1
```

Expected: after the fake changes `CanPowerOff` from `yes` to `no`, operation `9001` is rejected without an upstream `PowerOff(false)` call. Observed: the stale admitted bit remains true and one action call reaches logind.

This is P1 because the accepted action-admission contract says only an exact `yes` admits execution; the implementation uses historical rather than dispatch-time authorization truth.

### P2

#### P2-1 — The build-root production activation row is not a faithful replacement for the legacy activation row

Paths:

- `tests/services/power_client/tst_power_activation.cpp:219`
- `tests/services/power_service/tst_power_production_activation.cpp:225`
- `docs/wiki/architecture/power-service.md:272`

The legacy test's `daemonLossExitsAndReplacementStartsFresh()` kills the constructing bus, proves the exact service process exits, starts a new bus, and proves a distinct unique owner, epoch, and PID. The new build-root `PowerProductionActivationTests` has five rows for production truth/defaults/invalid mode/package text, but no constructing-bus-loss or fresh-replacement row. Therefore excluding the legacy test, as the handoff's 25-row selector does, removes the only executable proof behind the wiki's “constructing-bus-loss exit, fresh epoch on replacement” claim.

Exact reproduction:

```sh
rg -n '^void PowerActivationTests::' \
  tests/services/power_client/tst_power_activation.cpp
rg -n '^void PowerProductionActivationTests::' \
  tests/services/power_service/tst_power_production_activation.cpp
```

Result: both commands exit `0`. The legacy file lists `daemonLossExitsAndReplacementStartsFresh()` at line 219; the new file lists only `productionModePublishesFakeUpstreamTruth`, `productionModeWithoutFakesStaysHonest`, `defaultModeKeepsUnavailableTruth`, `invalidModeExitsFailClosed`, and `packagedDescriptorSelectsProduction` at lines 237–364.

Expected: the replacement row relocates both legacy activation behaviors below the build root. Observed: it does not. Workaround: continue running the legacy `/tmp` row, which is why this is P2 rather than P1.

### P3

#### P3-1 — The power architecture table still labels the pending idle collaborator as PB-2

Path: `docs/wiki/architecture/power-service.md:80`

The page's introduction says idle remains pending, and the revised vertical-slice row at line 259 says idle continues separately, but the module-state table still says `power_idle` is `PB-2`. This is internally inconsistent milestone truth. It does not alter runtime behavior, so it is P3.

Exact reproduction:

```sh
rg -n 'Idle, keyboard|`power_idle`|PB-2 \| Production upstream' \
  docs/wiki/architecture/power-service.md
```

Result: exit `0`; contradictory statements appear at lines 9, 80, and 259.

## Review-question evidence

1. **No host contact:** The complete Debug selector and the 25 safe Release rows pass with `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`. Production activation overwrites both bus addresses with its private abstract address and supplies a build-root `--backlight-root`. Every direct sysfs row constructs its `QTemporaryDir` from `QINDAQT_TEST_SCRATCH_DIR`; source inspection found no test use of `/sys/class/backlight`. The one permitted legacy activation execution left no `/tmp/qindaqt-power-activation-*` residue. No host service, hardware, uinput, compositor row, or network was run. The sysfs implementation forms reads/writes from `m_rootPath`; no test passes the production default root. This supports confinement of the executed suite, subject to the product defects above.
2. **Fail-closed truth:** Registered tests cover owner loss/replacement, malformed types/ordinals, device removal, estimate overflow, PB-0 aggregation rejection, inhibitor sanitization, sleep observation, backlight malformed/read-only/disappearance cases, duplicate action IDs, and interactive-false calls, and those rows pass in both profiles. P1-1 defeats correct upstream truth before PB-0 aggregation, and P1-2 defeats current `Can*` admission.
3. **Composition root:** `main.cpp` defaults to `unavailable`, opens `systemBus()` only for explicit production, and both installed activation files pass `--upstream=production`. Production activation passes against private fakes. P2-1 records the missing replacement/lifetime negative control.
4. **Protocol untouched:** `git diff --name-only 74da463..f93effea -- src/services/power_protocol src/services/power_client src/services/brightness_model src/shell/power_applet` produced no paths (exit `0`). Registry changes are scoped to the new ADR and its supersession note. The largest changed production file is `power_profiles_collaborator.cpp` at 461 nonblank non-comment lines, below the 500-line decomposition threshold. Adapter contracts and fencing traps have searchable `AGENT-CONTRACT`/`AGENT-GUARD` markers; P1-2 identifies one marker that the implementation does not satisfy.
5. **Docs truthful:** The ADR and non-claims correctly avoid physical-hardware, actual suspend/resume, and new UI maturity claims. P1-1 makes the claimed UPower production behavior untrue, P2-1 leaves activation evidence overstated when the legacy row is excluded, and P3-1 is an internal milestone inconsistency.

## Commands and results

### Identity and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 74da463
git status --porcelain
```

Initial result: exit `0`; worktree path matched the lane; candidate/tree/parent/base were exactly the SHAs in the header; porcelain output was empty.

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

Result: Debug exit `0`; Release exit `0`.

### Focused and adjacent builds

The following exact command was run once against each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/<profile> --parallel 3 --target \
  qindaqt_power_protocol_values_tests qindaqt_power_protocol_codec_tests \
  qindaqt_power_aggregation_tests qindaqt_power_client_tests \
  qindaqt_power_qt_transport_tests qindaqt_power_service_publication_tests \
  qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests \
  qindaqt_power_sysfs_backlight_tests qindaqt_power_upstream_composition_tests \
  qindaqt_power_upower_adapter_tests qindaqt_power_profiles_adapter_tests \
  qindaqt_power_logind_adapter_tests qindaqt_power_logind_actions_tests \
  qindaqt_power_production_activation_tests \
  qindaqt_power_applet_presentation_tests qindaqt_power_applet_controls_tests \
  qindaqt_power_applet_request_tests qindaqt_power_applet_controller_tests \
  qindaqt_power_applet_qml_tests qindaqt-shell
```

Result: Debug exit `0`, 399 Ninja steps; Release exit `0`, 399 Ninja steps.

The prescribed target list does not build the registered legacy activation executable. The first exact Debug selector therefore returned exit `8`, with 25 passed and `qindaqt.power-activation` Not Run. I then ran:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/debug \
  --parallel 3 --target qindaqt_power_activation_tests
```

Result: exit `0`, 4 Ninja steps.

### Tests

```sh
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/debug \
  -R '^qindaqt\.power-' --output-on-failure --no-tests=error
```

Final result after building the registered legacy executable: exit `0`, 26/26 passed, total 14.50 seconds. This was the legacy activation row's one permitted execution. `find /tmp -maxdepth 1 -name 'qindaqt-power-activation-*'` was empty before and after.

```sh
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/release \
  -R '^qindaqt\.power-' -E '^qindaqt\.power-activation$' \
  --output-on-failure --no-tests=error
```

Result: exit `0`, 25/25 passed, total 9.72 seconds. The legacy row was excluded in Release so it was not executed a second time; P2-1 explains why the new row does not replace its coverage.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-pb2-codex/site
./tools/check-source-shape
git diff --check 74da463..f93effea182abcb50dd3dfb9dd6b8906839d4e18
```

Results:

- `validate-docs`: exit `0`; 117 Markdown documents and navigation validated.
- `mkdocs build --strict`: exit `0`; site built in 1.32 seconds.
- `check-source-shape`: exit `0`; 1,791 files checked, zero skipped; only the two pre-existing unrelated 500/539-line warnings were emitted.
- `git diff --check`: exit `0`.
- `git diff --name-only 74da463..f93effea -- '*.json'`: no output; no JSON parser gate applied.

An auxiliary `systemd-analyze verify` against the uninstalled generated unit returned exit `1` solely because its configured installed command `/usr/bin/qindaqt-power-service` does not exist in the review host root. This was not treated as a syntax or candidate finding; the candidate's installed-package row passed in both profiles.

### Final immutability check

Recorded after writing this report: candidate, tree, parent, and base SHAs remained those in the header and `git status --porcelain` remained empty. No product path was edited, committed, amended, or rebased.

## Verdict

VERDICT REJECT P0/P1/P2/P3=0/2/1/1
