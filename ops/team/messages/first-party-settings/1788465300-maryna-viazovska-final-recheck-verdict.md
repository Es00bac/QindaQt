# Maryna Viazovska — independent Color Settings route recheck

- Persona: Maryna Viazovska, independent first-party reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `252b2fd7d6d590178b33135af9d64123b1acf6cf`
- Tree SHA: `fd7b8ecddc8f7f06c4f19e3aee19fcc77518429f`
- Parent SHA: `68d295686c3d711738da6b660bea79dd211ef5d6`
- Base SHA: `b971b43881fcef18980acec03c4e43e56ef9db2a`
- Rejected ancestor: `944673bf45bf3d9f142e94940ad11aec9491d115`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/color-settings-route-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex`

## Verdict summary

REJECT. The two requested repairs themselves are present and exercised: fresh-user provisioning/import works and the registered stage-closure row is non-vacuous. However, the mandatory Settings selector cannot pass with the session bus actually unreachable. Three warning-fatal real-host QML rows abort on the Color composition's expected disconnected-service diagnostic. The implementer's 55/55 environment merely unsets `DBUS_SESSION_BUS_ADDRESS`; in this review process it inherited `DISPLAY=:1`, so it did not establish the documented no-host-bus condition.

## Findings ledger

### P0

None.

### P1

1. **The safe Settings selector fails when the session bus is provably unreachable.**

   Contract and location:

   - `docs/wiki/development/testing-harness.md:1183-1191` prescribes the Color selector, and lines 1203-1209 state that the Color/navigation pages run with `QT_FATAL_WARNINGS=1`; line 1219 claims no selector contacts a host bus.
   - `tests/apps/settings_center/CMakeLists.txt:104-112` deliberately makes warnings from `Main.qml` or any route fatal. The same policy is set for the affected Customize and Bluetooth lifecycle rows at `tests/apps/settings/customize/CMakeLists.txt:109-117` and `tests/apps/settings/bluetooth/CMakeLists.txt:118-124`.
   - `src/apps/settings/color/color_route_composition.cpp:84-89` constructs the real Color Settings client from `QDBusConnection::sessionBus()` and emits `qWarning` when that bus is disconnected. `src/apps/settings_center/Main.qml:16,30` imports the backend and eagerly evaluates `ColorRouteComposition.model`, so unrelated real-host page fixtures trigger that warning.

   Exact full-selector reproduction, run once with `P=debug` and once with `P=release`:

   ```sh
   env -u DISPLAY -u WAYLAND_DISPLAY \
     DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
     DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
     HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-$P/home \
     XDG_CONFIG_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-$P/config \
     XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-$P/data \
     XDG_CACHE_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-$P/cache \
     XDG_RUNTIME_DIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-$P/runtime \
     ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/$P \
       -R '^qindaqt\.settings-' --output-on-failure --no-tests=error
   ```

   Observed in both Debug and Release: exit 8, 52/55 passed. These three rows abort with SIGABRT:

   - `qindaqt.settings-customize-window-lifecycle`
   - `qindaqt.settings-bluetooth-window-close`
   - `qindaqt.settings-navigation-page`

   Compact direct reproduction:

   ```sh
   ulimit -c 0
   env -u DISPLAY -u WAYLAND_DISPLAY \
     DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
     DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
     HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/home \
     XDG_CONFIG_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/config \
     XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/data \
     XDG_CACHE_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/cache \
     XDG_RUNTIME_DIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/runtime \
     QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
     /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/debug/tests/apps/settings_center/qindaqt_settings_navigation_page_test \
       -nocrashhandler
   ```

   Observed: exit 134 after:

   ```text
   PASS   : SettingsNavigationPageTest::initTestCase()
   QWARN  : SettingsNavigationPageTest::testWideTwoColumnLayoutAndRouteSwitching() qindaqt-settings: color Settings1 client unavailable: settings session D-Bus is not connected
   ```

   Expected: the registered Settings tests pass in their documented warning-fatal mode with no host service or bus reachable, using degraded/injected seams as claimed. Actual: the real Color singleton emits an expected availability warning, which the test contract converts into process aborts. This leaves the claimed safe 55-row gate unavailable and makes the reported 55/55 dependent on a potentially host-resolved session bus.

### P2

None.

### P3

None.

## Repair-specific recheck

### Fresh user root and fail-closed controls

The production composition now provisions only the `UserImported` root before client startup (`color_route_composition.cpp:38-69,84`). A fresh build-local XDG home was launched through the real Debug `qindaqt-settings --page color` composition with both bus addresses set to nonexistent sockets. The public C1 provider helper then imported a valid 132-byte ICC fixture. A second launch began with an EUID-owned mode-0777 root; the composition preserved that pre-existing mode and the writer rejected it.

The combined control command exited 0 and observed:

```text
fresh_root mode=700 uid=1000 type=directory
userRoot=.../fresh-home/data/color/icc exists=1 status=0 reason=
unsafe_root mode=777 uid=1000 type=directory
userRoot=.../unsafe-home/data/color/icc exists=1 status=9 reason=invalid-user-root
userRoot=.../parent-home/data/color/icc exists=0 status=9 reason=invalid-user-root
fresh_launch_status=124 fresh_import_status=0
unsafe_launch_status=124 unsafe_import_status=1
rejected_parent_launch_status=124 rejected_parent_root=missing rejected_parent_import_status=1
```

The rejected-parent control used the preserved exact rejected-build Settings binary at `/home/cabewse/work_SPaC3/builds/qindaqt/review-color-codex/debug-665/src/apps/settings_center/qindaqt-settings`. Its process stayed live for the two-second probe, did not create the root, and the same public provider rejected the import. The candidate therefore fixes the prior P1 without weakening mode validation.

The registered `qindaqt.settings-color-composition` row is substantive but its two functions share one temporary home: the first proves creation, and the second proves public-provider import using the created root. The independent process-level control above removes any doubt caused by that ordering.

### Desktop stage closure

`tests/session/DesktopPackageTests.cmake:51-95` registers `desktop.virtual.stage-closure`. It derives imports from the real `src/apps/settings_center/Main.qml`, which imports both `QindaQt.SettingsApp.Color` and `QindaQt.SettingsApp.ColorBackend`. The external Color module must have a regular contained staged `qmldir`; the Color backend is an embedded-static exemption. `_authenticate_qmldirs` rejects exemptions not present in the derived import set (`desktop_session_stage_closure.py:159-183`), so the added ColorBackend exemption is not tautological.

Verbose runs in both profiles reported:

```text
DesktopVirtual stage closure passed: 37 ELF files, 423 DT_NEEDED entries, 17 QML modules, staged applications load, missing-library and missing-QML-module negative controls
```

The row launches the staged Settings root with both buses pinned to nonexistent sockets and uses independent missing-Global-Menu-library and missing-Power-qmldir controls. Thus it exercises the Color route's imported module/backend closure while also proving the guard mechanism fails on omissions.

Cherry-pick fidelity was verified with stable patch IDs:

```text
99b0619  -> 2626610d8f48a43d89a3e99d5c49d06792d71e19
eac04ccb -> 2626610d8f48a43d89a3e99d5c49d06792d71e19
91377acf -> 5948bb3a6d8cbfd46b77ad4d45238aa389994e18
66478c40 -> 5948bb3a6d8cbfd46b77ad4d45238aa389994e18
```

The candidate-only stage change is the bounded additive ColorBackend exemption and corresponding documentation.

## Commands and results

### Identity and cache

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base --is-ancestor b971b43881fcef18980acec03c4e43e56ef9db2a HEAD
git status --porcelain=v1
stat -c '%n exists mode=%a size=%s' /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake
```

Exit 0. The SHAs match the header, the base is an ancestor, and status was empty. The candidate-compatible 6.6.5 cache is available at the required path, mode 0644, size 5024 bytes. It was used unchanged. No current-main/KWin-6.6.6 compatibility claim was attempted; that remains the Program Manager/integration-assistant gate.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited 0.

### Focused and adjacent builds

For each of the Debug and Release build directories, I built `qindaqt-desktop-session-probe`, `qindaqt-settings`, all executable targets backing the 55-row Settings selector, and the candidate's focused Color targets. The initial focused graph completed in 1616/1616 Ninja actions per profile. The ten service-side Settings executables omitted from that first target list were then built explicitly:

```sh
cmake --build <profile-build> --parallel 3 --target \
  qindaqt_settings_protocol_tests qindaqt_settings_protocol_dbus_tests \
  qindaqt_settings_repository_tests qindaqt_settings_service_lifecycle_tests \
  qindaqt_settings_service_process_lifecycle_tests qindaqt_settings_client_tests \
  qindaqt_settings_commit_reply_validation_tests qindaqt_do_not_disturb_controller_tests \
  qindaqt_qt_settings_transport_tests qindaqt_qt_settings_transport_adversarial_tests
```

Both exited 0, 40/40 actions. The install fixtures initially reported the known focused-build precondition that two adjacent staged shell plugins were absent. I built them in both profiles:

```sh
cmake --build <profile-build> --parallel 3 --target \
  qindaqt_global_menu_qmlplugin qindaqt_shell_launcher_qmlplugin
```

Both exited 0, 8/8 actions. The desktop rows then passed. These precondition failures were missing build artifacts, not candidate findings.

### Tests

The implementer-style Settings command (`env -u DBUS_SESSION_BUS_ADDRESS`, isolated HOME/XDG, system address nonexistent) exited 0 with 55/55 in both Debug and Release. That result is not accepted as the safety gate because `DISPLAY=:1` was inherited and the command did not prove the host session bus unreachable.

The stricter no-host-bus Settings command is reproduced in P1 above: Debug exit 8, 52/55; Release exit 8, 52/55, with the same three aborts.

The safe desktop command, with `DISPLAY` and `WAYLAND_DISPLAY` removed, both bus addresses set to nonexistent sockets, and profile-specific isolated HOME/XDG roots, was:

```sh
ctest --test-dir <profile-build> \
  -R '^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)$' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 3/3; stage closure 7.30 seconds.
- Release: exit 0, 3/3; stage closure 6.78 seconds.

Explicit verbose `^desktop\.virtual\.stage-closure$` runs also exited 0, 1/1 in both profiles and produced the 37-ELF/423-edge/17-module report above.

No nested compositor, host D-Bus service, hardware, uinput, or network row was intentionally run.

### Static gates

```sh
./tools/validate-docs
```

Exit 0; validated 136 Markdown documents and `mkdocs.yml` navigation.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/site
```

Exit 0; built in 1.63 seconds.

```sh
./tools/check-source-shape
```

Exit 0; checked 2324 files, skipped 0. It emitted only the existing decomposition-review warnings for `Main.qml`, `src/shell/CMakeLists.txt`, and five existing test files; no new violation.

```sh
git diff --check b971b43881fcef18980acec03c4e43e56ef9db2a..252b2fd7d6d590178b33135af9d64123b1acf6cf
git diff --name-only b971b43881fcef18980acec03c4e43e56ef9db2a..252b2fd7d6d590178b33135af9d64123b1acf6cf -- '*.json'
```

Both exited 0. The JSON enumeration was empty, so there were no changed JSON files to pass to `python3 -m json.tool`.

## Final state

The exact candidate remained checked out and `git status --porcelain=v1` was empty after review. No product path was edited, committed, amended, or rebased. Scratch artifacts are confined to the assigned build root.

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
