# Portal P1 candidate handoff — Mary Kenneth Keller

- Timestamp: 2026-09-02T22:34:20-06:00
- Candidate commit: `c33b4908f99cb1dfac04287383441d028ab8f25b`
- Candidate tree: `bfd3464afaa46af6b2b4f4a3cce1a603008b4c3d`
- Exact base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Branch: `worker/portal-p1-selection`
- Requested next action: independent exact review then manager integration.

## Outcome

The QindaQt desktop now selects its own Settings backend through the real
`xdg-desktop-portal` frontend, propagates exact appearance reads and live
changes, exposes Qt 6 `xdgdesktopportal` color-scheme reaction offscreen, and
routes reviewed non-Settings families through the explicit `kde;gtk;lxqt`
order while `default=none` and Background remain closed. The staged package
repeats those proofs and checks exact singleton integration artifacts.

## Changed paths

- `README.md`
- `docs/wiki/adr/0057-route-unimplemented-portal-families-explicitly.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/portal-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/portal-settings-backend-v1.md`
- `mkdocs.yml`
- `src/services/portal/data/qindaqt-portals.conf`
- `tests/services/portal/CMakeLists.txt`
- `tests/services/portal/check_boundary.cmake`
- `tests/services/portal/check_boundary_negative.cmake`
- `tests/services/portal/portal_frontend_test_support.cpp`
- `tests/services/portal/portal_frontend_test_support.h`
- `tests/services/portal/portal_toolkit_probe.cpp`
- `tests/services/portal/run_staged_package.cmake`
- `tests/services/portal/tst_portal_frontend_integration.cpp`

## Verification evidence

All commands below exited 0.

- Debug configure: `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`.
- Release configure: the same command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/release -DCMAKE_BUILD_TYPE=Release`.
- Debug focused build: `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/debug --parallel 3 --target qindaqt_portal_toolkit_probe qindaqt_portal_frontend_integration_tests xdg-desktop-portal-qindaqt qindaqt_portal_appearance_policy_tests qindaqt_portal_settings_source_tests qindaqt_portal_service_tests qindaqt_portal_process_lifecycle_tests`.
- Release focused build: the same command under `/home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/release`.
- Debug: `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/debug -R '^qindaqt\.portal-' --output-on-failure --no-tests=error` — 9/9 passed.
- Release: `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/release -R '^qindaqt\.portal-' --output-on-failure --no-tests=error` — 9/9 passed.
- `./tools/validate-docs` — validated 118 Markdown documents and navigation.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/portal-p1-selection/site` — strict build passed.
- `./tools/check-source-shape` — passed across 1,793 files; only two pre-existing threshold warnings outside owned paths were reported.
- `git diff --check` and `git diff --cached --check` — passed.
- No JSON file changed, so no JSON parser gate applied.

## Bounded caveats

- QindaQt still implements only `org.freedesktop.impl.portal.Settings`; no new
  portal family or consent UI is introduced.
- FileChooser proof uses an injected backend object at the staged KDE provider
  name. It proves frontend routing, not a real chooser UI or installed backend.
- The Qt row proves live `QStyleHints::colorScheme()` and a probe-owned palette
  derived from the hint. It does not claim Qt automatically replaces an
  application's explicitly applied palette.
- GTK/GSettings, Flatpak sandbox behavior, a host session bus, an installed or
  nested desktop session, hardware, and real non-Settings families remain
  deliberately unqualified.
