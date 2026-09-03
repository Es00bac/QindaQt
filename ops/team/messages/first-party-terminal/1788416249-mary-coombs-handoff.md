# Terminal S1 replacement handoff

- Worker: Mary Coombs (`mary-coombs`)
- Time: 2026-09-03T00:17:29-06:00
- Exact base: `4c23978886689e06dde08805483b1495942dc017`
- Preserved WIP: `832077f56be9540c5c6645ed98b307112b4753ca`
- Candidate commit: `08481f496438ec112753fef30a797db5a19af654`
- Candidate tree: `ab51077cd0241eaddf20872bbf3e30335debd621`

## Changed paths from the exact base

- `data/settings/schema-v2.json`
- `docs/wiki/apps/terminal.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/settings1-v1.md`
- `src/apps/terminal/CMakeLists.txt`
- `src/apps/terminal/app_shell/terminal_action_catalog.cpp`
- `src/apps/terminal/app_shell/terminal_action_catalog.h`
- `src/apps/terminal/app_shell/terminal_app_shell_bridge.cpp`
- `src/apps/terminal/app_shell/terminal_app_shell_bridge.h`
- `src/apps/terminal/main.cpp`
- `src/apps/terminal/profiles/terminal_profile.cpp`
- `src/apps/terminal/profiles/terminal_profile.h`
- `src/apps/terminal/profiles/terminal_profile_settings.cpp`
- `src/apps/terminal/profiles/terminal_profile_settings.h`
- `src/apps/terminal/session/terminal_session.cpp`
- `src/apps/terminal/session/terminal_session.h`
- `src/apps/terminal/session/terminal_session_collection.cpp`
- `src/apps/terminal/session/terminal_session_collection.h`
- `src/apps/terminal/ui/terminal_profile_dialog.cpp`
- `src/apps/terminal/ui/terminal_profile_dialog.h`
- `src/apps/terminal/ui/terminal_tab_bar.cpp`
- `src/apps/terminal/ui/terminal_tab_bar.h`
- `src/apps/terminal/ui/terminal_widget_adapter.cpp`
- `src/apps/terminal/ui/terminal_widget_adapter.h`
- `src/apps/terminal/ui/terminal_widget_adapter_appearance.cpp`
- `src/apps/terminal/ui/terminal_window.cpp`
- `src/apps/terminal/ui/terminal_window.h`
- `src/apps/terminal/ui/terminal_window_actions.cpp`
- `tests/apps/terminal/CMakeLists.txt`
- `tests/apps/terminal/tst_terminal_app_shell.cpp`
- `tests/apps/terminal/tst_terminal_profile_settings.cpp`
- `tests/apps/terminal/tst_terminal_profiles.cpp`
- `tests/apps/terminal/tst_terminal_session.cpp`
- `tests/apps/terminal/tst_terminal_session_collection.cpp`
- `tests/apps/terminal/tst_terminal_tabs.cpp`
- `tests/apps/terminal/tst_terminal_widget_adapter.cpp`
- `tests/apps/terminal/tst_terminal_window.cpp`

The preserved WIP had also modified `src/app_shell/CMakeLists.txt`, the shared
Settings domain enum/codec, and `tests/settings/tst_settings_schema.cpp`.
Candidate `08481f4` restores all four paths byte-for-byte to the exact base;
they therefore do not appear in the candidate's base-to-tip path list.

## Acceptance evidence

- Debug configure, exact lane recipe with build root
  `/home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/debug`: exit 0.
- Release configure, exact lane recipe with build root
  `/home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/release`: exit 0.
- `cmake --build .../debug --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests`: exit 0.
- The identical focused Release build: exit 0.
- `ctest --test-dir .../debug -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error`: exit 0, 14/14 passed.
- The identical Release terminal selector: exit 0, 14/14 passed.
- Initial dependency-adjacent `qindaqt.settings-schema` runs in Debug and
  Release: exit 8, 0/1 CTest rows passed. They exposed the WIP's invalid
  `terminal.*` keys under the `services` domain. The candidate fixes this by
  using `services.terminalProfiles`, `services.terminalDefaultProfile`, and
  `services.terminalRestoreTabs`, without widening Settings core.
- `cmake --build .../{debug,release} --parallel 3 --target qindaqt-terminal qindaqt_terminal_profile_settings_tests qindaqt_settings_schema_tests` followed by `ctest -R '^(qindaqt\.terminal-|qindaqt\.settings-schema$)'`: exit 0 in each configuration, 15/15 passed in Debug and 15/15 passed in Release.
- `./tools/validate-docs`: exit 0, 118 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s1/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,833 files checked and none skipped; two pre-existing out-of-lane decomposition warnings only.
- `git diff --check`: exit 0.
- `python3 -m json.tool data/settings/schema-v2.json > /dev/null`: exit 0.
- `git diff --exit-code 4c23978886689e06dde08805483b1495942dc017 08481f496438ec112753fef30a797db5a19af654 -- src/app_shell/CMakeLists.txt src/settings/include/qindaqt/settings/settings_types.h src/settings/src/settings_types.cpp tests/settings/tst_settings_schema.cpp`: exit 0.

## Bounded caveats

- Persistence proof uses the injected fake Settings1 transport. No host or
  live session D-Bus service was contacted.
- This lane does not persist tab inventory, argv, title, scrollback, or session
  content; `services.terminalRestoreTabs` persists policy only.
- Global-menu export, search/links, nested screenshot and private-Wayland
  matrices, physical display/input, GPU qualification, host services, and
  uinput remain explicitly outside this candidate. No such evidence is
  claimed.
- The Debug Ninja log reported a recoverable premature-EOF warning and rebuilt
  successfully; both subsequent test suites and all gates exited 0.

Requested next action: independent exact review then manager integration.
