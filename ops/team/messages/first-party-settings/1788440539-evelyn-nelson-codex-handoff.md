# Clipboard Settings route candidate handoff

- Worker: Evelyn Nelson-Codex (`evelyn-nelson-codex`)
- Time: `2026-09-03T07:02:19-06:00`
- Exact base: `753ea20ec6ad556772d63584f4c6840b1c668e12`
- Candidate commit: `6a2d7f45972d620fcccc8e038e01a416ced3a1cd`
- Candidate tree: `8a04a26c61c586589abe2d67c5a801b1c3f8a73b`
- Requested next action: independent exact review, then manager integration.

## Changed paths

- `docs/wiki/apps/clipboard-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/apps/settings/clipboard/CMakeLists.txt`
- `src/apps/settings/clipboard/clipboard_route_composition.cpp`
- `src/apps/settings/clipboard/clipboard_route_composition.h`
- `src/apps/settings/clipboard/clipboard_settings_model.cpp`
- `src/apps/settings/clipboard/clipboard_settings_service.cpp`
- `src/apps/settings/clipboard/include/qindaqt/apps/settings_clipboard/clipboard_settings_model.h`
- `src/apps/settings/clipboard/qml/ClipboardPage.qml`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `src/apps/settings_center/settings_route.cpp`
- `src/apps/settings_center/settings_route.h`
- `src/apps/settings_center/settings_route_registry.cpp`
- `tests/CMakeLists.txt`
- `tests/apps/settings/clipboard/CMakeLists.txt`
- `tests/apps/settings/clipboard/check_boundary.cmake`
- `tests/apps/settings/clipboard/check_boundary_negative.cmake`
- `tests/apps/settings/clipboard/check_installed_route.cmake`
- `tests/apps/settings/clipboard/clipboard_settings_center_assertions.h`
- `tests/apps/settings/clipboard/clipboard_settings_test_support.h`
- `tests/apps/settings/clipboard/stub_clipboard_settings_model.h`
- `tests/apps/settings/clipboard/tst_clipboard_page.cpp`
- `tests/apps/settings/clipboard/tst_clipboard_settings_preference.cpp`
- `tests/apps/settings/clipboard/tst_clipboard_settings_service.cpp`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/check_installed_routes.cmake`
- `tests/apps/settings_center/check_route_construction.cmake`
- `tests/apps/settings_center/tst_settings_navigation_controller.cpp`
- `tests/apps/settings_center/tst_settings_navigation_page.cpp`
- `tests/apps/settings_center/tst_settings_route_registry.cpp`

## Acceptance evidence

All commands ran from the assigned worktree. Product tests removed host display
variables, removed the inherited session-bus address, and set both bus endpoints
to nonexistent sockets. No nested/session, host clipboard, hardware, uinput, or
network test was run.

- Debug configure, exit 0:
  `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
- Release configure, exit 0:
  `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
- Debug focused build, exit 0:
  `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug --parallel 3 --target qindaqt_settings_clipboard qindaqt_settings_clipboard_qml qindaqt_clipboard_settings_preference_tests qindaqt_clipboard_settings_service_tests qindaqt_clipboard_page_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test`
- Release focused build, exit 0:
  `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/release --parallel 3 --target qindaqt_settings_clipboard qindaqt_settings_clipboard_qml qindaqt_clipboard_settings_preference_tests qindaqt_clipboard_settings_service_tests qindaqt_clipboard_page_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test`
- Debug Clipboard selector, exit 0, 6/6 passed:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -R '^qindaqt\.settings-clipboard-' --output-on-failure --no-tests=error`
- Release Clipboard selector, exit 0, 6/6 passed:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/release -R '^qindaqt\.settings-clipboard-' --output-on-failure --no-tests=error`
- Debug Settings Center selector, exit 0, 9/9 passed:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' --output-on-failure --no-tests=error`
- Release Settings Center selector, exit 0, 9/9 passed:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/release -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' --output-on-failure --no-tests=error`
- `./tools/validate-docs`, exit 0: 131 Markdown documents and `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/site`, exit 0.
- `./tools/check-source-shape`, exit 0: 2,152 source files checked, none skipped. It emitted decomposition-review warnings only: this route's 312-nonblank-line QML page, the 279-line shared Main.qml, the now exactly 600-line pre-existing shared navigation-page test, and three unrelated existing files.
- `git diff --check`, exit 0.
- JSON validation was not applicable because the candidate changes no JSON.

## Bounded caveats and integration note

- This is injected-fake, absent-bus, warning-fatal offscreen, and relocated-package evidence. It does not claim live Wayland capture, a real clipboard/data-control server, host D-Bus, hardware/uinput, live AT-SPI/screen-reader traversal, or nested-session screenshots.
- The page deliberately has no content-read/paste or individual-entry authority, no persistent history, and no configurable retention setting because schemas v1/v2 define only the default-false `services.clipboardHistory` Boolean.
- The statically composed Clipboard QML module is required and inspected in the relocated stage; content-authority failure is proved independently by the positive/negative allow-list poison rather than by withholding that static module.
- The active Power lane touches the same append-only route registries and will own `Ctrl+8`. During integration, preserve Power before Clipboard in final route traversal, preserve Clipboard's `Ctrl+9`, and update final route counts/indices additively without rewriting either route's model or QML boundary.
- `settings_route.cpp` contains the two mechanically necessary closed-enum validation/key cases corresponding to the owned `settings_route.h` enum addition; no bootstrap `main.cpp`, Clipboard service/client implementation, shell, compositor, or other lane path was changed.
