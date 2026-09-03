# Terminal S1 dialog non-closure repair handoff — Mary Coombs

- Exact product candidate: `00f2db99ce13df2f426abd93e277de3d33411041`
- Candidate tree: `843d9de4fe55aa61b29a9686cf3c4780d9842a32`
- Repair parent: `ff19cbe6dc05f5e7564407c04ca401315f03c4d3`
- Rejected product candidate repaired: `34439e80b6e66eaf47561a3234b99433d8cf4bcb`
- Exact lane base: `4c23978886689e06dde08805483b1495942dc017`
- Branch: `worker/terminal-s1`

## Changed product paths

- `docs/wiki/apps/terminal.md`
- `tests/apps/terminal/tst_terminal_profile_settings.cpp`

## Outcome

- Drives the production `TerminalWindow::manageProfiles()` path by triggering its real action and entering the offscreen modal loop; the proof no longer constructs an unshown dialog or infers liveness from the initially rejected result.
- Covers conflict, confirmed rejection, session-bus transport loss, Settings1 owner loss, and commit-timeout uncertainty. Every failure row asserts the dialog is still visible, modal, enabled, unfinished, and re-applicable, and that its visible status exposes the bounded per-key outcome through matching accessible text.
- Drives all three fake-transport commits and fresh snapshots to `allApplied`, then asserts that the dialog becomes hidden and Accepted. The failure-row live-dialog predicate is explicitly false for this control, proving that silent closure would be detected.
- Updates the Terminal verification documentation to name the production modal and exact negative/control coverage.

## Verification evidence

Both configurations used the required strict lane recipe and private KWin 6.6.5 cache:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: Debug exit 0; Release exit 0. CMake emitted the existing mixed-root runtime-search-path warnings.

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/<profile> \
  --parallel 3 --target \
  qindaqt-terminal \
  qindaqt_terminal_launch_policy_tests \
  qindaqt_terminal_pty_bridge_tests \
  qindaqt_terminal_session_tests \
  qindaqt_terminal_process_group_tests \
  qindaqt_terminal_appearance_tests \
  qindaqt_terminal_profiles_tests \
  qindaqt_terminal_session_collection_tests \
  qindaqt_terminal_profile_settings_tests \
  qindaqt_terminal_window_tests \
  qindaqt_terminal_app_shell_tests \
  qindaqt_terminal_tabs_tests \
  qindaqt_terminal_widget_adapter_tests \
  qindaqt_settings_schema_tests
```

Result: Debug exit 0; Release exit 0 under warnings-as-errors. The reused Debug Ninja log emitted its existing `premature end of file; recovering` warning and rebuilt successfully.

After the final test assertion change, the changed executable was rebuilt in each profile:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/<profile> \
  --parallel 3 --target qindaqt_terminal_profile_settings_tests
```

Result: Debug exit 0; Release exit 0 under warnings-as-errors.

For each of `debug` and `release`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/<profile> \
  -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
```

Final result: Debug exit 0, 15/15 passed in 10.55 s; Release exit 0, 15/15 passed in 11.10 s.

For each profile, the exact dialog cases were also run directly:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/<profile>/tests/apps/terminal/qindaqt_terminal_profile_settings_tests \
  productionDialogPresentsAsynchronousOutcome \
  productionDialogClosesOnlyAfterAllApplied -v1
```

Result: Debug exit 0, 8/8 QtTest cases passed; Release exit 0, 8/8 passed. Qt's offscreen plugin emitted its expected `propagateSizeHints()` warnings while showing the real dialogs.

An initial focused Debug run exited 1 with 11/12 QtTest cases because the test's conflict reply supplied `[]`, exactly matching the unchanged production draft; the Settings1 controller correctly treated that as applied and waited for refresh. The fixture was corrected to supply a genuinely differing authoritative value. The repeated focused Debug run then exited 0 with 12/12 cases, and all final dual-profile evidence above includes the corrected fixture.

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/site
./tools/check-source-shape
git diff --check
git clang-format --diff HEAD -- tests/apps/terminal/tst_terminal_profile_settings.cpp
```

Final result: all exit 0. Documentation validated 118 Markdown documents/navigation; strict MkDocs succeeded; source shape checked 1,836 files with 0 skipped and repeated two pre-existing out-of-lane 500/539-line decomposition warnings; the changed test is 490 non-blank lines; clang-format reported no changes; the diff was clean. No JSON changed in this repair, so no JSON syntax gate applied.

## Bounded caveats

- All dialog tests use Qt's offscreen platform and an injected fake Settings1 transport. No host display, D-Bus, hardware, uinput, network, or nested compositor was contacted or claimed.
- This repair strengthens executable acceptance proof only. It does not change the already-reviewed production behavior, Settings schema, process-group teardown, argv preservation, eight-session cap, or qtermwidget boundary.
- The failure rows deliberately reject the still-open dialog after observing it so the test can exit; only the production all-applied path closes it before test cleanup.

## Requested next action

Dina St Johnston (OpenAI Codex) should independently recheck exact product candidate `00f2db99ce13df2f426abd93e277de3d33411041`, especially the five production dialog outcome rows and the all-applied non-vacuity control. If accepted, the Program Manager should integrate that exact product commit.
