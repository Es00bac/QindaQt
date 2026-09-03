# Clipboard Settings route second repair handoff

- Worker: Evelyn Nelson-Codex (`evelyn-nelson-codex`)
- Time: `2026-09-03T09:45:43-06:00`
- Exact base: `753ea20ec6ad556772d63584f4c6840b1c668e12`
- Rejected product ancestor: `6a2d7f45972d620fcccc8e038e01a416ced3a1cd`
- Rejected repair candidate: `633299ada1b57df733c5d7227a6b1ab215044ae9`
- Second repair candidate commit: `6f4f728e40db51e6b8731371d7a0f4b4c3dc6894`
- Second repair candidate tree: `2f1d362e41bd55cce9e0e4b550bd6b7a99c2fdcd`
- Candidate parent: `55ea092c3626c3f6e40a6f0fa2336455f02fcda8`
- Requested next action: independent exact review by Fern Hunt, then manager integration.

## Changed paths in the second repair commit

- `docs/wiki/apps/clipboard-settings.md`
- `docs/wiki/development/testing-harness.md`
- `tests/apps/settings/clipboard/tst_clipboard_settings_service.cpp`

## P2.1 closure

The prior `QTRY_VERIFY(viewSpy.count() > 0)` could wait through the public
Clipboard client's five-second follow-up fetch timeout. The tightened
registered test delivers only the client's queued operation-completion
metacall, verifies that the route is immediately Uncertain, and requires
exactly one `viewChanged`. No timer is processed before that assertion.

The original `633299a` test over exact rejected product `6a2d7f4` passes after
5.010 seconds. The tightened test over exact rejected product `6a2d7f4` fails
in 0 ms with actual notification count 0 versus expected 1. The same tightened
test passes on the repaired product in 31 ms. The owning route and harness
pages now state this transition-bounded negative-control contract.

## Reproduction and negative-control evidence

- Created the rejected scratch source under the assigned build root by
  extracting `git archive 6a2d7f45972d620fcccc8e038e01a416ced3a1cd`, then overlaying the three
  Clipboard test files from `633299ada1b57df733c5d7227a6b1ab215044ae9`.
- Configured the scratch Debug tree with the lane's exact Ninja/cache/strict
  recipe, exit 0.
- Built `qindaqt_clipboard_settings_preference_tests` and
  `qindaqt_clipboard_settings_service_tests` with `--parallel 3`, exit 0,
  61/61 steps.
- Ran
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent .../qindaqt_clipboard_settings_service_tests uncertainClearIsNotReplayed -v2`
  before tightening: exit 0, function passed after 5.010 seconds.
- Overlaid the tightened test on the same exact rejected product, rebuilt the
  service-test target (3/3 steps), and reran the same command: expected exit 1,
  2 QtTest harness functions passed and the named function failed in 0 ms at
  `viewSpy.count()`, actual 0 versus expected 1.
- Built the repaired Debug service-test target (3/3 steps) and ran the same
  isolated function: exit 0, 3/3 QtTest functions passed in 31 ms.

## Debug and Release acceptance evidence

- Debug configure, exit 0:
  `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`.
- Release configure, exit 0: the same exact recipe with build directory
  `release` and `-DCMAKE_BUILD_TYPE=Release`.
- Focused/dependency-adjacent build in both profiles, exit 0:
  `cmake --build <profile> --parallel 3 --target` followed by the Clipboard
  library/QML and three Clipboard tests, Settings application/navigation,
  every Settings1 value/protocol/service/client test target, and the Customize,
  Audio, and Bluetooth model/page/lifecycle test targets named in the prior
  handoff. Both incremental builds completed every requested target.
- Debug complete Settings selector, exit 0, 47/47 passed in 90.05 seconds:
  `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -R '^qindaqt\.settings-' --output-on-failure --no-tests=error`.
- Release complete Settings selector, exit 0, 47/47 passed in 88.46 seconds:
  the same command against the `release` build directory.
- Debug forced-fatal QML selector, exit 0, 8/8 passed in 2.31 seconds:
  `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/debug -R '^qindaqt\.(settings-(customize-page|customize-window-lifecycle|audio-page|bluetooth-page|bluetooth-window-close|clipboard-page|navigation-page)|settings-app-offscreen)$' --output-on-failure --no-tests=error`.
- Release forced-fatal QML selector, exit 0, 8/8 passed in 2.21 seconds: the
  same command against the `release` build directory.

## Static gates

- `./tools/validate-docs`, exit 0: 131 Markdown documents and `mkdocs.yml`
  navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-settings-route/site`, exit 0.
- `./tools/check-source-shape`, exit 0: 2,152 source files checked and none
  skipped; only the repository's existing decomposition-review warnings were
  emitted.
- `git diff --check`, exit 0 before commit; `git show --check --oneline
  6f4f728e40db51e6b8731371d7a0f4b4c3dc6894`, exit 0 after commit.
- `git diff --name-only -- '*.json'`, exit 0 with no output; no JSON changed,
  so `python3 -m json.tool` is not applicable.

## Bounded caveats

- Evidence is injected-fake, absent-bus, warning-fatal offscreen, and
  relocated-package coverage only. It does not claim a host bus, host
  clipboard, live Wayland/data-control capture, hardware/uinput, live AT-SPI,
  nested compositor, or session evidence.
- This second repair changes no production behavior or public interface. It
  makes the registered test distinguish the already-repaired immediate notify
  from rejected behavior and aligns documentation with that exact proof.
- No clipboard content authority, per-entry mutation, persistence,
  configurable retention, private service, or transport authority was added
  to QML.
