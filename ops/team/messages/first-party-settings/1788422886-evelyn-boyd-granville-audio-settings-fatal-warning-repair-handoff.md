# Evelyn Boyd Granville — Audio Settings fatal-warning navigation proof repair handoff

| Field | Value |
|---|---|
| Persona | Evelyn Boyd Granville (`evelyn-boyd-granville`) |
| Provider / model | Z.AI `zai-coding-plan/glm-5.3`, reasoning high |
| Candidate commit | `d10abe28974c7cde80fa1094e8ee60e64402ba5d` |
| Candidate tree | `9c9eb1829186d5ba870bc435df2c520aa14581c9` |
| Parent | `1ee83155b37923b5618a2cea1700d80a1679db43` (prior coordination commit) |
| Product ancestor under repair | `c7a5b469c6c5a6efc77b4d203f5470cdbec67222` (Joan Clarke's REJECT 0/0/1/0) |
| Exact base | `74da46345c7a5094d45c756ad8b23ca87591fcd3` |
| Branch | `worker/audio-settings-route` |
| Worktree | `/home/cabewse/work_SPaC3/container-wm-workers/audio-settings-route` |
| Build root | `/home/cabewse/work_SPaC3/builds/qindaqt/audio-settings-route` |

## What changed (sorted)

- `docs/wiki/apps/settings-center.md` — the verification section now records
  that the offscreen navigation row is registered with `QT_FATAL_WARNINGS=1`
  and why the child-process construction/installed rows are deliberately not.
- `src/apps/settings_center/Main.qml` — the Quit `Shortcut` now uses the
  plural `sequences: [StandardKey.Quit]` form (plus an `AGENT-GUARD` keeping
  it plural). This is the entire Main.qml edit, kept to the shortcut block so
  it merges cleanly onto `main`, which carries the Customize and Bluetooth
  routes.
- `tests/apps/settings_center/CMakeLists.txt` — the
  `qindaqt.settings-navigation-page` row's `ENVIRONMENT` gains
  `QT_FATAL_WARNINGS=1`, with an `AGENT-GUARD` comment.
- `tests/apps/settings_center/tst_settings_navigation_page.cpp` — the Audio
  route tab's accessible name (`Audio`), `PageTab` role, and selected state
  are now asserted in the compact layout (`settingsCompactTab_audio`) and the
  wide layout (`settingsNavButton_audio`), closing the second half of P2-1.

## How this closes P2-1

1. Fatal-warning proof: the row now runs with `QT_FATAL_WARNINGS=1` from its
   registered CMake `ENVIRONMENT`, and `Main.qml` constructs without the
   `QML Shortcut: Only binding to one of multiple key bindings associated
   with 65` warning, so all four test functions — including both new-Audio
   layout functions — execute and pass in the required mode.
2. Accessible-name proof: `QAccessible::Name` for the Audio tab is asserted
   equal to the documented tab title `Audio` in both host layouts, alongside
   the `PageTab` role (and truthful selected state after Ctrl+5), matching
   the `docs/wiki/apps/audio-settings.md` verification claim.

Bounded registration decision: the child-process rows
(`settings-app-route-construction`, `settings-app-installed-routes`) are not
registered fatal. Under an explicitly fatal environment they abort on
`main()`'s deliberate `qindaqt-settings: Settings1 client unavailable`
diagnostic against the absent sandbox bus — behavior those rows are designed
to witness — not on the repaired QML warning. The `settings-app-offscreen`
row constructs only `NotificationsPage` (no `Main.qml`), and the
registry/controller rows are pure C++; none shares the repaired issue. This
is recorded on the owning wiki page and in an `AGENT-GUARD` in the test
CMakeLists.

## Commands and results (all run from the worktree; ROOT is the build root)

### Configure (reuse)

```sh
cmake -S . -B ROOT/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B ROOT/release ... (same with Release)
```

Result: Debug exit 0; Release exit 0.

### Focused builds

```sh
cmake --build ROOT/<profile> --parallel 3 --target \
  qindaqt_settings_audio qindaqt_settings_audio_qml qindaqt-settings \
  qindaqt_settings_audio_model_tests qindaqt_settings_audio_model_adversarial_tests \
  qindaqt_settings_audio_page_tests qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test \
  qindaqt_audio_protocol_tests qindaqt_audio_client_tests
```

Result: Debug exit 0; Release exit 0 (strict warnings enabled).

### The repaired row, registered fatal environment

```sh
ctest --test-dir ROOT/<profile> -R '^qindaqt\.settings-navigation-page$' \
  --output-on-failure --no-tests=error
```

Result: Debug 1/1 passed, exit 0; Release 1/1 passed, exit 0. The verbose run
shows `QT_FATAL_WARNINGS=1` applied and
`SettingsNavigationPageTest::{initTestCase,testWideTwoColumnLayoutAndRouteSwitching,testCompactLayoutAdaptation,testKeyboardNavigationAndShortcuts,testUnavailableRouteFailClosed,cleanupTestCase}`
all PASS.

### Hostile control (fails on the unrepaired tree)

With the singular `sequence: StandardKey.Quit` spelling temporarily restored
in `Main.qml` (then reverted back), the same registered row returned
0/1 passed (1 failed) in Debug — the registration is not vacuous.

### Reviewer's exact direct probes

```sh
QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ROOT/<profile>/tests/apps/settings_center/qindaqt_settings_navigation_page_test \
  testKeyboardNavigationAndShortcuts   # and testCompactLayoutAdaptation
```

Result: all four combinations exit 0 with PASS lines (reviewer observed exit
134 on the rejected candidate).

### Required selectors

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir ROOT/<profile> \
  -R '^qindaqt\.settings-audio-' --output-on-failure --no-tests=error
```

Result: Debug 5/5 passed, exit 0; Release 5/5 passed, exit 0.

```sh
ctest --test-dir ROOT/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Result: Debug 9/9 passed, exit 0; Release 9/9 passed, exit 0 (navigation-page
row fatal via registration).

```sh
ctest --test-dir ROOT/<profile> -R '^qindaqt\.audio-(protocol|client)$' \
  --output-on-failure --no-tests=error
```

Result: Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0.

### Registration-decision evidence

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir ROOT/debug \
  -R '^qindaqt\.settings-app-route-construction$' --output-on-failure --no-tests=error
QT_FATAL_WARNINGS=1 ctest --test-dir ROOT/debug \
  -R '^qindaqt\.settings-app-installed-routes$' --output-on-failure --no-tests=error
```

Result: 0/1 each; failure output is `qindaqt-settings: Settings1 client
unavailable: settings session D-Bus is …` from `main()`'s deliberate
absent-bus diagnostic — the documented reason those rows stay non-fatal.

### Static gates

```sh
./tools/validate-docs                                        # exit 0 (117 documents)
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir ROOT/site                                       # exit 0
./tools/check-source-shape                                   # exit 0; the acknowledged
                                                             # decomposition-review advisory for
                                                             # tst_settings_navigation_page.cpp moves
                                                             # 535 → 545 non-blank (< 600 hard limit)
git diff --check                                             # exit 0
```

No JSON file changed, so `python3 -m json.tool` was not applicable. No
nested-compositor row, host D-Bus service, hardware, uinput, or network was
used; all QML rows ran offscreen with the software backend.

## Remaining bounded caveats

- The child-process construction/installed rows and the QuickTest
  notifications-page row are not fatal-warning rows, for the deliberate
  absent-bus diagnostic reasons recorded above; this candidate does not claim
  strict-mode coverage for them.
- `main` carries the Customize and Bluetooth routes; the `Main.qml` hunk is
  limited to the Quit shortcut block (plus its guard comment) so the manager
  can merge it without taking unrelated Settings Center edits from this
  branch.
- Everything else in the rejected candidate's scope is unchanged; this
  commit is a bounded descendant, not a rebase or amend.

## Requested next action

Independent exact review of `d10abe28974c7cde80fa1094e8ee60e64402ba5d`
(same reviewer, Joan Clarke / Codex), then manager integration.
