# Vera Molnar — Terminal S2 candidate handoff

- Candidate commit: `0949cb985c1a3589135c42f330ae6630fb0f5573`
- Candidate tree: `baac8037333c5c96ffd7728e117955d779320f80`
- Exact base: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Branch: `worker/terminal-s2`

## Changed paths

- `docs/wiki/adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md`
- `docs/wiki/apps/terminal.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/terminal/CMakeLists.txt`
- `src/apps/terminal/app_shell/terminal_action_catalog.cpp`
- `src/apps/terminal/app_shell/terminal_action_catalog.h`
- `src/apps/terminal/links/terminal_link.cpp`
- `src/apps/terminal/links/terminal_link.h`
- `src/apps/terminal/links/terminal_link_opener.cpp`
- `src/apps/terminal/links/terminal_link_opener.h`
- `src/apps/terminal/main.cpp`
- `src/apps/terminal/search/terminal_search.cpp`
- `src/apps/terminal/search/terminal_search.h`
- `src/apps/terminal/session/terminal_session.cpp`
- `src/apps/terminal/session/terminal_session.h`
- `src/apps/terminal/session/terminal_session_backend.cpp`
- `src/apps/terminal/session/terminal_session_backend.h`
- `src/apps/terminal/ui/terminal_find_bar.cpp`
- `src/apps/terminal/ui/terminal_find_bar.h`
- `src/apps/terminal/ui/terminal_widget_adapter.cpp`
- `src/apps/terminal/ui/terminal_widget_adapter.h`
- `src/apps/terminal/ui/terminal_widget_adapter_search.cpp`
- `src/apps/terminal/ui/terminal_window.cpp`
- `src/apps/terminal/ui/terminal_window.h`
- `src/apps/terminal/ui/terminal_window_actions.cpp`
- `src/apps/terminal/ui/terminal_window_links.cpp`
- `src/apps/terminal/ui/terminal_window_search.cpp`
- `src/apps/terminal/ui/terminal_window_status.cpp`
- `tests/apps/terminal/CMakeLists.txt`
- `tests/apps/terminal/tst_terminal_adapter_boundary.cpp`
- `tests/apps/terminal/tst_terminal_app_shell.cpp`
- `tests/apps/terminal/tst_terminal_links.cpp`
- `tests/apps/terminal/tst_terminal_search.cpp`
- `tests/apps/terminal/tst_terminal_search_links_ui.cpp`
- `tests/apps/terminal/tst_terminal_widget_adapter.cpp`
- `tests/apps/terminal/tst_terminal_window.cpp`

## Acceptance evidence

All commands ran from `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s2` unless the command supplies its own directory.

- Debug configuration, exit 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- Release configuration, exit 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- Debug complete focused target build, exit 0:

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/debug --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_process_group_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_search_tests qindaqt_terminal_links_tests qindaqt_terminal_search_links_ui_tests qindaqt_terminal_adapter_boundary_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests
  ```

- Release complete focused target build, exit 0:

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/release --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_process_group_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_search_tests qindaqt_terminal_links_tests qindaqt_terminal_search_links_ui_tests qindaqt_terminal_adapter_boundary_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests
  ```

- Debug final incremental build, exit 0; this rebuilt the last search policy/test edit and its production-adapter consumer:

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/debug --parallel 3 --target qindaqt-terminal qindaqt_terminal_search_tests qindaqt_terminal_widget_adapter_tests
  ```

- Release final incremental build, exit 0; the same last-edit targets were rebuilt after the complete focused target set:

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/release --parallel 3 --target qindaqt-terminal qindaqt_terminal_search_tests qindaqt_terminal_widget_adapter_tests
  ```

- Debug full Terminal selector, exit 0, 19/19 passed:

  ```sh
  env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/debug -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
  ```

- Release full Terminal selector, exit 0, 19/19 passed:

  ```sh
  env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/release -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
  ```

- `./tools/validate-docs`, exit 0: 128 Markdown documents and `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/site`, exit 0.
- `./tools/check-source-shape`, exit 0: 2,074 source files checked; the four reported decomposition warnings predate this candidate and none is in Terminal-owned production code (the existing `tst_terminal_window.cpp` remains below the limit at 496 non-blank lines).
- `git diff --cached --check`, exit 0.
- No JSON was changed, so no JSON parser gate applies.

## Bounded caveats

- This candidate deliberately does not claim GPU/rendering-performance qualification, global-menu export, whole-application assistive-technology proof, the nested screenshot/session matrix, or physical display/input qualification.
- It detects printed `http(s)` URLs and absolute local paths in the bounded live-screen tail. It does not interpret OSC-8 semantics and never click-activates a target.
- Production opening is intentionally a detached desktop-dispatch request after exact-target confirmation; it is not a PTY child or owned terminal job. Tests use only injected recording seams and never invoke a real `xdg-open`.
- No host bus, host display, nested compositor, hardware, uinput, or network evidence is claimed.

## Requested next action

Independent exact review of candidate `0949cb985c1a3589135c42f330ae6630fb0f5573`, then Program Manager integration.
