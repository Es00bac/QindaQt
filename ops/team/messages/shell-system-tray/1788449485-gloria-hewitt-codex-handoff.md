# Status-notifier tray S1 third repair handoff

- Implementer: Gloria Hewitt-Codex (OpenAI Codex `gpt-5.6-sol`, reasoning high)
- Exact rejected candidate: `0e5fed95535a578c269b86cbfbe7f291f698819b`
- Exact starting HEAD: `a8803649d5dd38ea3dcb14c454fd07905fd744e4`
- Exact original product base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Exact product candidate: `68009bb47fccb7c6e674c2cee86ba874decd97f6`
- Product tree: `0c7f22090313948ab517e0021b0372e29e4927bf`
- Product candidate parent: `a8803649d5dd38ea3dcb14c454fd07905fd744e4`
- Branch/worktree: `worker/tray-s1` at `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1`

## Finding closure

### P1-1 — unrelated connection can forge owner loss

Closed by product commit `68009bb47fccb7c6e674c2cee86ba874decd97f6`.
Both production `NameOwnerChanged` subscriptions now bind their service match
to `org.freedesktop.DBus`, so another bus peer cannot satisfy the signal rule
by copying only the object path, interface, member, and payload. Both handlers
also require the exact unique-name loss tuple `name == oldOwner` with an empty
`newOwner` before retiring any owner state.

The registered private-bus control
`StatusNotifierMonitorTests::rejectsPeerForgedOwnerLoss` names the finding in
its `AGENT-NOTE:`. It keeps a legitimate item connection live, sends the forged
loss from an unrelated fourth connection, then requires the registry owner,
registry item, and watcher registration all to survive. Because the test
composes the real watcher and real monitor, leaving either production
subscription unfiltered fails it.

The reviewer's exact `secondConnectionCannotForgeOwnerLoss` control first
reproduced the rejected candidate with exit 1 and observed `target live=false`,
registry count `0`, watcher registrations `0`. Rebuilt against this candidate,
the same control exits 0 with `target live=true`, registry count `1`, watcher
registrations `1`.

The registered test source was also mechanically overlaid onto an immutable
`git archive` of `0e5fed9`, configured and built under the assigned build root.
`rejectsPeerForgedOwnerLoss -v1` exited 1 at the live-owner assertion with 2
passed and 1 failed. On this candidate it passes, while existing genuine
disconnect controls continue to pass in both the watcher and monitor rows.

## Sorted product paths changed by the closing commit

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/status-tray.md`
- `src/shell/status_notifier/item_client/include/qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h`
- `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp`
- `src/shell/status_notifier/watcher/include/qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h`
- `src/shell/status_notifier/watcher/src/status_notifier_watcher_service.cpp`
- `tests/shell/status_notifier/tst_status_notifier_monitor.cpp`

Coordination-only paths are
`ops/team/messages/shell-system-tray/1788449101-gloria-hewitt-codex-claim.md`,
this handoff, and `ops/team/workers/gloria-hewitt-codex.md`; they are committed
after the immutable product candidate.

## Exact verification evidence

The reviewer reproduction was run first against `0e5fed9` exactly as specified:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros/build --parallel 3
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros/build/tray_candidate_repros secondConnectionCannotForgeOwnerLoss -v1
```

Build exit 0; expected reproducer exit 1 with 2 passed, 1 failed. The same
reviewer source was then configured against this worktree and Debug build under
`<ROOT>/reviewer-repro-repair`; configure/build exited 0 and the control exited
0 with 3 passed, 0 failed, 0 skipped.

Both prescribed configuration commands exited 0:

```sh
cmake -S . -B <ROOT>/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B <ROOT>/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

The following focused build exited 0 in Debug and Release:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target qindaqt_status_notifier qindaqt_status_notifier_watcher qindaqt_status_notifier_item_client qindaqt_status_notifier_icon qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests qindaqt_status_notifier_values_tests
```

The literal requested discovery pipeline exited 0 in each profile but, because
the build root contains `tray`, also matched unrelated CTest missing-executable
diagnostics. The test-name-only refinement exited 0 and listed exactly rows
151–157: values, registry, presentation, watcher, item-client, monitor, icon.

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent sh -c "ctest --test-dir <ROOT>/<profile> -N 2>/dev/null | grep -i -E '^  Test +#[0-9]+: .*(status-notifier|tray)'"
```

The complete selector exited 0 in Debug and Release with 7/7 rows passed, 0
failed and 0 skipped:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' --output-on-failure --no-tests=error
```

Direct `-silent` runs under the same environment passed 104/104 QTest functions
in each profile: values 18, registry 25, presentation 9, watcher 13,
item-client 14, monitor 12, icon 13. There are no tray QML rows;
`QT_FATAL_WARNINGS=1` was nevertheless present for the full selector.

Static gates all exited 0:

- `./tools/validate-docs` — 116 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`.
- `./tools/check-source-shape` — 1,778 source files checked; only the two unrelated pre-existing decomposition warnings at `tests/compositor/CMakeLists.txt` (500 nonblank lines) and `tests/services/display_color_model/tst_color_model.cpp` (539).
- `git diff --check` and the cached product equivalent.
- JSON validation was not applicable because no JSON changed.

## Bounded caveats and requested next action

This candidate claims source/unit and injected private-session-bus evidence
only. It does not claim host session/system bus behavior, a rendered panel tray,
DBusMenu rendering, assistive-technology bridge behavior, nested compositor
behavior, hardware, uinput, or network evidence.

Requested next action: independent exact review by Marjorie Lee Browne, then
manager integration.
