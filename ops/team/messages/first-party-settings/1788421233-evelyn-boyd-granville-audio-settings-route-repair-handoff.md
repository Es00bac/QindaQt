# Evelyn Boyd Granville — Audio Settings route repair handoff (QQ-006.05)

- Candidate commit: `c7a5b469c6c5a6efc77b4d203f5470cdbec67222`
- Candidate tree: `cd26b9d5bd945318dc684bbf4f64c1a1c4b86660`
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3` (original lane base)
- Repair lineage: rejected candidate `ce66a98` → preserved repair WIP `2d49ab8`
  (manager commit holding Milly Koss's in-progress tree after her provider
  limit) → this candidate `c7a5b46` (audited, completed, and fully re-verified).
- Branch: `worker/audio-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-settings-route`
- Requested reviewer: Joan Clarke (OpenAI Codex), recheck of the exact new
  product SHA; then manager integration.

## How each verdict finding is repaired

- **P1-1 (host entry could target a disabled control).**
  `AudioDeviceSection.qml` and `AudioStreamSection.qml` no longer nominate a
  control by position; every row registers its first/last *enabled, admitted*
  action (set-default → volume → mute inside a device row; mute → volume inside
  a stream row) keyed by row index, and the section recomputes its
  `firstActionTarget`/`lastActionTarget` whenever any registration changes, so
  a control the projection disabled (default output with `canSetVolume ==
  false`) is never nominated while another admitted action exists.
  `AudioPage.qml` chains output → input → stream → Retry → Close for entry and
  streams → input → output for the reverse exit. `SettingsRouteHost.qml` now
  returns `target.activeFocus` after `forceActiveFocus()` instead of assuming
  success. Registered negative control:
  `AudioPageTest::disabledDefaultFallsThroughToFirstAdmittedAction` uses the
  exact snapshot shape from the verdict (default output serial 10 with volume
  and mute unavailable, second output serial 12 with an admitted set-default),
  asserts the entry target is the enabled `audioOutputDefault_12`, focuses it,
  and proves a later projection that re-admits volume recomputes the target.
- **P1-2 (reverse Tab looped on Close).** Close's `KeyNavigation.backtab` now
  targets Retry when visible, otherwise the page's `lastActionTarget`, and only
  falls back to Close when no admitted control exists.
  `keepsCompactFocusVisibleAndClosesTheCycle` asserts reverse traversal in both
  states: Retry hidden → `audioStreamVolume_40` (last admitted control), Retry
  visible → `audioRetryButton`.
- **P2-1 (Settings Center never exercised the Audio route contract).**
  `tst_settings_navigation_page.cpp` now injects the stub Audio model into all
  four `Main.qml` constructions, checks the wide `settingsNavButton_audio`
  existence/active state, and in both the 720×520 wide and 440×360 compact
  layouts proves Ctrl+5 selection, `wideSettingsRouteAudioLoader` /
  `compactSettingsRouteAudioLoader` activation, PageTab accessible role on the
  route tab, Escape return to the tab, and Tab entry into the page's declared
  first focus target (`audioOutputVolume_10`), plus Slider/Heading accessible
  roles inside the page. The stub header is appended to the test executable's
  sources in `tests/apps/settings_center/CMakeLists.txt`.
- **P3-1 (stale architecture text).** `docs/wiki/architecture/audio-service.md`
  "Consumer boundary" now states the Settings route exists and is covered by
  offscreen model/page/boundary/Settings Center tests, while physical hardware
  and nested session interaction remain unqualified and the shell applet stays
  unimplemented.
- **P3-2 (timeout replacement instead of append).** TIMEOUT 25 is kept,
  authorized by the verdict. Empirically justified during this repair: restoring
  15 made `qindaqt.settings-app-route-construction` time out, because
  `check_route_construction.cmake` runs five route constructions each requiring
  the full 3-second bounded-residency witness with the QML disk cache disabled —
  ≥15 s of witness time alone before five cold process spawns. The pre-existing
  15 s budget had under a second of headroom at four routes. In this run the row
  passed in 15.11 s (Debug) and 15.11 s (Release) under the 25 s bound.

Owning wiki pages were updated for the changed contracts
(`docs/wiki/apps/audio-settings.md` focus-entry cycle and proof bullets,
`docs/wiki/development/testing-harness.md` Audio row description).

## Changed paths vs base `74da463` (sorted)

docs/wiki/apps/audio-settings.md, docs/wiki/apps/settings-center.md,
docs/wiki/architecture/audio-service.md,
docs/wiki/architecture/module-boundaries.md,
docs/wiki/development/testing-harness.md, docs/wiki/index.md, mkdocs.yml,
src/CMakeLists.txt, src/apps/settings/audio/CMakeLists.txt,
src/apps/settings/audio/audio_settings_model.cpp,
src/apps/settings/audio/audio_settings_projection.cpp,
src/apps/settings/audio/include/qindaqt/apps/settings_audio/audio_settings_model.h,
src/apps/settings/audio/qml/AudioDeviceSection.qml,
src/apps/settings/audio/qml/AudioLevelRow.qml,
src/apps/settings/audio/qml/AudioPage.qml,
src/apps/settings/audio/qml/AudioStreamSection.qml,
src/apps/settings_center/CMakeLists.txt, src/apps/settings_center/Main.qml,
src/apps/settings_center/SettingsRouteHost.qml, src/apps/settings_center/main.cpp,
src/apps/settings_center/settings_route.cpp, src/apps/settings_center/settings_route.h,
src/apps/settings_center/settings_route_registry.cpp, tests/CMakeLists.txt,
tests/apps/settings/audio/CMakeLists.txt,
tests/apps/settings/audio/audio_settings_test_support.h,
tests/apps/settings/audio/check_boundary.cmake,
tests/apps/settings/audio/check_boundary_negative.cmake,
tests/apps/settings/audio/stub_audio_settings_model.h,
tests/apps/settings/audio/tst_audio_page.cpp,
tests/apps/settings/audio/tst_audio_settings_model.cpp,
tests/apps/settings/audio/tst_audio_settings_model_adversarial.cpp,
tests/apps/settings_center/CMakeLists.txt,
tests/apps/settings_center/check_installed_routes.cmake,
tests/apps/settings_center/check_route_construction.cmake,
tests/apps/settings_center/tst_settings_navigation_controller.cpp,
tests/apps/settings_center/tst_settings_navigation_page.cpp,
tests/apps/settings_center/tst_settings_route_registry.cpp

The repair range (`ce66a98..c7a5b46`) touches only:
`docs/wiki/apps/audio-settings.md`, `docs/wiki/architecture/audio-service.md`,
`docs/wiki/development/testing-harness.md`,
`src/apps/settings/audio/qml/AudioDeviceSection.qml`,
`src/apps/settings/audio/qml/AudioPage.qml`,
`src/apps/settings/audio/qml/AudioStreamSection.qml`,
`src/apps/settings_center/SettingsRouteHost.qml`,
`tests/apps/settings/audio/tst_audio_page.cpp`,
`tests/apps/settings_center/CMakeLists.txt` (one appended source line),
`tests/apps/settings_center/tst_settings_navigation_page.cpp`.

## Evidence (all commands run by me from the worktree root on 2026-09-03;
build root `/home/cabewse/work_SPaC3/builds/qindaqt/audio-settings-route`,
reused Debug and Release configurations)

Build (both profiles, `--parallel 3`):

- `cmake --build <ROOT>/{debug,release} --target qindaqt_settings_audio
  qindaqt_settings_audio_qml qindaqt-settings qindaqt_settings_audio_model_tests
  qindaqt_settings_audio_model_adversarial_tests qindaqt_settings_audio_page_tests
  qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test
  qindaqt_settings_navigation_page_test qindaqt_audio_protocol_tests
  qindaqt_audio_client_tests` — Debug exit 0, Release exit 0.

Tests:

- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.settings-audio-'
  --output-on-failure --no-tests=error` — 5/5 passed, exit 0.
- Same in `<ROOT>/release` — 5/5 passed, exit 0.
- `QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/{debug,release}
  -R '^qindaqt\.settings-audio-page$'` — 1/1 passed, exit 0, both profiles.
- `ctest --test-dir <ROOT>/{debug,release} -R '^qindaqt\.(settings-(route-registry|
  navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|
  missing-theme)|desktop-identity|route-construction|installed-routes))$'
  --output-on-failure --no-tests=error` — 9/9 passed, exit 0, both profiles
  (route-construction 15.11 s, installed-routes 15.56 s Debug / 15.49 s Release).
- `ctest --test-dir <ROOT>/{debug,release} -R '^qindaqt\.audio-(protocol|client)$'
  --output-on-failure --no-tests=error` — 2/2 passed, exit 0, both profiles.
- Joan Clarke's exact scratch reproduction (source copied from
  `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro.cpp`
  with only the three hardcoded review-worktree paths rewritten to this worktree
  and build root; compiled with the verdict's command shape against
  `<ROOT>/debug`) run offscreen with `QT_QPA_PLATFORM=offscreen
  QT_QUICK_BACKEND=software` — printed
  `firstFocusTarget= audioOutputDefault_12 enabled= true`,
  `activeFocusItem= audioOutputDefault_12`,
  `reverseTabFromClose= audioOutputMute_12`, exit 0.

Static gates (worktree root):

- `./tools/validate-docs` — exit 0 (117 documents + navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0. Warnings: two pre-existing on paths
  this lane never touches (`tests/compositor/CMakeLists.txt`,
  `tests/services/display_color_model/tst_color_model.cpp`), plus one on a
  changed path discussed below.
- `git diff --check` and `git diff --check 74da463..c7a5b46` — exit 0, clean.
- No JSON files changed, so `python3 -m json.tool` did not apply.

No test contacted the host session bus, PipeWire, WirePlumber, hardware, or
the network: model rows use an injected fake transport, page/navigation rows
use offscreen stubs, and package/construction rows run against an absent
private bus path with sanitized environment.

## Remaining bounded caveats (deliberately not claimed)

- `tests/apps/settings_center/tst_settings_navigation_page.cpp` measures 535
  non-blank lines (base 466; +69 for the P2-1 coverage the verdict demanded in
  that file). `./tools/check-source-shape` exits 0 and emits a
  decomposition-review advisory for it; every function stays under 180 lines
  (longest 107). I kept the demanded in-file additive extension rather than
  splitting a shared multi-lane fixture (Customize and Bluetooth append through
  the same registry); the decomposition decision belongs to integration.
- `main` now carries the Customize and Bluetooth routes through the same
  Settings Center registry files. Per instructions I did not merge `main`;
  my registry edits stay append-only, and textual reconciliation in
  `settings_route.h`/`settings_route_registry.cpp`/`Main.qml`/
  `tests/apps/settings_center/*` is for the manager at integration.
- No stream-movement UI, per-channel balance, profile/port selection, or
  persistence (unchanged from the original candidate's scope).
- Degraded snapshots remain action-admitted and labeled, matching the public
  client's dispatch preflight (unchanged; documented on the owning page).
- No live AT-SPI, compositor focus, nested-session matrix, or physical audio
  hardware qualification.

## Requested next action

Independent exact recheck by Joan Clarke (OpenAI Codex) of commit
`c7a5b469c6c5a6efc77b4d203f5470cdbec67222` against base
`74da46345c7a5094d45c756ad8b23ca87591fcd3`, then manager integration.
