# Status-notifier tray S1 second repair handoff

- Implementer: Gloria Hewitt-Codex (OpenAI Codex `gpt-5.6-sol`, reasoning high)
- Exact rejected product candidate: `4c8e47b28d2d92711bf433c8d2a5afc9be59030f`
- Exact second-round starting HEAD: `a360af742d8a3781c7f809a29a00b4a24ba33084`
- Exact original product base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Exact product candidate: `0e5fed95535a578c269b86cbfbe7f291f698819b`
- Product tree: `5adaf207e77f88b77b2e1c40726a72edb9416925`
- Branch/worktree: `worker/tray-s1` at `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1`

## Finding closure

### P1-1 — one live-owner generation per watcher epoch

The monitor now keeps a dedicated `uniqueName -> generation` ledger whose lifetime is the watcher epoch, independent of per-path client slots. An item-unregistered signal removes only that path; exact owner loss removes the ledger entry, and epoch reset clears the whole ledger. The registered private-bus control `StatusNotifierMonitorTests::preservesOwnerGenerationAfterLastPathRetires` advertises `/One`, retires it without owner or watcher loss, advertises `/Two`, and requires the owner generation and watcher epoch to remain unchanged.

The control was rebuilt against the rejected slot-derived generation lookup and exited 1 with `replacement.generation` 2 versus expected 1. After restoring the repair it passed 3/3 selected QTest functions.

### P3-1 — precise decoder wrong-type policy

Production source, public header, and owning wiki now agree: presentation-bearing recognized fields fail the descriptor closed on an unexpected wire type; unknown properties are ignored; the recorded-only `WindowId`, `OverlayIconName`, `ItemIsMenu`, and `Menu` facts are safe-dropped on a wrong type because they cannot reach the registry or renderer. The registered `productionProtocolCommentsStayCurrent` source-policy control checks all three surfaces.

With the rejected prose temporarily restored, that control exited 1 at the forbidden `top-level type does not match the wire` statement. With the repaired prose it passed 3/3 selected QTest functions.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/status-tray.md`
- `ops/team/messages/shell-system-tray/1788441672-gloria-hewitt-codex-claim.md` (coordination only)
- `ops/team/messages/shell-system-tray/1788442154-gloria-hewitt-codex-handoff.md` (coordination only)
- `ops/team/workers/gloria-hewitt-codex.md` (coordination only)
- `src/shell/status_notifier/item_client/include/qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h`
- `src/shell/status_notifier/item_client/include/qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h`
- `src/shell/status_notifier/item_client/src/status_notifier_item_client.cpp`
- `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp`
- `tests/shell/status_notifier/tst_status_notifier_monitor.cpp`
- `tests/shell/status_notifier/tst_status_notifier_values.cpp`

## Exact verification evidence

Both prescribed configurations exited 0:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/tray-s1/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/tray-s1/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

The following focused build exited 0 in Debug and Release:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target qindaqt_status_notifier qindaqt_status_notifier_watcher qindaqt_status_notifier_item_client qindaqt_status_notifier_icon qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests qindaqt_status_notifier_values_tests
```

Discovery ran under the required bus environment and exited 0 in each profile, listing exactly the seven rows `qindaqt.status-notifier-{values,registry,presentation,watcher,item-client,monitor,icon}`:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent sh -c "ctest --test-dir <ROOT>/<profile> -N 2>/dev/null | grep -i -E '^  Test +#[0-9]+: .*(status-notifier|tray)'"
```

The complete selector exited 0 in Debug and Release with 7/7 CTest rows passed and no skip or failure:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' --output-on-failure --no-tests=error
```

Direct per-binary `-silent` runs under the same environment passed 103/103 QTest functions in each profile: values 18, registry 25, presentation 9, watcher 13, item-client 14, monitor 11, icon 13. There are no tray QML rows; `QT_FATAL_WARNINGS=1` was set for all rows nevertheless.

Static gates:

- `./tools/validate-docs` — exit 0; 116 Markdown documents and navigation validated (rerun after the final prose polish, also exit 0).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/tray-s1/site` — exit 0 (rerun after final prose polish, also exit 0).
- `./tools/check-source-shape` — exit 0; 1,778 files checked. It reported only the two unrelated pre-existing decomposition warnings in `tests/compositor/CMakeLists.txt` (500 nonblank lines) and `tests/services/display_color_model/tst_color_model.cpp` (539).
- `git diff --check` — exit 0 before candidate commit and after the final prose polish.
- JSON validation — not applicable; no JSON changed.

At `2026-09-03T07:29:14-06:00`, process inspection found no private `dbus-daemon` started after this lane's `2026-09-03T07:20:34-06:00` process start. All visible daemons predated the lane and were left untouched. The icon test's relative temporary fixtures were moved out of the worktree into the assigned ignored build root; the product tree remained clean except for the intentional coordination update.

## Bounded caveats and requested next action

This candidate claims source/unit and injected private-session-bus evidence only. It does not claim host session/system bus behavior, a rendered panel tray, dbusmenu rendering, assistive-technology bridge behavior, nested compositor behavior, hardware, uinput, or network evidence.

Requested next action: independent exact review by Marjorie Lee Browne, then manager integration.
