# Status-notifier tray S1 rejected-candidate repair handoff

- Implementer: Gloria Hewitt-Codex (`gpt-5.6-sol`, reasoning high)
- Exact rejected candidate: `9d1a30b02729c0aba6deba9a43fa67418d283e76`
- Exact base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Exact product candidate: `4c8e47b28d2d92711bf433c8d2a5afc9be59030f`
- Product tree: `47cd0c50922b468f94e1374a287a8c4def526791`
- Branch/worktree: `worker/tray-s1` at `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1`

## Finding closure ledger

Every reviewer control was first reproduced against `9d1a30b`; all failed as reported except the P1-1 scratch receiver also encoded a non-protocol payload. Registered controls name the relevant finding in an `AGENT-NOTE:`.

| Finding | Closing commit | Registered control and result |
| --- | --- | --- |
| P1-1 missing host retirement signal | `8e44df5d`; protocol-signature correction in `4c8e47b2` | `tst_status_notifier_watcher::emitsHostUnregisteredWhenOwnerDisconnects` passes. KDE's installed `kf6_org.kde.StatusNotifierWatcher.xml` specifies a payload-free `StatusNotifierHostUnregistered`; the reviewer scratch `SLOT(record(QString))` is therefore not a valid receiver. The local QObject signal retains the unique owner for in-process consumers, while the wire signal is payload-free. |
| P1-2 unsigned item coordinates | `f4a6cce7` | `tst_status_notifier_item_client::dispatchesRecordedIntents` requires strict signed `(int,int)` slots and passes. |
| P1-3 two item paths wedge population | `b66abc08` | `tst_status_notifier_monitor::populatesTwoPathsFromOneOwner` passes with one owner generation shared across both paths. |
| P1-4 root object path dropped | `b66abc08` | `tst_status_notifier_monitor::populatesRootObjectPath` passes. |
| P1-5 wrong-typed required facts accepted | `f4a6cce7` | `tst_status_notifier_item_client::rejectsWrongTypedRequiredStrings` passes and fails closed. |
| P1-6 theme lookup escapes injected root | `24a27d66` | `tst_status_notifier_icon::locatorRejectsEscapingThemeDirectories` passes for parent and symlink escapes. |
| P1-7 themed image exceeds 512 px | `24a27d66` | `tst_status_notifier_icon::rendererBoundsThemeAndFallbackImages` passes for themed input. |
| P1-8 placeholder exceeds 512 px | `24a27d66` | The same renderer control passes for fallback output. |
| P2-1 degraded start not idempotent | `8e44df5d` | `tst_status_notifier_watcher::degradedStartIsIdempotent` passes. |
| P3-1 future-adapter comment is stale | `4c8e47b2` | `tst_status_notifier_values::productionProtocolCommentsStayCurrent` passes and guards the production source policy. |

The earlier timeout row's 0.061-second pass on `9d1a30b` was reproduced as an immediate no-owner error rather than timeout evidence. Commit `f4a6cce7` splits this into `reportsImmediateTransportErrors` and `reportsLiveOwnerTimeouts`; the latter holds a live private-bus owner and withholds the reply.

## Rejected-candidate reproduction

The reviewer harness was built against `9d1a30b` and run one selector at a time. Results: `hostUnregisteredSignalIsMissing` exit 1 (0 observations); `signedCoordinateMethodIsNotInvoked` exit 1 (0 signed calls); `twoPathsFromOneOwnerWedgePopulation` exit 1 (count 1, not populated); `rootObjectPathIsDroppedByMonitor` exit 1 (count 0); `wrongTypedSnapshotIsAccepted` exit 1 (integer `Id`/`Title` coerced to strings); `themeIndexCanEscapeInjectedRoot` exit 1 (outside path located); `themedImageSizeIsNotBounded` exit 1 (513x1); `placeholderSizeIsNotBounded` exit 1 (513x513); `degradedStartIsNotIdempotent` exit 1 (true then false). The stale-comment `rg` control found the quoted production comment. The old timeout selector exited 0 with 3/3 QTest functions in 0.061 seconds, confirming it exercised immediate failure.

## Changed paths

- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/status-tray.md`
- `ops/team/messages/shell-system-tray/1788437566-jean-sammet-handoff.md`
- `ops/team/workers/jean-sammet.md`
- `src/shell/status_notifier/CMakeLists.txt`
- `src/shell/status_notifier/icon/CMakeLists.txt`
- `src/shell/status_notifier/icon/include/qindaqt/shell/status_notifier/icon/status_notifier_icon_locator.h`
- `src/shell/status_notifier/icon/include/qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h`
- `src/shell/status_notifier/icon/src/status_notifier_icon_locator.cpp`
- `src/shell/status_notifier/icon/src/status_notifier_icon_renderer.cpp`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_limits.h`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_types.h`
- `src/shell/status_notifier/item_client/CMakeLists.txt`
- `src/shell/status_notifier/item_client/include/qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h`
- `src/shell/status_notifier/item_client/include/qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h`
- `src/shell/status_notifier/item_client/src/status_notifier_item_client.cpp`
- `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp`
- `src/shell/status_notifier/src/status_notifier_registry.cpp`
- `src/shell/status_notifier/watcher/CMakeLists.txt`
- `src/shell/status_notifier/watcher/include/qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h`
- `src/shell/status_notifier/watcher/src/status_notifier_watcher_object.cpp`
- `src/shell/status_notifier/watcher/src/status_notifier_watcher_object_p.h`
- `src/shell/status_notifier/watcher/src/status_notifier_watcher_service.cpp`
- `tests/shell/status_notifier/CMakeLists.txt`
- `tests/shell/status_notifier/status_notifier_fake_item_test_support.h`
- `tests/shell/status_notifier/status_notifier_private_bus_test_support.h`
- `tests/shell/status_notifier/tst_status_notifier_icon.cpp`
- `tests/shell/status_notifier/tst_status_notifier_item_client.cpp`
- `tests/shell/status_notifier/tst_status_notifier_monitor.cpp`
- `tests/shell/status_notifier/tst_status_notifier_values.cpp`
- `tests/shell/status_notifier/tst_status_notifier_watcher.cpp`

The two Jean Sammet coordination paths are inherited unchanged from the branch base above the rejected product commit; Gloria did not edit them.

## Acceptance evidence

Both exact prescribed configure commands completed with exit 0:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/tray-s1/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/tray-s1/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Focused Debug and Release builds of the four status-notifier libraries and seven test executables completed with exit 0. The first Release build invocation yielded before its terminal status was captured, so the exact build was immediately rerun and exited 0; the final watcher/test delta was rebuilt in both profiles with exit 0.

For each profile, discovery used:

```text
ctest --test-dir <profile> -N 2>/dev/null | grep -i -E 'Test +#[0-9]+: .*(status-notifier|tray)'
```

It exited 0 and found exactly seven registered rows (`151` through `157`). Each profile then ran:

```text
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir <profile> -R '^qindaqt\.status-notifier-' -V --output-on-failure --no-tests=error
```

Debug: exit 0, 7/7 CTest rows and 102/102 QTest functions. Release: exit 0, 7/7 rows and 102/102 functions. Per profile: values 18, registry 25, presentation 9, watcher 13, item-client 14, monitor 10, icon 13. A final non-verbose run after the payload-free watcher correction passed 7/7 in Debug and 7/7 in Release (about 4.43 seconds each). There are no tray QML rows; `QT_FATAL_WARNINGS=1` was nevertheless set for the complete selector.

Static gates on the product candidate:

- `./tools/validate-docs`: exit 0; 116 documentation files and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/tray-s1/site`: exit 0.
- `./tools/check-source-shape`: exit 0; 1,778 files checked. It reported only the two pre-existing non-owned warnings in `tests/compositor/CMakeLists.txt` (500) and `tests/services/display_color_model/tst_color_model.cpp` (539). The repaired item-client source is 495 non-blank lines.
- `git diff --check`: exit 0.
- JSON validation: not applicable; no JSON changed.

At 2026-09-03T07:03:44-06:00, process inspection found no process referring to either lane build path and no `dbus-daemon` started after the 06:44 claim. Pre-existing daemons from 04:57–05:38 belonged to other activity and were left untouched. No lane-owned private bus was leaked.

## Bounded caveats and next action

This candidate claims private-bus/injected-fake coverage only. It does not claim host desktop, hardware, uinput, system/session service, or nested-compositor evidence. No network calls were made. The tray implementation does not yet consume the global-menu dbusmenu decoder; no duplicate parser was added in this repair.

Requested next action: independent exact review by Marjorie Lee Browne, then manager integration.
