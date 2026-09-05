# Handoff — Shell iconography I1 (gerty-cori)

- Candidate commit: `54cda1fbea6b4b043eaacef04a1f9d9ed7fed4c8`
- Candidate tree: `8b31c20851a8a3af18861a8683bb77c5701a8001`
- Exact base: `37f8523ed105e66d9784f8cca767d060eed3f1da` (`main` tip at claim)
- Branch / worktree: `worker/shell-icons` at
  `/home/cabewse/work_SPaC3/container-wm-workers/shell-icons`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/shell-icons`

## Changed paths (sorted)

    docs/wiki/adr/0071-shell-iconography-confined-xdg-icon-themes.md
    docs/wiki/adr/index.md
    docs/wiki/architecture/module-boundaries.md
    docs/wiki/development/testing-harness.md
    docs/wiki/shell/iconography.md
    mkdocs.yml
    src/CMakeLists.txt
    src/shell/icons/CMakeLists.txt
    src/shell/icons/include/qindaqt/shell/icons/desktop_entry_icon_resolver.h
    src/shell/icons/include/qindaqt/shell/icons/icon_image_provider.h
    src/shell/icons/include/qindaqt/shell/icons/icon_lookup.h
    src/shell/icons/include/qindaqt/shell/icons/icon_runtime.h
    src/shell/icons/include/qindaqt/shell/icons/icon_theme_limits.h
    src/shell/icons/include/qindaqt/shell/icons/icon_theme_locator.h
    src/shell/icons/qml/Icon.qml
    src/shell/icons/src/desktop_entry_icon_resolver.cpp
    src/shell/icons/src/icon_image_provider.cpp
    src/shell/icons/src/icon_lookup.cpp
    src/shell/icons/src/icon_runtime.cpp
    src/shell/icons/src/icon_theme_locator.cpp
    tests/CMakeLists.txt
    tests/shell/icons/CMakeLists.txt
    tests/shell/icons/shell_icons_test_fixtures.h
    tests/shell/icons/tst_shell_icons_locator.cpp
    tests/shell/icons/tst_shell_icons_provider.cpp
    tests/shell/icons/tst_shell_icons_qml.cpp
    tests/shell/icons/tst_shell_icons_resolver.cpp

Coordination files (`ops/team/messages/shell-iconography/*`,
`ops/team/workers/gerty-cori.md`) follow in a second commit on the same
branch.

## What landed

New module `src/shell/icons` (static `QindaQt::ShellIcons`, compiled
`QindaQt.Shell.Icons 1.0`): `IconThemeLocator` (XDG icon-theme lookup over
injected ordered roots with Inherits cycle guard/depth cap, hicolor last,
closest-size rule with scale awareness, `-symbolic` preference, canonical
containment, bounded indexes/caches), `DesktopEntryIconResolver` (app id /
desktop-entry id → `Icon=` name over injected application roots, reusing the
launcher's public pure `DesktopEntryParser`), `IconImageProvider`
(`image://qindaqt-icon/<name>?size=&scale=&color=&symbolic=1`, QSvgRenderer
rendering, alpha-preserving symbolic recolor, bounded LRU, deterministic
neutral placeholder, never null/never warning), the `IconRuntime::install`
composition seam with freedesktop root helpers (never `~/.icons`), and the
QML `Icon` element (token-only typed fallback glyph, Accessible naming).
ADR-0071 plus the iconography wiki page, module-boundaries row,
testing-harness selector row, and mkdocs nav are in the same commit.

## Evidence (all commands actually run by me)

Configure (both profiles, exit 0 each):

    cmake -S . -B <ROOT>/debug -G Ninja -C .../qindaqt-system-kwin-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
      -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
    (same for <ROOT>/release with -DCMAKE_BUILD_TYPE=Release)

Build (exit 0, strict warnings-as-errors; no warning output):

    cmake --build <ROOT>/{debug,release} --parallel 3 --target \
      qindaqt_shell_icons qindaqt_shell_icons_locator_tests \
      qindaqt_shell_icons_resolver_tests qindaqt_shell_icons_provider_tests \
      qindaqt_shell_icons_qml_tests

Tests (each run under `env -u DBUS_SESSION_BUS_ADDRESS
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`; GUI rows additionally force
`QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`,
`QT_FATAL_WARNINGS=1` with `DISPLAY`/`WAYLAND_DISPLAY` emptied via CTest
`ENVIRONMENT`):

    ctest --test-dir <ROOT>/debug   -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error
      → 4/4 passed (locator 29 subtests, resolver 9, provider 14, qml 1), exit 0
    ctest --test-dir <ROOT>/release -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error
      → 4/4 passed, exit 0

Static gates (exit 0 each):

    ./tools/validate-docs                      → validated 145 documents + mkdocs nav
    .../qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site
    ./tools/check-source-shape                 → exit 0 (pre-existing warnings only on other lanes' files)
    git diff --check                           → exit 0

No JSON files were changed, so `python3 -m json.tool` had nothing to check.
The fixture trees are generated at test time beneath the build root; nothing
writes under /tmp or contacts a host bus, display, compositor, network, or
hardware.

## Negative controls

Every hostile row fails on a tree without the rule it proves: `../` and
symlink-escape fixture files exist and resolve only if confinement is
removed; the oversized index parses only if the byte ceiling is removed;
hostile names pass the grammar only if validation is removed; the symbolic
recolor pixel assertion holds source pixels if recoloring is removed; the
inheritance-depth row resolves chain19 if the depth cap is removed. During
development the locator initially missed the theme-directory component and
25 themed-lookup assertions failed, demonstrating the rows are live.

## Caveats (deliberate non-claims)

- No applet, `src/shell/runtime/**`, or `src/shell/qml/**` is touched; wiring
  (`IconRuntime::install` from the shell composition root, theme JSON
  `iconTheme` key, applet/task-list consumption) belongs to the later Codex
  lane.
- No installed-package/component staging for the new module; packaging is a
  wiring-lane concern.
- No nested-session, host-bus, hardware, or live-theme (breeze) claims;
  breeze is only the documented production default name.
- Icon names use the documented `[A-Za-z0-9._-]` grammar; theme names with
  other characters are refused (fail-closed choice, documented in the wiki
  page).

## Requested next action

Independent exact review of `54cda1fbea6b4b043eaacef04a1f9d9ed7fed4c8`,
then manager integration.
