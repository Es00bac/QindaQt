# Independent exact-candidate review — Power Settings route

- Reviewer persona: **Julia Robinson** (`julia-robinson`), independent application reviewer
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `13a0870433f5a101af9a5d860a9690599ba5391f` (verified: `git rev-parse HEAD` matches exactly)
- Tree SHA: `8869e2270cbefbe61c6bed7c9301d850387fdc1b` (matches the handoff)
- Parent SHA: `347d32f92b32c57edd4fe055c26aab8a8f27298e` (equals the named base)
- Base SHA: `347d32f92b32c57edd4fe055c26aab8a8f27298e`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/power-settings-route-k3-review` (detached at candidate; `git status --porcelain` empty before and after the review; no product path was modified)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-power-k3`
- Scratch reproductions: `/home/cabewse/work_SPaC3/builds/qindaqt/review-power-k3/scratch/` (outside the worktree)

## Findings ledger

### P0

None.

### P1

None. The core admission==presentation contract holds: `profileRows()`/`keyboardBrightnessRows()`
use the same `profileAdmission`/`brightnessAdmission` predicates that `requestProfile`/
`requestKeyboardBrightness` re-run at dispatch, the client preflight
(`src/services/power_client/src/power_client_operations.cpp:45`) mirrors them, and owner/epoch/
revision fencing, debounce coalescing, convergence, and no-replay behavior are proven by the
candidate's own tests in both build profiles.

### P2-1 — Stale state leaves every control enabled and dispatchable while the page says "Controls are unavailable."

- Location: `src/apps/settings/power/power_settings_model.cpp:62-65` (`snapshotAdmitsBase()` checks
  snapshot presence/owner/epoch/revision/availability but never `m_client.state()`); contradictory
  text at `power_settings_model.cpp:103` ("Power information is stale while the service recovers.
  Controls are unavailable."). The host-entry focus chain in `qml/PowerPage.qml:18-27` also treats
  `stale` as a no-domain-control state, showing the intent was that stale closes admission.
- Reproduction (scratch test `staleSnapshotStillAdmitsAndDispatches`, fake transport,
  `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`):
  1. publish the ready fixture snapshot (epoch 41, revision 7, owner `:1.80`);
  2. `Q_EMIT transport.invalidated(":1.80", 41, 8)` so the client refetches;
  3. fail the refetch: `Q_EMIT transport.snapshotReply(":1.80", fetchId, false, Snapshot{}, "boom")`.
     The client publishes `Unavailable` while retaining the ready snapshot
     (`src/services/power_client/src/power_client.cpp:228-233`, timeout path at `:259-268`).
- Observed: `model.stale() == true`; `statusText()` = "…Controls are unavailable.";
  `profileRows()[0]["available"] == true`; `keyboardBrightnessRows()[0]["available"] == true`;
  `model.requestProfile("power-saver")` returns `true` and the fake transport records one real
  `SetProfile` submission. (A concurrent keyboard request during that pending profile operation is
  correctly refused by the one-in-flight fence.)
- Expected: per review question 1, a stale snapshot disables the controls and a request issued
  anyway is refused — and per the model's own stale status text. The Bluetooth route
  (`bluetooth_settings_model.cpp:90` requires `ClientState::Ready`) and the Power applet
  (`power_applet_controller.cpp:160-165` requires `Ready || Degraded`) both close admission here.
- Mitigating context: the dispatched operation remains lineage-safe (exact owner, epoch/revision
  pinned, service-side stale-handle rejection, route-side convergence fencing), and the Audio route
  documents the identical client-preflight-mirroring choice as deliberate. No candidate test pins
  stale-state admission either way (missing negative control). Repair is bounded: gate
  `snapshotAdmitsBase()` on `state() == Ready || Degraded`, or adopt the Audio-style wording and
  document the choice on the owning wiki page.

### P2-2 — A slider request with an unchanged value still dispatches a raw request.

- Location: `src/apps/settings/power/power_settings_model.cpp:235-251`
  (`requestKeyboardBrightness` never compares `normalized` with the row's current normalized value)
  and `src/apps/settings/power/qml/PowerBrightnessSection.qml:147-149` (`onMoved` forwards every
  interactive move, including value-preserving ones such as an arrow key at a clamped value or a
  drag that returns to its origin inside the debounce window).
- Reproduction (scratch test `unchangedValueStillDispatches`, fake transport): publish the ready
  fixture (keyboard raw 5 of 10, normalized 5000), then
  `model.requestKeyboardBrightness(id, 5000)`.
- Observed: the call is admitted and after the 120 ms debounce the transport records one
  `SetKeyboardBrightness` submission with raw value 5 — identical to the current raw value.
- Expected: per review question 2, no request is sent when the value is unchanged. No test in
  `tests/apps/settings/power/tst_power_settings_slider.cpp` covers the unchanged case (missing
  negative control). The effect is benign (idempotent, lineage-safe, one request per gesture), so a
  one-line equality check against the resolved row is a complete repair.

### P3-1 — "Reconnecting…" operation status persists after a successful retry reconnect.

- Location: `src/apps/settings/power/power_settings_model.cpp:201-212` (`retry()` sets
  `m_operationStatusText = "Reconnecting to the power service…"`); no path clears it when a fresh
  authoritative snapshot arrives while no debounce/pending/convergence fence exists
  (`synchronizeAuthority()` at `:320-362` clears the text only together with a fence).
- Reproduction (scratch test `retryStatusTextAfterReconnect`): publish ready truth, `model.retry()`,
  re-announce the owner, finish the refetch. Observed: `model.ready() == true`, `errorText` empty,
  but `operationStatusText` still "Reconnecting to the power service…", so the page's
  `powerOperationStatus` label shows it indefinitely. Expected: the transient status clears once
  authoritative state is shown.

## Commands executed and results

Configure (exact prescribed recipe, initial cache
`/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake`):

- Debug configure `-B …/review-power-k3/debug -DCMAKE_BUILD_TYPE=Debug …`: exit 0.
- Release configure `-B …/review-power-k3/release -DCMAKE_BUILD_TYPE=Release …`: exit 0.

Builds:

- `cmake --build …/debug --parallel 3 --target qindaqt_power_settings_model_tests
  qindaqt_power_settings_slider_tests qindaqt_power_page_tests qindaqt_settings_route_registry_test
  qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test
  qindaqt-settings`: exit 0 (658/658 fresh steps).
- Release, same targets: exit 0 (658/658 fresh steps).

Test selectors (both under `env -u DBUS_SESSION_BUS_ADDRESS
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`, `--output-on-failure --no-tests=error`):

- Debug `ctest -R '^qindaqt\.settings-power-'`: exit 0, **6/6 passed** (model, slider, page,
  boundary, boundary-poison, installed-route).
- Debug Settings Center selector (route-registry, navigation-controller, navigation-page,
  settings-app offscreen/rejects-unknown-route/rejects-missing-theme/desktop-identity/
  route-construction/installed-routes): exit 0, **9/9 passed**.
- Release `ctest -R '^qindaqt\.settings-power-'`: exit 0, **6/6 passed**.
- Release Settings Center selector: exit 0, **9/9 passed**.

Static gates (from the worktree root):

- `./tools/validate-docs`: exit 0 (130 Markdown documents plus navigation validated).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir
  …/review-power-k3/site`: exit 0.
- `./tools/check-source-shape`: exit 0 (only pre-existing non-failing decomposition-review
  warnings in unrelated files; no new entries from this candidate).
- `git diff --check 347d32f..13a0870`: exit 0.
- No JSON files changed in the diff; `python3 -m json.tool` not applicable.

Scratch reproductions (reviewer-written, under the build root, linked against the Debug
artifacts, run with both host buses poisoned):

- `unchangedValueStillDispatches`: PASS as a probe — confirms P2-2 (1 submission, raw 5 == current).
- `staleSnapshotStillAdmitsAndDispatches`: confirms P2-1 (controls available and a real
  `SetProfile` submission during stale while the UI text claims otherwise).
- `retryStatusTextAfterReconnect`: confirms P3-1 (stale "Reconnecting…" status after success).

`tests/session` rows were not run, per the brief.

## Review questions

1. **Admission equals presentation.** Holds for ready/degraded/unavailable/owner-change/pending
   cases (shared predicate at model and dispatch; client preflight mirrors it; owner replacement
   clears actionable truth with no replay — all test-proven). **Fails for the retained-snapshot
   stale state (P2-1).** Profile selection is limited to `snapshot.profiles.supported`
   (`power_settings_model.cpp:128-130`); holds are read-only with reasons and no hold invokables
   exist; no session actions exist (`sessionActionsSupported` constant `false`, meta-object probe in
   the model test, boundary poison covers suspend/hibernate/shutdown/powerOff/reboot/lock with
   positive and negative controls).
2. **Brightness sliders bounded and debounced.** A three-change burst yields exactly one request
   with exact raw conversion (9000 → raw 9 of 10, test-proven); 0–10000 bounds enforced; keyboard
   values are announced (accessible description carries normalized + raw, verified in the page
   test). **Unchanged values still dispatch (P2-2).**
3. **Route wiring append-only.** Verified in `git diff 347d32f..13a0870`: one enum value, one
   registration after bluetooth, one host mapping, Ctrl+8 appended, RPATH/QML/import rows extended,
   Settings Center tests changed only with additive expectations (7→8 counts, appended power
   entries) plus a 25→30 s timeout bump on the construction row. First-focus target is an enabled
   admitted control in wide and compact hosts (page test + navigation helpers). Rows pass under
   `QT_FATAL_WARNINGS=1`; the installed row relocates for real (withhold-module poison requires
   exit 3, restored module stays resident with both buses poisoned) — 6/6 and 9/9 in Debug and
   Release.
4. **Isolation.** All six power rows and the Settings Center selector ran with the session bus
   unset and the system bus pointed at `/nonexistent`. `grep -rniE 'sysfs|/sys/|upower|logind|
   system_bus|systemBus'` over `src/apps/settings/power` and `tests/apps/settings/power` finds only
   the `BacklightReason::LogindError` label mapping and the poison regexes; Qt D-Bus is confined to
   `power_route_composition.cpp` (verified by the boundary row and by grep). The model reads only
   the injected client.
5. **Scope and shape.** Changed paths match the handoff list exactly and stay within owned/additive
   paths from the lane brief. `check-source-shape`, `validate-docs`, strict MkDocs, and
   `git diff --check` all pass. The page keeps physical devices and session actions excluded and
   says so visibly (`powerAuthorityBoundary` StateCard, asserted in the page test).

## Verdict

Two P2 findings (stale-state admission contradicts the page's own "Controls are unavailable" claim
and the review-brief expectation; unchanged brightness values still dispatch) and one P3 (lingering
"Reconnecting…" status). ACCEPT requires P0 = P1 = P2 = 0, so the candidate is rejected back to the
implementer for the bounded repairs named in P2-1 (gate admission on client state or correct the
stale status text, plus a negative-control test), P2-2 (equality guard plus test), and optionally
P3-1.

VERDICT REJECT P0/P1/P2/P3=0/0/2/1
