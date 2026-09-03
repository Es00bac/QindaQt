# Rózsa Péter — independent shell review

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate SHA: `b10692c98f202e5bfc616fd9bfa0767f5bcda57e`
- Tree SHA: `d8bb4a6b3aec5e406289728296e609ad64cda06b`
- Parent SHA: `cfbe0cc61288bc5c157c0e55f9d66779defd8442`
- Base SHA: `893805724933b307e165fdefc485df1ae4a13015`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-applet-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex`

## Findings ledger

### P0

None.

### P1

#### P1-1 — rejected live updates change registry presentation state without notifying the controller, and the degradation has no production acknowledgement path

`StatusNotifierMonitorAdapter::ForwardingSink::forward()` emits `changed()` only for an accepted registry outcome (`src/shell/status_notifier/applet/src/status_notifier_monitor_adapter.cpp:77`). That is not equivalent to “presentation did not change.” In particular, `StatusNotifierRegistry::registerItem()` rejects a malformed replacement but first changes `m_degradedReason` to `malformed-item-replacement` while retaining the last-known-good descriptor (`src/shell/status_notifier/src/status_notifier_registry.cpp:189`). Capacity rejection similarly changes the reason before returning a rejected outcome (`src/shell/status_notifier/src/status_notifier_registry.cpp:213`).

This violates the documented projection contract: `docs/wiki/shell/status-tray.md:61` says a degraded registry must be presented as `degraded`, and lines 200–204 say a malformed live replacement is rejected, marks the registry degraded, and retains the last-known-good descriptor until acknowledgement. The adapter owns the registry privately, and the candidate has no production call to `StatusNotifierRegistry::acknowledgeDegraded()`; repository search finds calls only in registry tests/test support. Consequently, the controller first lies as `ready` after the malformed update, then changes to `degraded` only when an unrelated accepted update finally emits `changed()`, and remains sticky thereafter.

Exact reproduction (scratch sources are under the assigned build root):

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/repro-degraded \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/repro-degraded/build -G Ninja
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/repro-degraded/build --parallel 3
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/repro-degraded/build/repro
```

Exit status: `0`. The executable used the candidate's real watcher, monitor, item client, registry, adapter, controller, and a private `dbus-daemon`; it did not connect to a host bus.

Observed:

```text
initial adapter=1 controller=ready
after-malformed adapter=3 reason=malformed-item-replacement controller=ready controllerReason=
after-valid adapter=3 reason=malformed-item-replacement controller=degraded controllerReason=malformed-item-replacement
```

Expected: as soon as the current-owner malformed replacement changes registry presentation to degraded, the controller must be notified and project `degraded` while retaining its last-known-good row. The composed production boundary must also expose or apply the contract's acknowledgement/recovery transition; a later valid update must not be the event that first reveals an earlier degradation.

### P2

None.

### P3

None.

## Contract and boundary audit

- Owner, watcher-epoch, and owner-generation checks are present in S1 and are revalidated at the controller seam before dispatch. Owner loss/replacement and stale keys are exercised by the candidate tests. The applet boundary remains narrow: QML imports no D-Bus/platform module, and the standalone poison probe accepted all 11 real source/QML files and rejected its injected prohibited-authority cases.
- Item presentation is capped at 24, registry membership at 64, menu projection at 128 rows/depth 8, and icon inputs/outputs are bounded. The focused model, controller, icon, installed-package, and QML rows passed. The P1 above is the exception to truthful degraded-state projection.
- Compiled QST/Controls QML ran with `QT_FATAL_WARNINGS=1`. Candidate accessibility, status/attention/placeholder, keyboard activation/context menu, and overflow tests passed. A scratch compiled-QML test additionally covered horizontal and vertical Tab/Backtab traversal and both wrap endpoints; it passed in Debug and Release (4/4 assertions as QtTest cases in each profile).
- Registration changes are additive. The status-notifier package, least-authority policy rows, integrity checks, relocation poison, component closure, dispatcher test stub, and safe DesktopVirtual staging selectors passed. Hosting remains absent.
- The two-manifest caveat is real but accurately documented as future work: `system-tray` and `status-notifier` advertise the same placement and status-item capabilities under different ids/entry points, while only `system-tray` has `collapsePassive`. Neither is hosted by the current built-in dispatcher/profile. A later hosting change must select or migrate one identity/settings contract; hosting both would make tray ownership and duplicate presentation ambiguous. The candidate does not claim that reconciliation is complete.

## Commands and results

### Immutable tree checks

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
```

Results: candidate, tree, and parent matched the lane values above; status produced no output before review and again after all review work.

### Configure

Debug:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release used the identical command with `-B .../release -DCMAKE_BUILD_TYPE=Release`.

Results: Debug `0`; Release `0`.

### Focused builds

For each of Debug and Release:

```sh
cmake --build <BUILD> --parallel 3 --target \
  qindaqt_status_notifier_values_tests \
  qindaqt_status_notifier_registry_tests \
  qindaqt_status_notifier_presentation_tests \
  qindaqt_status_notifier_watcher_tests \
  qindaqt_status_notifier_item_client_tests \
  qindaqt_status_notifier_monitor_tests \
  qindaqt_status_notifier_icon_tests \
  qindaqt_status_notifier_applet_model_tests \
  qindaqt_status_notifier_applet_controller_tests \
  qindaqt_status_notifier_applet_adapter_tests \
  qindaqt_status_notifier_applet_qml_tests \
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
  qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests \
  qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests \
  qindaqt-shell qindaqt-desktop-session-probe
cmake --build <BUILD> --parallel 3 --target qindaqt_controls_font_pinning_tests
cmake --build <BUILD> --parallel 3 --target \
  qindaqt-shell-preview qindaqt_global_menu_qmlplugin qindaqt_shell_launcher_qmlplugin
```

Results: all builds exited `0`; the primary focused graph completed 1588/1588 steps in each profile. The last three installable targets were needed by the pre-existing shell/DesktopVirtual component-closure scripts in a clean build root.

### Focused and adjacent CTest selectors

Every selector below was run in both Debug and Release with this environment prefix:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1
```

Commands and final results per profile:

```sh
ctest --test-dir <BUILD> -R '^qindaqt\.controls-font-pinning$' \
  --output-on-failure --no-tests=error
# Debug 1/1, Release 1/1; both exit 0.

ctest --test-dir <BUILD> -R '^qindaqt\.status-notifier-' \
  --output-on-failure --no-tests=error
# Debug 15/15, Release 15/15; both exit 0.

ctest --test-dir <BUILD> \
  -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$' \
  --output-on-failure --no-tests=error
# Debug 6/6, Release 6/6; both exit 0.

ctest --test-dir <BUILD> -R '^qindaqt\.shell-runtime-component-closure$' \
  --output-on-failure --no-tests=error
# Debug 1/1, Release 1/1; both exit 0.

ctest --test-dir <BUILD> \
  -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' \
  --output-on-failure --no-tests=error
# Debug 3/3, Release 3/3; both exit 0.
```

The first clean-root status-selector attempt exited `8` because all three QML rows require the persistent pinned-theme output and `qindaqt.controls-font-pinning` had been built but not yet executed. After running that documented prerequisite, the unchanged exact selector passed 15/15 in each profile. Initial component/stage-closure attempts likewise exited `8` on missing unbuilt install artifacts (`qindaqt-shell-preview`, then launcher/global-menu plugins); after building those adjacent targets, the unchanged selectors passed as reported. These were prerequisite/build-graph omissions in the review invocation, not runtime failures of the candidate.

No nested-compositor, host-bus, hardware, uinput, or network row was run.

### Extra compiled-QML orientation probe

For each profile:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  QML2_IMPORT_PATH=<BUILD>/qml \
  <BUILD>/tests/shell/status_notifier/applet/qindaqt_status_notifier_applet_qml_tests \
  -input /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/qml-orientation/tst_StatusNotifierAppletOrientation.qml
```

Results: Debug `4 passed, 0 failed`, exit `0`; Release `4 passed, 0 failed`, exit `0`. Two earlier runs exited `2` because the reviewer-authored scratch fixture accidentally exposed zero model rows; correcting that scratch-only fixture produced the results above. No candidate file was changed.

### Standalone boundary poison gate

```sh
cmake \
  -DQINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR=/home/cabewse/work_SPaC3/container-wm-workers/tray-applet-codex-review/src/shell/status_notifier/applet \
  -DQINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY=/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/boundary-poison \
  -P /home/cabewse/work_SPaC3/container-wm-workers/tray-applet-codex-review/tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake
```

Result: exit `0`; `Validated 11 Status Notifier Applet source/QML files and poison probe rejection`.

### Static and JSON gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/site
./tools/check-source-shape
git diff --check
git diff --check 893805724933b307e165fdefc485df1ae4a13015 \
  b10692c98f202e5bfc616fd9bfa0767f5bcda57e
python3 -m json.tool data/applet-policy/default.json >/dev/null
python3 -m json.tool data/applets/status-notifier.json >/dev/null
```

Results: every command exited `0`. `validate-docs` validated 139 Markdown documents and navigation. MkDocs strict completed successfully. Source-shape checked 2433 files with no allowlisted skips and emitted nine non-fatal decomposition-review warnings; the candidate-touched one is `src/shell/CMakeLists.txt` at 543 non-blank lines (it was already 502 at the base). Both diff checks and both JSON parses were clean.

## Verdict

The exact candidate violates the truthful degraded-state contract at its real S1-to-S2 composition seam. ACCEPT requires P1 = 0, so the candidate is rejected.

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
