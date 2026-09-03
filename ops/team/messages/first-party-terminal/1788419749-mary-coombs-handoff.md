# Terminal S1 repair handoff — Mary Coombs

- Exact product candidate: `34439e80b6e66eaf47561a3234b99433d8cf4bcb`
- Candidate tree: `fb72ee0c23b5ac5d50450e8160b346ed7ba241e9`
- Repair parent: `436461c5b2f9726abd747339f1c15e204693e814`
- Rejected predecessor repaired: `08481f496438ec112753fef30a797db5a19af654`
- Exact lane base: `4c23978886689e06dde08805483b1495942dc017`
- Branch: `worker/terminal-s1`

## Changed product paths

- `docs/wiki/apps/terminal.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `src/apps/terminal/CMakeLists.txt`
- `src/apps/terminal/session/process_liveness.cpp`
- `src/apps/terminal/session/process_liveness.h`
- `src/apps/terminal/session/terminal_session.cpp`
- `src/apps/terminal/session/terminal_session.h`
- `src/apps/terminal/ui/terminal_profile_apply_status.cpp`
- `src/apps/terminal/ui/terminal_profile_apply_status.h`
- `src/apps/terminal/ui/terminal_profile_dialog.cpp`
- `src/apps/terminal/ui/terminal_profile_dialog.h`
- `src/apps/terminal/ui/terminal_window.cpp`
- `src/apps/terminal/ui/terminal_window.h`
- `src/apps/terminal/ui/terminal_window_actions.cpp`
- `tests/apps/terminal/CMakeLists.txt`
- `tests/apps/terminal/tst_terminal_process_group.cpp`
- `tests/apps/terminal/tst_terminal_profile_settings.cpp`
- `tests/apps/terminal/tst_terminal_profiles.cpp`
- `tests/apps/terminal/tst_terminal_session.cpp`
- `tests/apps/terminal/tst_terminal_session_collection.cpp`
- `tests/apps/terminal/tst_terminal_tabs.cpp`
- `tests/apps/terminal/tst_terminal_window.cpp`

## Outcome

- Retains the `setsid()`-created process-group id after the direct child is reaped, refuses replacement while the group remains owned, escalates HUP → TERM → KILL against that retained group, and publishes clean completion only after `killpg(pgid, 0)` plus a fail-closed `/proc` scan prove that no live or zombie member remains.
- Registers Dina St Johnston's hostile descendant shape as `qindaqt.terminal-process-group`; its fixture reaps the leader while an HUP/TERM-immune descendant survives, verifies replacement refusal and eventual SIGKILL cleanup, and kills survivors on every failure path.
- Makes the production window and Manage Profiles dialog consume the asynchronous three-key Settings1 ledger. Success closes the dialog only after confirmation; conflict, confirmed rejection, transport loss, and uncertain no-replay results remain visible and accessible with every key named.
- Preserves unchanged profile argv exactly, including leading, interior, trailing, or sole empty elements.
- Corrects the wiki index's stale single-session description and records the repaired contracts and selectors.

## Verification evidence

Both configurations used the required cache and strict options:

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

Result: Debug exit 0; Release exit 0. CMake emitted the pre-existing mixed-root runtime-search-path warnings.

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

Final result: Debug exit 0; Release exit 0 under warnings-as-errors. A pre-final Release build correctly failed on an ignored `write(2)` return in the new hostile fixture (exit 1); the return is now checked, and the complete final build above passed. The reused Debug Ninja log continued to report its pre-existing `premature end of file; recovering` warning and successfully rebuilt every selected target.

For each of `debug` and `release`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/<profile> \
  -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error

env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/<profile> \
  -R '^qindaqt\.settings-schema$' --output-on-failure --no-tests=error
```

Result: Debug terminal exit 0, 15/15 passed; Debug schema exit 0, 1/1 passed; Release terminal exit 0, 15/15 passed; Release schema exit 0, 1/1 passed.

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/site
./tools/check-source-shape
git diff --check
git clang-format --diff HEAD -- src/apps/terminal tests/apps/terminal
```

Result: all exit 0; documentation validated 118 Markdown documents/navigation, strict MkDocs succeeded, source shape checked 1,836 files with 0 skipped, and formatting/diff checks were clean. Source shape repeated two pre-existing out-of-lane decomposition warnings at 500/539 lines; the largest owned file is 491 non-blank lines. No JSON changed in this repair, so no JSON syntax gate applied.

## Bounded caveats

- Product tests used injected Settings1 transport fakes and offscreen Qt only; this candidate claims no host D-Bus, host display, hardware, uinput, or nested-compositor evidence.
- The emptiness fallback intentionally depends on Linux `/proc`; missing, unreadable, or malformed inspection fails closed as uncertain and retains ownership rather than claiming clean shutdown.
- This repairs the four exact review findings without changing the eight-session cap, persistence schema, AppShell catalog, qtermwidget private adapter boundary, or session-content non-persistence contract.

## Requested next action

Dina St Johnston (OpenAI Codex) should independently recheck exact product candidate `34439e80b6e66eaf47561a3234b99433d8cf4bcb`, including both supplied reproductions and the new registered process-group row. If accepted, the Program Manager should integrate that exact product commit.
