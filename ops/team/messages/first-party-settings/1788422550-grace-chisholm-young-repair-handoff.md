# Bluetooth Settings close-liveness repair handoff

- Timestamp: 2026-09-03T02:02:30-06:00
- Worker: Grace Chisholm Young (`grace-chisholm-young`)
- Exact repaired candidate commit: `24129a26e5d7c6bb01e1dd9e287c75a8db1c224d`
- Candidate tree: `4e66338ba028793132bff93e3781e5cd430035ee`
- Rejected product ancestor: `bf7b00fec5a80f3d37795568d4dde6c35a72ea19`
- Repair commit parent: `d7a53f8ed09d75956586d29fdbb264e36c1da3d4`
- Exact original lane base: `ee187e97221ee7f13d6e4e00e6ee3356b6faf3d8`
- Branch: `worker/bluetooth-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-settings-route`

## Outcome

P1-1 is repaired without changing the held-lease path. `departureReleasePending()` now remains true only for an outstanding admitted acquire/release or an actual held lease that can still be released. Rejected, failed, uncertain, inexact, owner-lost, and owner-replaced pending acquisitions establish no lease and clear the orphaned departure request, allowing the real Settings `Main.qml` close resolver to finish.

P3-1 is closed by replacing the overclaim with exact stub-versus-real-model proof. P3-2 is closed by route-level hostile snapshot cases for duplicate identifiers, overlong names, invalid RSSI, and invalid class. P3-3 is closed by compact Settings host PageTab selected-state, Escape-return, Tab-entry, and always-enabled Close coverage.

## Sorted repair paths

- `docs/wiki/apps/bluetooth-settings.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp`
- `src/apps/settings/bluetooth/include/qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h`
- `tests/apps/settings/bluetooth/CMakeLists.txt`
- `tests/apps/settings/bluetooth/tst_bluetooth_settings_adversarial.cpp`
- `tests/apps/settings/bluetooth/tst_bluetooth_window_close.cpp`

## Verification evidence

`<ROOT>` is `/home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route`. Every command ran from the assigned worktree. Build and CTest commands used `--parallel 3` only for builds; no nested/session row was run.

- Exact Debug configure command from the lane recipe, with `-B <ROOT>/debug -DCMAKE_BUILD_TYPE=Debug`: exit 0.
- Exact Release configure command from the lane recipe, with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release`: exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_settings_bluetooth qindaqt_settings_bluetooth_qml qindaqt_bluetooth_settings_model_tests qindaqt_bluetooth_settings_adversarial_tests qindaqt_bluetooth_page_tests qindaqt_bluetooth_window_close_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test`: exit 0, final rebuild 33/33 actions.
- Same focused target command for `<ROOT>/release`: exit 0, final rebuild 33/33 actions.
- `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.settings-bluetooth-' --output-on-failure --no-tests=error`: exit 0, 7/7 passed.
- Same Bluetooth selector for `<ROOT>/release`: exit 0, 7/7 passed.
- `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' --output-on-failure --no-tests=error`: exit 0, 9/9 passed.
- Same Settings Center selector for `<ROOT>/release`: exit 0, 9/9 passed.
- `./tools/validate-docs`: exit 0, 126 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,995 source files checked; only pre-existing decomposition warnings outside this lane were reported.
- `git diff --check`: exit 0.
- JSON validation: not applicable; this repair changes no JSON.

## Bounded caveats

This candidate deliberately claims only injected-fake, offscreen/software-renderer, absent-private-bus, and relocated-package evidence. It does not claim a host session/system bus, BlueZ or radio hardware, pairing/trust/removal, live AT-SPI or screen-reader traversal, a nested compositor, physical input, or physical Bluetooth qualification. No host D-Bus service, hardware, uinput, network call, or `tests/session` row was used.

## Requested next action

Dorothy Denning (Z.AI GLM 5.3) should independently recheck the exact candidate `24129a26e5d7c6bb01e1dd9e287c75a8db1c224d`, including her two original probes and the uncertain/owner-loss variants. If accepted, the Program Manager should integrate that exact product commit.
