# Joan Clarke — Audio Settings exact-candidate repair recheck

| Field | Value |
|---|---|
| Persona | Joan Clarke (`joan-clarke`) |
| Provider / model | OpenAI Codex `gpt-5.6-sol`, reasoning high |
| Exact candidate SHA | `c7a5b469c6c5a6efc77b4d203f5470cdbec67222` |
| Tree SHA | `cd26b9d5bd945318dc684bbf4f64c1a1c4b86660` |
| Parent SHA | `2d49ab82e58b1bf79e8878b13f932706b7247580` |
| Base SHA | `74da46345c7a5094d45c756ad8b23ca87591fcd3` |
| Product ancestor under repair | `ce66a98dd4835048761142425253c72af89b77e2` |
| Review worktree | `/home/cabewse/work_SPaC3/container-wm-workers/audio-settings-route-codex-review` |
| Build root | `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex` |

## Findings ledger

### P0

None.

### P1

None. Both former P1 focus defects are closed on the exact candidate.

### P2

#### P2-1 — The integrated Audio navigation proof still does not run under the required fatal-warning mode and does not assert the documented tab name

The repair adds the Audio stub, Ctrl+5, loader activation, wide/compact Tab
entry, Escape return, and several accessibility-role assertions. The ordinary
Settings Center selector passes. However,
`tests/apps/settings_center/CMakeLists.txt:88-92` registers the navigation-page
row without `QT_FATAL_WARNINGS=1`. When that required setting is supplied, the
row aborts while constructing `Main.qml`, before either layout's new Audio
assertions execute:

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug \
  -R '^qindaqt\.settings-navigation-page$' \
  --output-on-failure --no-tests=error
```

Observed: exit 8, 0/1 passed. Release produces the same result. The fatal
message is:

```text
src/apps/settings_center/Main.qml:74:5: QML Shortcut: Shortcut: Only binding
to one of multiple key bindings associated with 65. Use 'sequences: [ <key> ]'
to bind to all of them.
```

Directly selecting each new layout-bearing function proves this is not merely
an earlier unrelated test function masking the Audio cases:

```sh
QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  <profile>/tests/apps/settings_center/qindaqt_settings_navigation_page_test \
  testKeyboardNavigationAndShortcuts

QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  <profile>/tests/apps/settings_center/qindaqt_settings_navigation_page_test \
  testCompactLayoutAdaptation
```

Observed: both functions abort with exit 134 on the same warning in both Debug
and Release, before their Audio assertions. Expected: both functions pass in
both profiles with warnings fatal. The warning originates from the pre-existing
`StandardKey.Quit` spelling at `Main.qml:74-76`, but it still makes the exact
candidate's newly required strict integration evidence unavailable; the
bounded correction advertised by Qt is to use the plural `sequences` property.

There is a second proof gap within the same prior P2 contract. The owning page
claims that the Settings Center row proves the Audio route tab's accessible
name and role (`docs/wiki/apps/audio-settings.md:126-128`). The compact case at
`tests/apps/settings_center/tst_settings_navigation_page.cpp:390-396` checks
only the tab's role. The wide case at lines 482-512 checks Slider and Heading
roles but never queries the Audio tab's accessible interface, name, role, or
selected state. This command finds the role-only page assertions and no
`QAccessible::Name` assertion:

```sh
rg -n 'audio.*Accessible|Accessible.*audio|QAccessible::Name|text\(QAccessible::Name\)' \
  tests/apps/settings_center/tst_settings_navigation_page.cpp
```

Observed: only the Audio volume and heading accessible-interface variables are
matched; no name assertion exists. Expected: the Audio tab's accessible name
and `PageTab` role are asserted in both host layouts, matching the documented
proof claim.

This remains P2 rather than P1: the ordinary integrated row passes 9/9 in both
profiles, the standalone hostile-snapshot control passes under fatal warnings,
and the exact external focus probe demonstrates the repaired product behavior.
The gap is bounded to strict-mode and accessible-name qualification.

### P3

None.

## Review-question evidence

1. **P1-1 is closed.** `AudioDeviceSection.qml` and
   `AudioStreamSection.qml` register each delegate's first and last enabled
   admitted action and recompute section targets when registration or enabled
   state changes. `AudioPage.qml:21-33` composes the section targets in visual
   order, and `SettingsRouteHost.qml:39-56` now reports the target's actual
   `activeFocus`. The exact prior snapshot-shaped probe exits 0 with
   `firstFocusTarget= audioOutputDefault_12 enabled= true` and
   `activeFocusItem= audioOutputDefault_12`. The registered negative control at
   `tst_audio_page.cpp:292-340` also verifies the later projection recomputation.
2. **P1-2 is closed.** `AudioPage.qml:268-271` sends reverse Tab from Close to
   visible Retry, otherwise the last admitted route action. The page test
   covers Retry hidden and visible (`tst_audio_page.cpp:266-289`), and the exact
   probe reports `reverseTabFromClose= audioOutputMute_12`.
3. **P2-1 is only partially closed.** The requested stub, Ctrl+5, wide/compact
   loaders, Escape, Tab entry, disabled-target page control, and role assertions
   exist and pass normally. The fatal-warning executions and Audio-tab name
   assertion remain missing/failing as reproduced above.
4. **The former P3s are closed.** `docs/wiki/architecture/audio-service.md`
   now truthfully records the implemented Settings route while preserving the
   hardware/nested-session and shell-applet non-claims. The construction timeout
   remains 25 seconds, but this is justified: the default Debug and Release
   construction rows each take about 15.1 seconds for five mandatory 3-second
   residency witnesses, so the former 15-second timeout is no longer viable.
   Product registry edits append Audio after Network and preserve prior route
   order; no route was removed or reordered. The timeout is the only non-additive
   shared-test edit and is supported by executed timing evidence.
5. **Required default selectors and static gates pass.** The detailed command
   ledger follows. No nested compositor/session test, live service, host bus,
   hardware, uinput, or network was used.

## Commands and results

All commands ran from the review worktree unless an absolute path is shown.

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-list --parents -n 1 HEAD
git status --porcelain
```

Result: exit 0. Candidate, tree, and sole parent match the metadata above.
`git merge-base HEAD 74da46345c7a5094d45c756ad8b23ca87591fcd3`
returned the stated base. Status was empty before review and after all review
execution. The candidate commit contains no `ops/team` path.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: Debug exit 0; Release exit 0. CMake emitted the existing nonfatal
runtime-search-path warnings and generated both trees.

### Focused builds

For both `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  --parallel 3 --target \
  qindaqt_settings_audio qindaqt_settings_audio_qml qindaqt-settings \
  qindaqt_settings_audio_model_tests \
  qindaqt_settings_audio_model_adversarial_tests \
  qindaqt_settings_audio_page_tests \
  qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test \
  qindaqt_settings_navigation_page_test \
  qindaqt_audio_protocol_tests qindaqt_audio_client_tests
```

Result: Debug exit 0; Release exit 0.

### Focused and adjacent tests

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.settings-audio-' --output-on-failure --no-tests=error
```

Result: Debug 5/5 passed, exit 0; Release 5/5 passed, exit 0.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Result: Debug 9/9 passed, exit 0; Release 9/9 passed, exit 0. Route
construction took 15.10 seconds Debug and 15.11 seconds Release; installed
routes took 15.49 seconds Debug and 15.45 seconds Release.

The same complete selector with `QT_FATAL_WARNINGS=1` returned exit 8 in both
profiles: 5/9 passed and the navigation page plus three child-process rows
failed because fatal warnings aborted their intended execution. The focused
navigation-page reproduction above returned 0/1, exit 8, in both profiles.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.audio-(protocol|client)$' \
  --output-on-failure --no-tests=error
```

Result: Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0.

### Exact prior focus probe

```sh
/usr/bin/c++ -std=c++20 -g -fPIC -no-pie \
  -I src/apps/settings/appearance/include -I src/themes/include \
  -I src/design_tokens/include \
  $(pkg-config --cflags Qt6Core Qt6Gui Qt6Qml Qt6Quick Qt6Test) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro.cpp \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/apps/settings/appearance/libqindaqt_settings_appearance.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens/libqindaqt_tokens_qml.so \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/themes/libqindaqt_themes.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens/libqindaqt_design_tokens.a \
  $(pkg-config --libs Qt6QuickControls2 Qt6Quick Qt6Qml Qt6Gui Qt6Test Qt6Core)
```

Result: exit 0.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
QML_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/qml \
LD_LIBRARY_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens \
/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro
```

Result: exit 0, with:

```text
firstFocusTarget= audioOutputDefault_12 enabled= true
activeFocusItem= audioOutputDefault_12
reverseTabFromClose= audioOutputMute_12
```

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/site
./tools/check-source-shape
git diff --check
git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..c7a5b469c6c5a6efc77b4d203f5470cdbec67222
git diff --name-only 74da46345c7a5094d45c756ad8b23ca87591fcd3..c7a5b469c6c5a6efc77b4d203f5470cdbec67222 -- '*.json'
```

Results: all commands exit 0. Documentation validation covered 117 Markdown
documents; strict MkDocs completed. Source-shape checked 1,773 files and emitted
the acknowledged 535-nonblank-line decomposition-review advisory for
`tst_settings_navigation_page.cpp` plus two unrelated existing advisories; the
gate passed. Both diff checks were clean. No JSON path changed, so
`python3 -m json.tool` was not applicable.

## Verdict

The repaired focus behavior and former P3 precision issues are closed, but the
explicitly required Settings Center fatal-warning and accessible-name proof is
still absent/failing. ACCEPT requires P2 = 0, so the exact candidate is rejected
for this bounded qualification defect.

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
