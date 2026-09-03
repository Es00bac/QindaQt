# Independent exact-candidate recheck — Power Settings route

- Reviewer persona: **Argelia Velez-Rodriguez** (`argelia-velez-rodriguez`), substitute independent reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `0392aee499e1de3426cd953a61694ed04f1facb9`
- Tree SHA: `b2b217c7dcd2902d4b4e3e61f53770bfd5d5bc73`
- Parent SHA: `8002fa562b813813bd106dca776862ce566a8ad6`
- Repaired product ancestor: `13a0870433f5a101af9a5d860a9690599ba5391f`
- Base SHA: `347d32f92b32c57edd4fe055c26aab8a8f27298e`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/power-settings-route-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex`
- Rejected-ancestor scratch source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/scratch/rejected-13a0870-qb5E65m9`

`git rev-parse HEAD` matched the exact candidate and `git status --porcelain`
was empty before and after review work. No product path in the review worktree
was edited. All scratch mutations remained below the assigned build root.

## Findings ledger

### P0

None.

### P1

None. The repair retains the exact owner/epoch/revision fences and closes
admission whenever the public client state is not `Ready` or `Degraded`.
Neither the candidate tests nor the review probes contacted a host bus,
hardware, Wayland, a nested compositor, or the network.

### P2

None. Julia Robinson's two P2 findings are closed by production guards and
registered, non-vacuous regression tests:

- **P2-1, retained stale truth:**
  `src/apps/settings/power/power_settings_model.cpp:62` now admits base truth
  only when the public client state is `Ready` or `Degraded`. The registered
  `staleSnapshotClosesPresentationAndAdmission` case creates the same failed
  newer-revision fetch, asserts stale status, checks all profile and keyboard
  availability flags are false, forces both request APIs, and requires zero
  transport submissions. It passes on the exact descendant. With only the
  descendant test source transplanted onto `13a0870`, the same registered model
  row fails at the first stale profile availability assertion.
- **P2-2, value-preserving brightness gesture:**
  `src/apps/settings/power/power_settings_model.cpp:239` cancels a same-row
  queued gesture when the requested normalized value equals observed truth or
  converts to the same raw value; `dispatchDebouncedBrightness()` retains a
  final raw-equality guard. The registered data-driven test first queues 4000,
  then returns to exact 5000 or raw-equivalent 5001, waits beyond 120 ms, and
  requires zero submissions plus an idle, error-free model. Both rows pass on
  the descendant and each records one unwanted submission on `13a0870`.

### P3-1 — A retained degraded snapshot is simultaneously stale and degraded

- Location: `src/apps/settings/power/power_settings_model.cpp:80` makes
  `degraded()` true solely from the retained snapshot availability, while
  `stale()` at line 86 independently becomes true from the unavailable client
  state. `statusText()` checks degraded before stale at lines 104–106, whereas
  `src/apps/settings/power/qml/PowerPage.qml:75` gives stale precedence in its
  title.
- Reproduction: in isolated scratch, publish a valid `Degraded` snapshot,
  invalidate revision 8, fail that fetch, and compare the status with the stale
  message:

  ```sh
  env -u DBUS_SESSION_BUS_ADDRESS \
    DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
    /home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/rejected-debug/tests/apps/settings/power/qindaqt_power_settings_model_tests \
    retainedDegradedSnapshotReportsStaleStatus
  ```

  Exit 1. Observed status:
  `Power information is limited; only currently admitted controls are enabled.`
  Expected status:
  `Power information is stale while the service recovers. Controls are unavailable.`
  The page therefore combines a **Stale power information** title with the
  limited-state message, and the model exposes both booleans despite the wiki's
  claim that the states are shown separately.
- Impact: presentation precision only. The repaired `snapshotAdmitsBase()`
  still disables all domain controls and refuses dispatch, so this does not
  reopen P2-1 or weaken lineage safety. A later cleanup can make `degraded()`
  conditional on a non-stale client state or give stale precedence in
  `statusText()`.

## Baseline-finding and regression rechecks

1. **Stale controls and dispatch:** closed. The descendant's registered model
   regression passes; the transplanted assertion fails on `13a0870` with the
   stale profile row still available. The later request assertions are also
   directly protected by the same production predicate used for presentation.
2. **Unchanged slider values:** closed. Exact-normalized and raw-equivalent
   descendant rows both pass with zero submissions. Each row fails on
   `13a0870`, observing one submission instead of zero.
3. **Reconnect status:** closed. The registered descendant case stops/starts
   the client, accepts a fresh exact-owner snapshot, and requires ready truth
   plus empty operation/error text. On `13a0870`, it fails because
   `operationStatusText()` remains nonempty.
4. **Profile admission fence:** direct Debug and Release probes of
   `sharesProfileAdmissionAndConvergenceFence` each passed (3 QtTest results,
   0 failed). They verify active-profile rejection, supported-profile dispatch,
   one submission, all controls fenced while pending, and success only after an
   authoritative revision-8 snapshot converges.
5. **Slider debounce:** direct Debug and Release probes of
   `burstCoalescesToOneExactRawRequest` each passed (3 QtTest results, 0 failed,
   about 152 ms). The three-value burst emits no early submission and exactly
   one final raw value 9 before convergence.

The route/Settings Center registry changes remain append-only in the complete
`347d32f..0392aee` view. The repair itself changes only the Power model/header,
its two focused test files, and the two owning verification documents; the
coordination files in `13a0870..0392aee` belong to the intervening handoff
commit rather than the repair product commit.

## Commands and results

### Exact candidate identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse 'HEAD^{tree}'
git rev-parse HEAD^
git status --porcelain
```

Exit 0. Values matched the header; status output was empty before and after.

### Configure

Debug:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release used the identical command with `release` and
`-DCMAKE_BUILD_TYPE=Release`. Both exited 0.

### Focused builds

For each of Debug and Release:

```sh
cmake --build <build-dir> --parallel 3 --target \
  qindaqt_power_settings_model_tests \
  qindaqt_power_settings_slider_tests \
  qindaqt_power_page_tests \
  qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test \
  qindaqt_settings_navigation_page_test \
  qindaqt-settings
```

Both exited 0, **658/658 build steps** completed.

### Required poisoned-bus selectors

For each Debug and Release build directory:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <build-dir> -R '^qindaqt\.settings-power-' \
  --output-on-failure --no-tests=error
```

Debug: exit 0, **6/6 passed**. Release: exit 0, **6/6 passed**.

For each Debug and Release build directory:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <build-dir> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Debug: exit 0, **9/9 passed**. Release: exit 0, **9/9 passed**.

Direct descendant repair probes under the same poisoned environment:

- Debug model functions `staleSnapshotClosesPresentationAndAdmission` and
  `successfulRetryClearsReconnectStatus`: exit 0, **4 QtTest results passed**.
- Debug slider function `unchangedNormalizedAndRawEquivalentDispatchNothing`:
  exit 0, **4 QtTest results passed**, including both data rows.
- Debug/Release profile-fence function: exit 0 in each, **3/3 QtTest results**.
- Debug/Release debounce function: exit 0 in each, **3/3 QtTest results**.

### Rejected-ancestor differential

An isolated worktree was detached at exact `13a0870`; only the two descendant
test sources were transplanted for this differential before building the old
production model:

```sh
git worktree add --detach \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/scratch/rejected-13a0870-qb5E65m9 \
  13a0870433f5a101af9a5d860a9690599ba5391f
git checkout 0392aee499e1de3426cd953a61694ed04f1facb9 -- \
  tests/apps/settings/power/tst_power_settings_model.cpp \
  tests/apps/settings/power/tst_power_settings_slider.cpp
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/rejected-debug \
  --parallel 3 --target qindaqt_power_settings_model_tests \
  qindaqt_power_settings_slider_tests
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/rejected-debug \
  -R '^qindaqt\.settings-power-(model|slider-admission-debounce)$' \
  --output-on-failure --no-tests=error
```

Build exit 0, **42/42 steps**. CTest exit 8, **0/2 registered rows
passed**. Model totals: **5 passed, 2 failed** at the stale availability and
reconnect-status assertions. Slider totals: **4 passed, 2 failed**, with one
submission observed instead of zero in both no-op data rows. This independently
establishes that all three descendant regression cases distinguish the rejected
production behavior.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-power-codex/site
./tools/check-source-shape
git diff --check
git diff --check 13a0870..0392aee499e1de3426cd953a61694ed04f1facb9
git diff --check 347d32f92b32c57edd4fe055c26aab8a8f27298e..0392aee499e1de3426cd953a61694ed04f1facb9
```

All exited 0. Documentation validated **130 Markdown documents plus
navigation**; strict MkDocs completed; source-shape checked **2101 files** with
only its reported non-failing pre-existing/shared decomposition warnings.
No JSON file changed in either the repair or complete candidate range, so
`python3 -m json.tool` was not applicable.

`tests/session` and all nested-compositor, host D-Bus, hardware, uinput, and
network rows were intentionally not run.

## Verdict

The exact repair descendant closes Julia Robinson's P2/P2/P3 baseline with
registered tests that independently fail against `13a0870`, preserves the
profile and debounce fences, and passes every required Debug/Release and static
gate. The one new P3 is a bounded stale/degraded message-priority inconsistency;
it does not admit or dispatch a control and therefore does not block acceptance.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/1
