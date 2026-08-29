# Display Settings D5 — exact candidate review verdict

- Author: Mae Jemison
- Role: Display Settings exact candidate reviewer
- Provider/model/reasoning: Anthropic Claude Code / exact `claude-sonnet-5` / high
- Reviewed candidate: `0666a5ae86f71eaa8ae4e0bb50cddab742c44477`
  (tree `db6227ef4f2a9cb7aefbc3f46a1713631200d175`, sole parent
  `b2901bebf96b4b1395c86f083e858d693f231d4a`)
- Worktree: `/mnt/d/QindaQt/reviews/display-settings-d5-mae` (detached HEAD,
  read-only for the duration of this review)
- Builds: `/mnt/d/QindaQt/builds/display-settings-d5-mae/{dev,release}`
- Verdict: **P2 — do not integrate as-is.** One reproducible blocking
  correctness/truth defect in the QML layer; repair direction below. Core
  model/coordinator/topology/package layers are sound and pass every focused
  gate in both Debug and Release.

## What I verified clean (with fresh evidence, not trusted from the handoff)

- **Byte-clean, unmodified candidate**: `git status` shows only the
  pre-existing untracked `.omc/`; `git diff --stat` against HEAD is empty;
  `git log -1 --format='%H %T %P'` reproduces
  `0666a5ae86f71eaa8ae4e0bb50cddab742c44477 db6227ef4f2a9cb7aefbc3f46a1713631200d175 b2901bebf96b4b1395c86f083e858d693f231d4a`
  exactly, matching the assignment. No edits were made inside the reviewed
  worktree.
- **Public boundary only**: every `#include` in
  `src/apps/settings/display/**` resolves to
  `qindaqt/services/display_client/*`, `display_protocol/*`,
  `display_topology/*`, or the module's own headers. No compositor, no
  `_p.h`, no private reach-through.
- **Source shape**: `python3 tools/check-source-shape` exit 0, 1545 files
  checked, 0 violations touching this candidate (`display_settings_model.cpp`
  416 lines, `display_settings_draft.cpp` 334, `display_settings_values.cpp`
  213 — comfortably decomposed under the 500-line review threshold, resolving
  the prior 655/738-line blocker).
- **Docs**: `python3 tools/validate-docs` exit 0, "Validated 105 Markdown
  documents and mkdocs.yml navigation."
- **Debug build**: configured+built from a disposable source copy (see note
  below) with the repo's exact dev preset flags
  (`CMAKE_BUILD_TYPE=Debug`, `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, etc.) —
  clean, 0 warnings, 0 errors, 282/282 targets.
- **Release build**: same flags with `CMAKE_BUILD_TYPE=Release` — clean, 0
  warnings, 0 errors, 282/282 targets.
- **Focused tests reproduced passing in both configs (12/12 each, 24/24
  total)**:
  `qindaqt.display-settings-model`, `qindaqt.display-settings-model-adversarial`,
  `qindaqt.display-page` (offscreen+accessibility), `qindaqt.settings-route-registry`,
  `qindaqt.settings-navigation-controller`, `qindaqt.settings-navigation-page`
  (offscreen), `qindaqt.settings-app-offscreen` (QuickTest),
  `qindaqt.settings-app-rejects-unknown-route`,
  `qindaqt.settings-app-rejects-missing-theme`,
  `qindaqt.settings-app-desktop-identity`,
  `qindaqt.settings-app-route-construction` (RUN_SERIAL, confirms
  `display` is a constructed root route, not just `notifications`/`appearance`),
  `qindaqt.settings-app-installed-routes` (package/install-tree gate).
  `ctest --output-on-failure`: `100% tests passed, 0 tests failed` both times.
- **Route/package wiring**: `settings_route_registry.cpp`, `settings_route.{h,cpp}`,
  `Main.qml`, `SettingsRouteHost.qml`, `main.cpp`, and `CMakeLists.txt`
  RPATH/`add_dependencies` changes are clean, additive, and exactly mirror the
  existing Appearance-route precedent. One `Client`/`Coordinator`/
  `DisplaySettingsModel` triple is constructed once for the process lifetime
  in `main.cpp`, matching the wiki's process-lifetime claim — no
  per-route-switch reconstruction/leak risk.
- **Transaction truth semantics**: `DisplaySettingsModel`'s hardcoded 15s
  countdown display does **not** read the wire's
  `TransactionSummary::deadlineMonotonicMilliseconds` — I checked whether that
  was a fabricated/unsafe guess, but it is not: that field is measured against
  `display_service`'s own `QElapsedTimer` started at daemon-process launch
  (`src/services/display_service/app/main.cpp`), so it is opaque and
  meaningless to any other process. The client-side 15s is a documented
  "rescue" estimate matching the real server default
  (`confirmationTimeoutMilliseconds = 15'000` in
  `display_transaction/transaction_types.h`), consistent with
  `Coordinator`'s own AGENT-CONTRACT ("not transaction timer authority").
  Cleared, not a defect.
- Server-authority/topology validation (`DisplayTopology::validateAndNormalize`)
  is the sole gate for accept/no-op/reject in both `applyDraft()` and
  `validateDraft()`; local draft mutation never bypasses it. FlipX transform
  variants are intentionally not exposed in `DisplayTransformSection.qml` —
  this matches the wiki's documented scope table exactly (0°/90°/180°/270°
  only), not a gap.

## Blocking finding (P2) — reproduced, not just inspected

**Position (X, Y) text fields go stale/lie after the first edit.**

- File: `src/apps/settings/display/qml/DisplayArrangementSection.qml:64` and
  `:85` (`posXField.text: root.posX.toString()`,
  `posYField.text: root.posY.toString()`).
- These are live declarative bindings on an *editable* `TextField.text`. The
  first time a user types into either field, Qt/QML's standard
  write-breaks-binding rule fires (a `TextField`'s internal `setText` from key
  input is an ordinary property write, same as any external write) and the
  binding is permanently destroyed for that Item instance. After that:
  - Switching the selected output (`DisplayOutputSection` → a different
    `DisplayOutputCard`) no longer updates the position fields to the newly
    selected output's real X/Y — they keep showing the previously typed value.
  - `cancelDraft()` (Revert button), an incoming snapshot update mid-edit, or
    the coordinator's automatic revert-on-timeout/reject all restore
    `root.posX`/`root.posY` in the model, but the dead binding means the UI
    keeps displaying the stale, no-longer-true typed value.
  - This is a direct violation of the "apply/confirm/cancel/revert ... truth"
    bar this review is chartered against: after a Revert, the position fields
    actively misrepresent the real committed/current output position.
- **Reproduced empirically**, not just by inspection: I loaded the actual
  `DisplayArrangementSection.qml` offscreen (`QT_QPA_PLATFORM=offscreen`,
  `qml6`) against a minimal mock `displaySettings` object, wrote `"999"` into
  `posXField.text` + fired `editingFinished()` (the same code path a real
  keypress commit takes), then changed the mock's `selectedOutput.positionX`
  to `7` (simulating either a Revert or an output switch). Expected `"7"` if
  the binding were alive; the field kept showing the edited value instead —
  confirmed twice, deterministic. Harness left at
  `/mnt/d/QindaQt/builds/display-settings-d5-mae/repro/repro_position_binding.qml`
  (outside the candidate tree).
- **Test-coverage gap**: `tests/apps/settings/display/tst_display_page.cpp`
  never exercises `displayPosXField`/`displayPosYField` interactively (the
  only hit for `setOutputPosition` is an unrelated fake-model stub) — this
  path was never under test, so the offscreen page test passing does not
  cover it.
- **Repair direction**: don't bind an editable `text` property directly to
  live model state. Either (a) drive the field's displayed text imperatively
  from `onSelectedOutputIdChanged`/`draftChanged`/model-reset signals (set
  `text` only when the field does not have active focus / is not mid-edit),
  or (b) use a `Binding` with `when: !posXField.activeFocus` (or equivalent
  restore-on-external-change pattern) so the binding is reasserted whenever
  the model changes for a reason other than the field's own edit. Add an
  interactive `tst_display_page.cpp` case that edits a position field, then
  switches outputs (or calls the fake model's revert path), and asserts the
  field reflects the new/reverted value — this scenario must be under test
  going forward.

## Non-blocking findings (P3, reporting hygiene)

- `docs/wiki/apps/display-settings.md` describes a "Scale factor slider
  (0.5× – 3.0×)"; the actual control
  (`src/apps/settings/display/qml/DisplayScaleSection.qml`) is a fixed preset
  button row from 100%–300%, matching `kMinimumScale=1.0`/`kMaximumScale=3.0`
  in `display_limits.h` — there is no slider and no 0.5× option anywhere in
  the implementation or protocol. Docs need a one-line fix to match the
  shipped control and true scale bounds.
- The handoff (`1787962456-elena-prism-handoff.md`) reports "9 model test
  cases" and "7 adversarial test cases." Both files actually define 7 and 5
  real `Q_SLOTS` test methods respectively; the reported numbers are QtTest's
  raw `Totals: N passed` line, which also counts `initTestCase()`/
  `cleanupTestCase()` as if they were behavioral cases. The underlying tests
  do genuinely pass (I reproduced 100% pass, both configs) — this is a
  reporting-accuracy note per AGENTS.md's evidence bar for test claims, not a
  functional defect.

## Environment note (transparency, not a candidate defect)

Top-level CMake configure fails before reaching any Display/Settings target
because `src/apps/terminal` unconditionally `find_package(qtermwidget6 ...
REQUIRED)` (ADR-0040) and this container has no `qtermwidget6` installed —
unrelated to this candidate. Rather than touch host packages (shared
container) or the read-only candidate worktree, I built from a disposable
`git archive` copy of this exact commit under
`/mnt/d/QindaQt/builds/display-settings-d5-mae/src-copy` with only
`add_subdirectory(apps/terminal)` commented out in that copy's
`src/CMakeLists.txt`/`tests/CMakeLists.txt`. The reviewed worktree itself was
never touched (see byte-clean proof above); all commands, logs, and
non-candidate source copies live under `/mnt/d/QindaQt/builds/display-settings-d5-mae/`.

## Requested next action

Route back to Elena Prism (or whoever repairs) for the P2 QML binding fix +
the missing interactive test, plus the one-line scale-range doc correction.
Repair in the same worktree; I will recheck the repaired commit rather than
approve on prose, per the workflow contract.
