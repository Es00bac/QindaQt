# Clipboard Settings route repair handoff

- Worker: Evelyn Nelson-Codex (`evelyn-nelson-codex`)
- Time: `2026-09-03T08:21:13-06:00`
- Exact base: `753ea20ec6ad556772d63584f4c6840b1c668e12`
- Rejected candidate: `6a2d7f45972d620fcccc8e038e01a416ced3a1cd`
- Repair candidate commit: `633299ada1b57df733c5d7227a6b1ab215044ae9`
- Repair candidate tree: `867febe29f9eca30df7a35a484a6dcda8c667cf8`
- Requested next action: independent exact review by Fern Hunt, then manager integration.

## Changed paths in the repair commit

- `docs/wiki/apps/clipboard-settings.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/settings/clipboard/clipboard_settings_model.cpp`
- `src/apps/settings/clipboard/clipboard_settings_service.cpp`
- `tests/apps/settings/bluetooth/CMakeLists.txt`
- `tests/apps/settings/clipboard/clipboard_settings_test_support.h`
- `tests/apps/settings/clipboard/tst_clipboard_settings_preference.cpp`
- `tests/apps/settings/clipboard/tst_clipboard_settings_service.cpp`
- `tests/apps/settings/customize/CMakeLists.txt`

## Finding closure ledger

- **P1.1 — source-layer consent/direct opt-in:** closed by `633299ada1b57df733c5d7227a6b1ab215044ae9`. `ClipboardSettingsModel` now presents enabled only for Boolean `true` sourced from `user-overrides`; inherited `true` is off and remains directly editable into an explicit `set true` commit. Registered row `qindaqt.settings-clipboard-preference`, test `inheritedTrueRequiresExplicitUserOptIn`, carries the `AGENT-NOTE: Regression for Fern Hunt P1.1` marker.
- **P1.2 — malformed later value still mutable:** closed by `633299ada1b57df733c5d7227a6b1ab215044ae9`. Edit/apply admission now requires route state Ready or Conflict, so the existing baseline cannot keep an Unavailable route mutable. Registered row `qindaqt.settings-clipboard-preference`, test `malformedRefreshFailsClosed`, carries the `AGENT-NOTE: Regression for Fern Hunt P1.2` marker.
- **P1.3 — uncertain clear misses QML notify:** closed by `633299ada1b57df733c5d7227a6b1ab215044ae9`. `retireClearAsUncertain` now emits the shared property notification. Registered row `qindaqt.settings-clipboard-service`, test `uncertainClearIsNotReplayed`, installs a `QSignalSpy` and carries the `AGENT-NOTE: Regression for Fern Hunt P1.3` marker.
- **P1.4 — unconditional Clipboard import breaks lifecycle hosts:** closed by `633299ada1b57df733c5d7227a6b1ab215044ae9`. Both in-process `Main.qml` test targets now link the static Clipboard QML module/plugin and depend on its plugin, mirroring the static-route rule. Existing registered rows `qindaqt.settings-customize-window-lifecycle` and `qindaqt.settings-bluetooth-window-close` carry adjacent CMake `AGENT-NOTE: Regression for Fern Hunt P1.4` markers and pass their behavioral assertions.

## Reproduction before repair

All commands ran with host display variables removed, the inherited session-bus address removed, and the system-bus endpoint set to a nonexistent socket.

- Fern's standalone `repro consent`, exit 1: `FAIL consent-source: effective profile true is displayed as enabled=1, direct explicit opt-in admitted=0, commits=0`.
- Fern's standalone `repro malformed`, exit 1: `FAIL malformed-setting: unavailable=1, canEdit=1, canApply=1`.
- Fern's standalone `repro notify`, exit 1: `FAIL uncertain-notify: clearUncertain=1, viewChanged emissions=0`.
- Exact existing lifecycle selector against Fern's review build, exit 8, 0/2 passed: both roots failed at `Main.qml:13` because `qindaqt_settings_clipboard_qmlplugin` was not found.

Fern's saved P1.1 `repro.cpp` is valid for reproducing the rejected tree but its success condition is inverted for a repaired tree: it increments failures when `admitted` is true or `commitCount` is nonzero, while the finding text requires both. The registered `inheritedTrueRequiresExplicitUserOptIn` regression asserts the stated contract: displayed consent false, direct opt-in admitted, and one explicit Boolean-true commit.

## Acceptance evidence

- Debug configure, exit 0:
  `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
- Release configure, exit 0: the same exact recipe with build directory `release` and `-DCMAKE_BUILD_TYPE=Release`.
- Debug focused/dependency-adjacent build, exit 0, 152/152 executed steps completed: Clipboard library/QML and three Clipboard test executables, Settings application/navigation tests, all Settings1 value/protocol/service/client rows, Customize model/page/lifecycle, Audio model/adversarial/page, and Bluetooth model/adversarial/page/lifecycle targets under `--parallel 3`.
- Release build of the same exact target list, exit 0, 152/152 executed steps completed.
- Debug direct repair selector, exit 0, 4/4 passed:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -R '^qindaqt\.(settings-clipboard-(preference|service)|settings-customize-window-lifecycle|settings-bluetooth-window-close)$' --output-on-failure --no-tests=error`
- Release direct repair selector, exit 0, 4/4 passed: the same command against the `release` build directory.
- Debug complete Settings selector, exit 0, 47/47 passed:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -R '^qindaqt\.settings-' --output-on-failure --no-tests=error`
- Release complete Settings selector, exit 0, 47/47 passed: the same command against the `release` build directory. Registered offscreen QML rows retain `QT_FATAL_WARNINGS=1`; child-process absent-bus construction rows intentionally retain their documented nonfatal warning policy.
- `./tools/validate-docs`, exit 0: 131 Markdown documents and `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/site`, exit 0.
- `./tools/check-source-shape`, exit 0: 2,152 files checked, none skipped. It emitted only decomposition-review warnings for the existing 312-line Clipboard page, 279-line shared Main root, 600-line shared navigation test, 500-line compositor CMake registry, 539-line Display Color test, and 563-line Audio applet test. The lane-prohibited base `tests/session/DesktopSessionTests.cmake` is listed at 499 non-blank lines and was not changed.
- `git diff --check`, exit 0.
- JSON validation is not applicable; the repair changes no JSON.

## Bounded caveats

- Evidence is injected-fake, absent-bus, warning-fatal offscreen, and relocated-package coverage only. It does not claim a host bus, host clipboard, live Wayland/data-control capture, hardware/uinput, live AT-SPI/screen-reader traversal, nested compositor, or session evidence.
- No clipboard descriptor/content, per-entry mutation, persistence, configurable retention, private service, or transport authority was added to QML.
- The repair does not import the later Power route. Its two additive lifecycle link entries follow the same static-module composition rule and should be preserved when the manager combines Clipboard with Power.
