# Joan Clarke — Audio Settings second-repair exact-candidate recheck

| Field | Value |
|---|---|
| Persona | Joan Clarke (`joan-clarke`) |
| Provider / model | OpenAI Codex `gpt-5.6-sol`, reasoning high |
| Exact candidate SHA | `d10abe28974c7cde80fa1094e8ee60e64402ba5d` |
| Tree SHA | `9c9eb1829186d5ba870bc435df2c520aa14581c9` |
| Parent SHA | `1ee83155b37923b5618a2cea1700d80a1679db43` |
| Base SHA | `74da46345c7a5094d45c756ad8b23ca87591fcd3` |
| Product ancestor under repair | `c7a5b469c6c5a6efc77b4d203f5470cdbec67222` |
| Review worktree | `/home/cabewse/work_SPaC3/container-wm-workers/audio-settings-route-codex-review` |
| Build root | `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex` |

## Findings ledger

### P0

None.

### P1

None.

### P2

None. The sole P2 from the `c7a5b46` recheck is closed.

### P3

None.

## Recheck evidence

1. **P1-1 remains closed.** `AudioPage.qml:16-33` selects section targets in
   traversal order, and the device/stream sections register only enabled,
   admitted controls and refresh their targets when delegate admission changes.
   `SettingsRouteHost.qml:39-56` returns the target's actual `activeFocus`
   rather than assuming `forceActiveFocus()` succeeded. The registered exact-
   shape negative control at `tst_audio_page.cpp:292-340` passed in Debug and
   Release. The rebuilt external probe exited 0 under fatal warnings and
   reported:

   ```text
   firstFocusTarget= audioOutputDefault_12 enabled= true
   activeFocusItem= audioOutputDefault_12
   reverseTabFromClose= audioOutputMute_12
   ```

2. **P1-2 remains closed.** `tst_audio_page.cpp:266-289` asserts reverse Tab
   from Close reaches the last enabled stream control while Retry is hidden and
   reaches Retry after the unavailable projection makes it visible. The
   `keepsCompactFocusVisibleAndClosesTheCycle` function passed in both profiles;
   the external probe independently reproduced the Retry-hidden path.

3. **P2-1 is closed.** Relative to `c7a5b46`, `Main.qml` changes only the Quit
   shortcut block and now uses `sequences: [StandardKey.Quit]` at line 79.
   `tests/apps/settings_center/CMakeLists.txt:88-97` registers the navigation
   row with `QT_FATAL_WARNINGS=1`. Verbose CTest output in both profiles showed
   that exact environment and all six QtTest stages passing, including
   `testCompactLayoutAdaptation` and `testKeyboardNavigationAndShortcuts`.
   Those functions exercise the Audio stub, Ctrl+5, loader selection, Escape,
   Tab entry, and assert the Audio tab's `PageTab` role, exact `Audio` name, and
   selected state in compact (`tst_settings_navigation_page.cpp:381-407`) and
   wide (`:485-522`) layouts. The standalone Audio page row supplies the
   disabled-target projection-change control and passed with fatal warnings.

4. **The former P3 concerns remain closed.** The Audio service consumer text at
   `docs/wiki/architecture/audio-service.md:132-142` truthfully describes the
   implemented route while retaining the physical-hardware, nested-session,
   and shell-applet non-claims. The construction timeout remains 25 seconds at
   `tests/apps/settings_center/CMakeLists.txt:145-154`; this is justified by the
   observed five-route construction times of 15.11 seconds (Debug) and 15.12
   seconds (Release), which cannot fit the former 15-second limit. The installed
   rows took 15.52 and 15.48 seconds. The candidate does not touch the registry
   relative to `c7a5b46`; against the original base, Audio is appended after
   Network without removing or reordering an existing route.

5. The descendant is bounded to four files: 30 insertions and 6 deletions in
   `Main.qml`, the navigation test registration, navigation assertions, and the
   owning Settings Center wiki page. No JSON changed. No nested-compositor or
   `tests/session` row, host D-Bus service, hardware, uinput, or network was run.

## Commands and results

All commands ran from the review worktree unless an absolute path is shown.

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base 74da46345c7a5094d45c756ad8b23ca87591fcd3 HEAD
git status --porcelain
```

Exit 0. Candidate, tree, parent, and base matched the table. Status was empty
before review and after all review execution.

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

Debug exit 0; Release exit 0. CMake emitted the existing nonfatal library
runtime-search-path warnings.

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

Debug exit 0; Release exit 0.

### Focused and adjacent tests

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.settings-audio-' --output-on-failure --no-tests=error
```

Debug 5/5 passed, exit 0; Release 5/5 passed, exit 0.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -V -R '^qindaqt\.settings-navigation-page$' --no-tests=error
```

Debug 1/1 passed (six QtTest stages), exit 0; Release 1/1 passed (six
QtTest stages), exit 0. Both verbose runs showed `QT_FATAL_WARNINGS=1` in the
registered environment.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Debug 9/9 passed, exit 0; Release 9/9 passed, exit 0. The navigation row ran
fatal through its registered environment.

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -V -R '^qindaqt\.settings-audio-page$' --no-tests=error
```

Debug 1/1 passed (nine QtTest stages), exit 0; Release 1/1 passed (nine
QtTest stages), exit 0. Both runs executed the reverse-Tab and disabled-target
test functions.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.audio-(protocol|client)$' \
  --output-on-failure --no-tests=error
```

Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0.

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

QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
QML_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/qml \
LD_LIBRARY_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens \
/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro
```

Compile exit 0; probe exit 0 with the exact output recorded above.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/site
./tools/check-source-shape
git diff --check
git diff --check 74da46345c7a5094d45c756ad8b23ca87591fcd3..d10abe28974c7cde80fa1094e8ee60e64402ba5d
git diff --name-status 74da46345c7a5094d45c756ad8b23ca87591fcd3..HEAD -- '*.json'
```

All commands exited 0. Documentation validation covered 117 Markdown files;
strict MkDocs completed. Source shape checked 1,773 files and emitted the
acknowledged 545-nonblank-line decomposition-review advisory for
`tst_settings_navigation_page.cpp` plus two unrelated existing advisories; the
gate passed. Both diff checks were clean. The JSON query was empty, so
`python3 -m json.tool` was not applicable.

## Verdict

The exact descendant closes the last strict navigation/accessibility proof gap,
all earlier behavior and precision repairs remain intact, and no P0-P3 defect
was found. The candidate is accepted for manager integration.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
