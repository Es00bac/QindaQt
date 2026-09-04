# Kathrin Bringmann — independent platform recheck

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate: `e473bbf74e9954ce064763a3daf49fcaa9eee552`
- Tree: `17cbd7946ff72ecf0f9b8842f9266e78f686c298`
- Parent: `6720a4faf325d0a66826e72813c4e8d7239b3857`
- Exact base: `6720a4faf325d0a66826e72813c4e8d7239b3857`
- Main merge-base observed during review: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Repaired rejected candidate: `7025a1cab90419baf07431e5880bd40ebee2afac`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-pairing-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex`

## Findings ledger

### P0

None.

### P1

None.

### P2

None. The prior P2 is repaired and its exact negative control remains discriminating.

### P3

None.

## Review disposition

The repair meets the Bluetooth and Settings Center contracts. In
`src/apps/settings_center/Main.qml:141`–`156`, the host Escape shortcut now
yields only when all three conditions hold: the active route component is
Bluetooth, the route model exists and reports an active prompt, and the prompt
reply lane is free. In that state the existing window-context shortcut in
`src/apps/settings/bluetooth/qml/BluetoothPairingSection.qml:26`–`39` is the
sole enabled match and sends the route-owned rejection. When there is no prompt,
when the reply lane is busy, or on any other route, the host shortcut remains
enabled and returns focus to the active wide or compact route tab.

The added row at
`tests/apps/settings/bluetooth/tst_bluetooth_window_close.cpp:221`–`305` is not
vacuous. It loads the real `Main.qml`, holds focus on the Bluetooth Close button
outside the pairing section, makes a confirmation prompt visible, sends Escape,
requires exactly one reply with `false`, waits 50 ms and requires the count to
remain one, then makes the reply lane busy and requires Escape to return focus
to the Bluetooth route tab without a second reply. The same row compiled against
an immutable extracted `7025a1c` Settings host fails with `promptReplies` 0 at
line 291, while the exact candidate passes in Debug and Release.

The existing no-prompt controls remain active: the Settings navigation-page row
exercises ordinary wide Bluetooth Escape return and PageTab accessibility, and
the Bluetooth window-close row exercises compact no-prompt Escape/Tab focus.
The full window-close binary passes 9/9 in each configuration, including all
discovery-release close outcomes. The Bluetooth page, applet offscreen/surface,
Settings navigation, accessibility, keyboard, boundary, installed-package, and
adjacent Settings rows all pass. No new process, transport, service, persistence,
threading, owner/epoch/revision/generation, or platform authority was introduced.

## Commands and results

### Identity and cleanliness

Before review and after all builds/tests/gates:

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
```

Results: candidate, tree, and parent matched the header; both status checks
produced no output (exit 0). `git merge-base HEAD main` returned
`196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`.

### Configure and focused builds

Debug configure:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release used the identical command with `/release` and
`-DCMAKE_BUILD_TYPE=Release`. Both exited 0.

The following command ran in both build trees (substituting `debug` and
`release`), exited 0, and ended at Ninja `[195/195]` after dependency/coalescing
updates in the reused roots:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug \
  --parallel 3 --target \
  qindaqt-settings qindaqt-bluetooth-service \
  tests/settings/all \
  tests/services/settings_protocol/all \
  tests/services/settings_service/all \
  tests/services/settings_client/all \
  tests/services/bluetooth_protocol/all \
  tests/services/bluetooth_model/all \
  tests/services/bluetooth_client/all \
  tests/services/bluetooth_bluez_adapter/all \
  tests/services/bluetooth_service/all \
  tests/apps/settings/network/all \
  tests/apps/settings/customize/all \
  tests/apps/settings/audio/all \
  tests/apps/settings/bluetooth/all \
  tests/apps/settings/power/all \
  tests/apps/settings/clipboard/all \
  tests/apps/settings_center/all \
  tests/shell/bluetooth_applet/all
```

### Exact Escape reproductions and controls

The prior product reproducer was rebuilt against the candidate by replaying the
current `ninja -t commands qindaqt_bluetooth_window_close_tests` compile/link
lines and substituting only its scratch source/object/output under
`<ROOT>/repros/settings-escape-product`. Compile and link each exited 0. Run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/settings-escape-product/settings_escape_product_repro
```

Observed: `promptReplies= 1 lastBoolean= false`. Exit was 7, which is the
legacy defect reproducer's expected repaired outcome because its source returns
0 only when the old defect (`promptReplies == 0`) is reproduced.

The independent Qt ambiguity control:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  /usr/lib64/qt6/bin/qmltestrunner \
  -input /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/settings-escape \
  -import /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug/qml \
  -o -,txt
```

Exit 0; 3/3 passed; observed
`hostActivations=0 promptActivations=0` for two enabled identical window
shortcuts.

The new focused candidate row ran in both Debug and Release:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  <ROOT>/<config>/tests/apps/settings/bluetooth/qindaqt_bluetooth_window_close_tests \
  promptEscapeInRealHostSendsSingleRejection -v1
```

Each exited 0 with 3/3 QtTest functions passed.

For the negative control, `git archive 7025a1c -- src/apps/settings_center`
was extracted only under
`<ROOT>/repros/negative-7025/source`. The exact candidate test source and
candidate-built libraries/QML plugins were compiled and linked with only
`QINDAQT_SETTINGS_SOURCE_DIR` redirected to that immutable extracted host. Run:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/negative-7025/qindaqt_bluetooth_window_close_tests_7025_product \
  promptEscapeInRealHostSendsSingleRejection -v1
```

Expected negative-control exit 1: 2 passed, 1 failed. Observed failure:
`Actual (bluetooth.promptReplies): 0`, `Expected (1): 1`, at candidate test
line 291. The extracted `Main.qml` SHA-256 was
`4b1c9b4e4d86501631cbe89d6cc36121bc08603a7952f24230e4b7ca6c29887f`,
identical to `git show 7025a1c:src/apps/settings_center/Main.qml` and distinct
from the candidate file.

The full warning-fatal window-close binary ran with the same isolated
environment in Debug and Release:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  <ROOT>/<config>/tests/apps/settings/bluetooth/qindaqt_bluetooth_window_close_tests -v1
```

Debug exited 0 with 9/9 passed; Release exited 0 with 9/9 passed.

### Debug and Release selectors

All selector commands unset host display variables and pin both buses to an
unreachable path:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug \
  -R 'bluetooth' --output-on-failure --no-tests=error
```

Debug: exit 0, 31/31 passed, 16.59 s. Release used `/release`: exit 0,
31/31 passed, 13.20 s.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug \
  -R '^qindaqt\.settings-' --output-on-failure --no-tests=error
```

Debug: exit 0, 54/54 passed, 105.28 s. Release used `/release`: exit 0,
54/54 passed, 103.70 s.

### Static, documentation, and JSON gates

- `./tools/validate-docs`: exit 0; 140 Markdown documents and navigation
  validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/site`:
  exit 0; documentation built successfully.
- `./tools/check-source-shape`: exit 0; 2,421 source files checked, 0 skipped.
  It emitted only existing decomposition-review threshold warnings, including
  `src/apps/settings_center/Main.qml` at 309 non-blank lines; no hard failure.
- `git diff --check`, `git diff --check HEAD^ HEAD`, and
  `git diff --check 196e69d..HEAD`: each exit 0 with no output.
- `git diff --name-only HEAD^ HEAD -- '*.json'`: no output. No JSON changed, so
  `python3 -m json.tool` was not applicable.

No nested compositor/session test, ambient host D-Bus, display server, hardware,
radio, uinput, network, or other forbidden surface was used.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
