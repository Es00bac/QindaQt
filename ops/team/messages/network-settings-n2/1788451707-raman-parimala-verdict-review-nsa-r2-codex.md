# Raman Parimala — Network secret-agent repair-descendant recheck

- Persona: Raman Parimala, independent platform reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Review completed: 2026-09-03T09:53:11-06:00
- Exact candidate SHA: `21871a4d143459d04df0542510251908c73e6807`
- Candidate tree SHA: `d41b16a8f949b0690096d536b91277495c811193`
- Parent SHA: `24cb378bb9cb02b9e686e6292637c445fa3bcdfc`
- Base SHA: `9033df8b8d469a9e072a898bf4a951582c481106`
- Rejected product ancestor: `f06d2fd88166e5648f092e44cb6190dfcf5f133e`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-secret-agent-codex-review`
- Review build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex`
- Review surface: repair diff `f06d2fd..21871a4`, with the complete
  `9033df8..21871a4` candidate retained as context

The detached worktree matched the exact candidate and was clean before and
after review. No product path was edited. Scratch sources and builds stayed
under the assigned review build root. No session/compositor test, host bus,
real NetworkManager, hardware, uinput, or network operation was run.

## Findings ledger

### P0

None.

### P1

None.

### P2

#### P2-1 — The proof table claims depth and container-count boundary coverage that the registered controller row does not exercise

- Paths: `docs/wiki/development/testing-harness.md:1521`,
  `tests/services/network_secret_agent/tst_secret_agent_controller.cpp:111-120`,
  and the unexercised decisions in
  `src/services/network_secret_agent/src/secret_request_policy.cpp:42-105`.
- Contract/evidence claim: the proof table attributes “recursive
  aggregate/depth/container bounds” to
  `qindaqt.network-secret-agent-controller`. The implementation separately
  enforces a 256-item nested-container ceiling and a depth ceiling.
- Reproduction:

  ```sh
  /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/debug/tests/services/network_secret_agent/qindaqt_network_secret_agent_controller_tests -functions

  rg -n 'kMaximumVariantDepth|kMaximumNestedItems|depth|256|QVariantHash|unknown|metaType' \
    tests/services/network_secret_agent/tst_secret_agent_controller.cpp
  ```

  The function listing contains eight cases, including the recursive
  81,920-byte aggregate rejection, but no depth-boundary or 256/257-item
  boundary case. The source search exits 1 with no match. The aggregate
  regression uses 80 items at one nested level, so it reaches neither decision.
- Expected: registered negative controls exercise the maximum accepted depth
  versus the first rejected depth and 256 versus 257 nested items before the
  proof table claims those bounds.
- Observed: both bounds exist only as implementation branches; the registered
  row and its named cases do not execute them.
- Impact: an off-by-one or removed depth/item fence could pass every reported
  lane gate. This is a bounded missing-negative-control defect; the exact
  65,536-byte nested aggregate regression itself is repaired.

### P3

None.

## Prior-finding recheck

### P1-1 — shared-allocation and standard-input wiping

The repair is functionally closed. The registered
`wipesSharedSecretAllocations` case uses dynamically allocated shared
`QByteArray`, direct/nested UTF-16, nested byte, and editor-value aliases and
requires every byte/code unit to be zero after clearing. Direct execution
passed. Inspection confirms that `GetSecrets`, `SaveSecrets`, and
`DeleteSecrets` take mutable method copies guarded by
`wipeSettingsMap(connection)`; the helper recursively wipes supported nested
Qt values without a copy-on-write detach. The registered boundary row also
requires three standard-input wipe sites.

The original `wipe` scratch command still exits 1:

```text
map_empty=true shared_copy_after_wipe="canary-UTF16-S3cr3t"
```

That exact scratch fixture uses `QStringLiteral`, whose zero-capacity static
storage is deliberately not writable, so it does not model the dynamically
owned D-Bus/QML/reply allocation named by the finding. This is an accepted
rebuttal to that fixture. A candidate-sensitive dynamic-allocation negative
control compiled against exact `f06d2fd` exits 1 with
`old_dynamic_utf16_zeroed=false`; the repaired registered case passes.

### P1-2 — recursive 65,536-byte aggregate budget

The exact prior `bounds` reproduction now exits 0:

```text
aggregate_bytes=81920 accepted=false prompt_shown=false completed=true
```

The registered `refusesNestedOverBudgetConnectionBeforePrompt` case asserts
the same 80 × 1,024-byte nested payload returns `NoSecrets` and creates no
prompt, while `acceptsAccountedNestedConnectionValues` is a positive control.
The focused direct execution passed. The same candidate-sensitive fixture
compiled against exact `f06d2fd` exits 1 with
`old_nested_accepted=true prompt_shown=true completed=false`, confirming the
assertion distinguishes the rejected ancestor. P2-1 is limited to the separate
depth and per-container count claims.

### P1-3 — diagnostic canary

The repaired registered test installs a real Qt handler, emits a diagnostic
canary, asserts that the handler captured it, clears only captured output, then
scans that same channel after the credential flow. Direct execution passed.
Applying the repaired registered boundary script to an exact archived
`f06d2fd` source tree exits 1 with:

```text
Network secret-agent diagnostics proof discards the captured channel
```

The original `diagnostics` scratch command still exits 1 because it
hard-codes the removed empty-handler/test-authored-entry pattern; it is not
candidate-sensitive and is accepted as a reproduction of the rejected test
shape, not as evidence against the descendant. Product-channel inspection
finds only two additional `fprintf(stderr, ...)` sites in `app/main.cpp`,
both fixed startup literals with no credential-derived argument. The repaired
proof table precisely claims the self-proved real Qt channel rather than
executable stderr capture.

### P3-1 — prompt proof precision

Closed as an evidence-precision repair. The registered
`traversesControlsAndExposesCheckboxStates` case directly exercises forward
and reverse Tab order, accessible checkbox roles/states, show/hide echo state,
and remember state. `enterSubmitsAndWindowCloseCancels` directly exercises
Enter submission, editor clearing at completion, and window-close
cancellation. Both direct cases passed under fatal QML warnings. Because the
ancestor product already contained the declarative behavior, absence of the
old test cases—not a required old-product behavior failure—is the accepted
rebuttal to requiring these new UI assertions to fail on `f06d2fd`.

## Admission and boundary regression

The exact hostile-bus selector passed 7/7 in Debug and 7/7 in Release. The
adjacent set passed 12/12 in each configuration, including Network Settings
model/adversarial, page, positive boundary, boundary poison, secret-agent
positive/poison boundary, private-bus agent, installed package, and
presence-only observation. The repair does not edit Network1 production
paths, and source inspection found no reverse Network1 dependency or expanded
Settings credential/callable-agent surface.

## Commands and results

### Identity and cleanliness

```sh
pwd
git rev-parse HEAD
git status --porcelain
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 9033df8
```

Exit 0. HEAD `21871a4d143459d04df0542510251908c73e6807`;
tree `d41b16a8f949b0690096d536b91277495c811193`; parent
`24cb378bb9cb02b9e686e6292637c445fa3bcdfc`; base
`9033df8b8d469a9e072a898bf4a951582c481106`. Status was empty before
and after the review.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exit 0. Configure/generation completed. CMake emitted the same unrelated
runtime search-path warnings seen in the prior review.

### Focused builds

For both `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/<profile> \
  --parallel 3 --target \
  qindaqt-network-secret-agent \
  qindaqt_network_secret_agent_controller_tests \
  qindaqt_network_secret_agent_prompt_tests \
  qindaqt_network_secret_agent_dbus_tests \
  qindaqt_network_settings_model_tests \
  qindaqt_network_settings_model_adversarial_tests \
  qindaqt_network_page_tests \
  qindaqt_network_secret_agent_presence_tests
```

Debug exit 0, 230/230 actions. Release exit 0, 30/30 incremental actions.

### Exact requested selector

For both `debug` and `release`:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/<profile> \
  -R '^qindaqt\.(network-secret-agent-|settings-network-)' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0; 7/7 passed, 0 failed.
- Release: exit 0; 7/7 passed, 0 failed.

### Adjacent admission and boundary selector

For both `debug` and `release`:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/<profile> \
  -R '^(qindaqt\.network-secret-agent-|qindaqt\.settings-network-secret-agent-presence$|qindaqt\.network-settings-(model|model-adversarial|boundary|boundary-poison)$|qindaqt\.network-page$)' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0; 12/12 passed, 0 failed.
- Release: exit 0; 12/12 passed, 0 failed.

### Direct repaired cases

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/debug/tests/services/network_secret_agent/qindaqt_network_secret_agent_controller_tests \
  wipesSharedSecretAllocations \
  refusesNestedOverBudgetConnectionBeforePrompt \
  capturesDiagnosticsAndNeverLogsCanary
```

Exit 0; 5 passed including init/cleanup, 0 failed, 0 skipped.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  -u DBUS_SYSTEM_BUS_ADDRESS QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/debug/tests/services/network_secret_agent/qindaqt_network_secret_agent_prompt_tests \
  traversesControlsAndExposesCheckboxStates \
  enterSubmitsAndWindowCloseCancels
```

Exit 0; 4 passed including init/cleanup, 0 failed, 0 skipped.

### Prior and old-candidate negative controls

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build --parallel 3
/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros wipe
/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros bounds
/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros diagnostics
```

Configure/build exit 0. The exact prior modes exit 1, 0, and 1 respectively;
their candidate-sensitivity dispositions are recorded in the prior-finding
sections above.

An exact `f06d2fd` archive and reviewer fixture under
`<ROOT>/old-f06-source` and `<ROOT>/old-negative` compile only the rejected
controller/types/request-policy sources. Configure/build exit 0.
`nsa_old_negative wipe` exits 1 with
`old_dynamic_utf16_zeroed=false`; `nsa_old_negative bounds` exits 1 with
`old_nested_accepted=true prompt_shown=true completed=false`.

```sh
cmake \
  -DSOURCE_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/old-f06-source \
  -P /home/cabewse/work_SPaC3/container-wm-workers/network-secret-agent-codex-review/tests/services/network_secret_agent/check_boundary.cmake
```

Exit 1 as expected, rejecting the old empty diagnostic-handler proof.

### Static gates

```sh
./tools/validate-docs
```

Exit 0; 133 Markdown documents and MkDocs navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/site-r2
```

Exit 0.

```sh
./tools/check-source-shape
```

Exit 0; 2,189 source files checked, 0 skipped. Four warnings are unchanged
paths outside this lane (583, 563, 539, and 500 non-blank lines).

```sh
git diff --check
git diff --check 9033df8..HEAD
```

Both exit 0.

```sh
git diff --name-only 9033df8..HEAD -- '*.json'
```

Exit 0 with no output. No JSON changed, so `python3 -m json.tool` is not
applicable.

## Verdict

The three prior P1 defects and the prior P3 proof-precision issue are repaired
or have accepted fixture-scope rebuttals. The candidate is still rejected
because the focused evidence table claims depth and per-container count
negative coverage that the registered controller test does not contain.

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
