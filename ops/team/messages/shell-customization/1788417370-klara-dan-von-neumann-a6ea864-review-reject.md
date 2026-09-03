# Independent exact-candidate review — Customize Settings canvas

- Reviewer: **Klara Dan von Neumann** (`klara-dan-von-neumann`), independent first-party route reviewer
- Provider/model: Moonshot Kimi `kimi-code/k3`, reasoning high
- Candidate SHA: `a6ea864ec2162dd5c1a8fba9f2d8f1f2f09a321f`
- Candidate tree SHA: `eacc1e919a60f04702a3a52b12f38d453ece623a`
- Parent SHA: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a` (sole parent)
- Base SHA: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a` (parent == base)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review` (detached at candidate; `git rev-parse HEAD` matched the candidate exactly; `git status --porcelain` empty before and after the review)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-k3`
- Scratch reproductions: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-k3/scratch` (never inside the worktree)

## Verdict summary

**REJECT** — two P2 defects. The domain composition, persistence truth,
boundary discipline, and documentation are otherwise in good shape, and every
mandated gate passes in Debug and Release.

## Findings ledger

### P0

None.

### P1

None. The claimed outcomes work: gesture/preview/rollback authority stays in
`EditorSession`, persistence goes only through the profiles store adapter,
conflict/uncertain/lease-loss truth held up under direct attack (see scratch
evidence below), and all mandated selectors pass 16/16 and 9/9 in both build
profiles.

### P2

**P2-1 — `Overlay` ReferenceError in the candidate's discard dialog; broken
centering binding, QWARN on every page load, fatal under `QT_FATAL_WARNINGS=1`.**

- Location: `src/apps/settings/customize/qml/CustomizeActionBar.qml:76`
  (`anchors.centerIn: Overlay.overlay`). The file imports
  `QtQuick.Controls as T` (line 5), so unqualified `Overlay` is not in scope;
  the binding throws `ReferenceError: Overlay is not defined` every time the
  dialog component is created (i.e. on every Customize page construction). The
  discard-confirmation dialog therefore never gets its centering anchor and is
  positioned at the popup default instead of centered over the window.
- Reproduction (observed):
  ```
  cd <build>/debug/tests/apps/settings/customize
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 \
    ./qindaqt_settings_customize_page_tests
  → QWARN : ... CustomizeActionBar.qml:76: ReferenceError: Overlay is not defined
    (test still passes: Totals 3 passed, 0 failed — the warning is uncaught)
  ```
  With `QT_FATAL_WARNINGS=1` added, the same binary aborts with SIGABRT in
  `rendersCompactAndWideWithoutLosingAccessibleEditors` — the page cannot be
  exercised warning-free, which the review brief's keyboard/a11y traversal
  requirement (`QT_FATAL_WARNINGS=1`) demands.
- Expected: no QML reference errors in shipped route presentation; the modal
  discard dialog centered via `T.Overlay.overlay` (or an unqualified
  QtQuick.Controls import). Observed: ReferenceError on every load; dialog
  mispositioned; fatal under strict-warning traversal. The candidate's page
  test passes anyway because it neither runs under fatal warnings nor asserts
  dialog geometry — so this defect shipped through the candidate's own proof.
- Note for calibration: the repo's pre-existing `Main.qml:67` also emits a
  benign Shortcut-ambiguity QWARN under `QT_FATAL_WARNINGS=1`, so
  warning-fatal is not an enforced repo-wide gate; but a ReferenceError is a
  broken binding in candidate-introduced code, not a benign Qt notice. Bounded
  workaround: the dialog is still modal and operable (open/Cancel/Discard all
  work, as the page test proves).

**P2-2 — Dirty draft is silently discarded through the window Quit / title-bar
close path, bypassing the documented discard confirmation.**

- `docs/wiki/apps/customize-settings.md:53-56` promises: "The route's Close
  and navigation-away paths do not discard silently … Close or selecting
  another Settings route opens a modal discard confirmation."
- The in-page Close button (`CustomizeActionBar.qml:18-24`) and
  navigation-away (`SettingsRouteHost.qml` `customizeDeparturePending` +
  `CustomizeRoute.qml` `onActiveRouteChanged`) are correctly guarded. But the
  application-level close paths do not consult the page at all:
  `src/apps/settings_center/Main.qml:67-70` binds `StandardKey.Quit` directly
  to `root.close()`, and there is no `onClosing` handler anywhere under
  `src/apps/` (verified by grep). The title-bar X takes the same unguarded
  path. Closing the window destroys the engine-singleton model and the dirty
  in-memory draft with no confirmation.
- Reproduction (code path, unambiguous): build `qindaqt-settings`, open
  `--page customize`, perform any edit (e.g. remove an applet) so
  `customizeSettings.dirty` is true, then press Ctrl+Q or close the window.
  The process exits; the draft is gone; no `customizeDiscardDialog` appears.
  Compare: clicking the route's Close button in the same state opens the
  modal confirmation (proved by `tst_customize_page.cpp:112-118`).
- Bounded workaround: use the route's Close button; nothing persisted is
  harmed (disk writes occur only through Apply). Still a real hole in the
  route's headline no-silent-discard contract.

### P3

- **P3-1 — Boundary positive scan is a fixed file list that omits the route's
  composition root.** `tests/apps/settings/customize/check_boundary.cmake:7-16`
  scans 8 named files; `customize_route_composition.{h,cpp}` — the files that
  actually contain `QDBusConnection` / `sessionBus()` — are excluded, and any
  future file added to the route escapes the scan entirely (no glob in
  `SOURCE_ROOT` mode). The poison negative control is real and proves the
  matcher rejects `QDBusConnection`/`LayerShellQt` when scanned, and the
  composition's session-bus use is itself legitimate (documented in
  `settings-center.md` "Customize owns a separate Settings1 client"; sibling
  routes' transports are composed the same way in `main.cpp`). But
  `docs/wiki/development/testing-harness.md` ("Boundary and negative-control
  rows scan only the route's owned source") reads as if all owned source is
  covered; the exemption is undocumented.
- **P3-2 — Stale `selected` field in a CONSTANT projection.**
  `customize_settings_projection.cpp:87` projects `{"selected", profile.id ==
  m_selectedProfileId}` per profile, but the `profiles` Q_PROPERTY is
  `CONSTANT` (`customize_settings_model.h:46`); the field is stale after
  `selectProfile()`. No current QML consumes `modelData.selected` (the
  profile selector compares against `selectedProfileId` directly), so this is
  a latent trap for a future consumer, not a live defect.
- **P3-3 — Responsive host switch evades the pending departure dialog.**
  With a dirty draft and the discard confirmation open in the wide host,
  resizing the window across the 540-px threshold deactivates the wide host
  (`SettingsRouteHost.qml`: `presentationActive` gate), destroying the page and
  its dialog; the compact host's `customizeLoader` has `item === null`, so
  `customizeDeparturePending` never engages and the newly selected route shows
  while the model stays dirty. No data loss (the engine-scoped singleton
  retains the draft and re-presenting Customize restores it), but the promised
  modal-on-departure is evaded by a resize race.

## Review questions (answered with evidence)

1. **Domain authority — satisfied.** Every mutation in
   `customize_settings_actions.cpp` goes through
   `CustomizeEditorHost` → `EditorSession` (`armDrag`/`beginVisualDrag`/
   `hoverTarget`/`drop`/`cancelGesture`/`applyGesture`/`undo`/`redo`/
   `applyToUserProfile`); QML only invokes model `Q_INVOKABLE`s. The canvas
   consumes repository-projected rectangles (`panels()` carries solved
   geometry); zone thirds are presentation-only. Acceptance comes solely from
   the session's `DropAcceptance` (`hoverDropTarget`,
   `customize_settings_actions.cpp:175-197`). Preview bracket honored:
   `visualDragActive` gates hover/commit (only true after `BeginPreview`
   succeeded); `commitDrag`/`cancelDrag` map to one drop/cancel; the model
   test `pointerGestureCommitsOneUndoStepAndCancelRollsBack` proves one undo
   step per gesture and exact rollback, and `rejectedTargetRollsBackDeterministically`
   proves rejection leaves the draft untouched. No placement policy is
   re-implemented in the app or QML.
2. **Pointer/keyboard parity — satisfied at the model level, with one
   presentation defect.** `pointerAndKeyboardPathsConverge` proves a pointer
   palette drag and `keyboardInsert` produce identical projected panels; the
   domain translator/gesture suites (6/6 passing both profiles) prove sequence
   identity. Palette Enter activation inserts via the same gesture path
   (`keyboardInsert` → arm/beginDrag/hover/commit). Accessible names for
   palette/panel/zone are asserted through real `QAccessibleInterface` in the
   page row, and Space activation is exercised with real focus. However the
   page row emits the P2-1 ReferenceError and aborts under
   `QT_FATAL_WARNINGS=1`, so warning-free wide/compact traversal is not
   currently achievable.
3. **Persistence and lifecycle — satisfied, including under attack.** Apply
   writes the profile through the profiles-owned `UserProfileStore` first and
   commits the Settings1 selection second (`customize_settings_actions.cpp:413-434`);
   the app contains no filesystem code (all persistence inside the editor
   domain/profiles store). Dirty selection switching is rejected; discard
   rebuilds from the last confirmed profile; lease loss at construction fails
   closed and recovers on refresh (`foreignLeaseFailsClosedThenRecoversOnRefresh`).
   My scratch attacks (below) additionally proved the candidate's *untested*
   uncertain-commit path: an unanswered commit surfaces `Unavailable` with an
   explicit diagnostic and retains dirty truth, the post-uncertainty refresh
   presents Conflict (never false success), hostile non-string and unknown
   `panels.layoutProfile` snapshots fail closed, and discard recovers.
   Residual gap: the Quit/title-bar close path (P2-2).
4. **Boundaries — satisfied.** No `src/shell/**`, LayerShellQt, compositor,
   KWin, or Wayland references anywhere under `src/apps/settings/customize/`
   (grep-verified; boundary row passes). The only D-Bus touch is the
   documented route-owned Settings1 transport in the composition root.
   Settings Center registry/enum/host/CMake edits are purely additive
   (append-only ordering after `network`; RPATH extended, nothing reordered or
   replaced — clean for the concurrent `audio` route). Boundary poison has a
   real negative control (P3-1 covers the fixed-list precision).
5. **Docs — accurate except as noted.** `customize-settings.md` claims match
   the tests; live-shell binding and reveal are explicitly deferred, and the
   "adopted at next shell start" language is consistent across the new page,
   `customization-editor.md`, `layout-profiles.md`, and `settings-center.md`.
   Navigation/index rows present (mkdocs.yml, index.md). Overclaims found:
   the no-silent-discard sentence vs the Quit path (P2-2), and the
   boundary-row wording vs the fixed scan list (P3-1). `settings-center.md`'s
   Ctrl+1–4 list remains accurate (no Ctrl+5 exists or is claimed).

## Commands executed (exact) and results

Configure (both exit 0):
```
cmake -S <worktree> -B <ROOT>/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON   # exit 0
# same with -B <ROOT>/release -DCMAKE_BUILD_TYPE=Release                        # exit 0
```

Focused builds (strict warnings on):
```
cmake --build <ROOT>/debug   --parallel 3 --target qindaqt_settings_customize_model_tests \
  qindaqt_settings_customize_page_tests qindaqt-settings qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test \
  qindaqt_panel_editing_tests qindaqt_applet_editing_tests qindaqt_preview_history_tests \
  qindaqt_coordinator_lease_tests qindaqt_editor_query_tests \
  qindaqt_customize_editor_intent_tests qindaqt_customize_editor_gesture_tests \
  qindaqt_customize_editor_session_tests qindaqt_customize_editor_dirty_state_tests \
  qindaqt_customize_editor_persistence_tests qindaqt_customize_editor_accessibility_tests   # exit 0, 514 steps
cmake --build <ROOT>/release --parallel 3 --target <same 17 targets>                          # exit 0, 514 steps
```

Test selectors:
```
ctest --test-dir <ROOT>/debug   -R 'customiz' --output-on-failure --no-tests=error
  → exit 0, 16/16 passed (5 settings-customize + 5 shell-customization + 6 customize-editor)
ctest --test-dir <ROOT>/release -R 'customiz' --output-on-failure --no-tests=error
  → exit 0, 16/16 passed
ctest --test-dir <ROOT>/debug   --output-on-failure --no-tests=error -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
  → exit 0, 9/9 passed
ctest --test-dir <ROOT>/release --output-on-failure --no-tests=error -R '<same selector>'
  → exit 0, 9/9 passed
```

Static gates (from worktree root):
```
./tools/validate-docs                        → exit 0, 117 Markdown documents + navigation validated
<docs-venv>/mkdocs build --strict --site-dir <ROOT>/site
                                             → exit 0
./tools/check-source-shape                   → exit 0, 1,786 files; only pre-existing
                                               decomposition-review warnings outside candidate paths
git diff --check HEAD~1 HEAD                 → exit 0
python3 -m json.tool                         → not applicable; candidate changes no JSON files
```

Extra reviewer evidence (scratch, under `<ROOT>/scratch`, not in the worktree):
```
# Scratch QtTest binary compiled against the Debug route libraries reusing the
# candidate's customize_test_support.h; 4 attack cases:
#   unansweredCommitBecomesUncertainNotSuccess  (100 ms commit timeout →
#     Unavailable + diagnostic + dirty retained → refresh → Conflict → discard recovers)
#   hostileSnapshotValuesFailClosed             (panels.layoutProfile = 42 → Unavailable, "invalid")
#   unknownProfileSnapshotFailsClosed           ("ghost" → Unavailable, "not installed")
#   dirtySelectionRejectsProfileSwitch          (selectProfile rejected, selection unchanged)
./customize_scratch_attacks                    → exit 0, 6 passed 0 failed (4 cases + init/cleanup)

# Warning-fatal traversal probes:
QT_FATAL_WARNINGS=1 ./qindaqt_settings_customize_page_tests (offscreen/software/a11y)
  → SIGABRT; QWARN CustomizeActionBar.qml:76 ReferenceError: Overlay is not defined   [P2-1]
same binary without QT_FATAL_WARNINGS          → 3/3 pass but the ReferenceError QWARN is printed
QT_FATAL_WARNINGS=1 ./qindaqt_settings_navigation_page_test
  → SIGABRT on pre-existing Main.qml:67 Shortcut-ambiguity QWARN (baseline context: warning-fatal
    QML is not a repo-enforced gate; P2-1 stands as a candidate-introduced broken binding)
```

Gates not run / unavailable: none mandated were skipped. No `tests/session`
rows, host buses, hardware, uinput, or network were touched, per the brief.

## Verdict

Two P2 defects (candidate-introduced QML ReferenceError with a broken dialog
binding that also fails the brief's `QT_FATAL_WARNINGS=1` traversal
requirement; silent draft discard through the window Quit/title-bar path
contradicting the route's documented no-silent-discard contract). ACCEPT
requires P0 = P1 = P2 = 0. Recommended minimal repair: qualify the Overlay
reference (`T.Overlay.overlay`) and either guard the window close against a
dirty Customize draft or narrow the documented discard-confirmation claim;
consider folding in the three P3s.

VERDICT REJECT P0/P1/P2/P3=0/0/2/3
