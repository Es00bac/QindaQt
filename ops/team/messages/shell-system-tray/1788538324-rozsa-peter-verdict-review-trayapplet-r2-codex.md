# Rózsa Péter — independent shell recheck

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate SHA: `544d1c30c2d61a5b3e12e7002ba274326dcc736c`
- Tree SHA: `8add8cdd8f1f3377b8f413bcb4921cc07606045a`
- Parent SHA: `82990ab8142da480885312c65d70ef9e51eb6795`
- Base SHA: `893805724933b307e165fdefc485df1ae4a13015`
- Prior rejected candidate: `b10692c98f202e5bfc616fd9bfa0767f5bcda57e`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-applet-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex`

## Findings ledger

### P0

None.

### P1

None. The prior P1-1 is repaired. A malformed current-owner replacement and a
membership-capacity rejection now notify the controller on the degradation
transition itself, retain the last-known-good rows, and remain degraded until
the admitted acknowledgement action clears the registry marker. The composed
controller/seam/adapter boundary then reprojects synchronously to `ready`.

### P2

None.

### P3

None.

## Repair and contract audit

- `StatusNotifierMonitorAdapter::ForwardingSink` snapshots the registry's
  degradation reason before each forwarded event, executes the event once, and
  emits `changed()` when the outcome was accepted or the degradation marker
  changed. Rejected stale/hostile events that leave presentation unchanged do
  not notify.
- `acknowledgeDegraded()` is the sole non-item mutation on the least-authority
  source seam. It is a no-op without a pending registry degradation, and the
  controller refuses to call it when read observation is denied or the source
  is absent. Synchronous `changed()` re-entry only reprojects; item dispatch
  remains protected by the existing exactly-once guard and retains its local
  and seam-side generation checks.
- The two new private-bus adapter rows are mutation-sensitive: both fail on the
  exact old candidate with the adapter degraded while the controller remains
  `ready`, and pass on the repaired candidate. The direct candidate executions
  reported zero skips. The controller rows cover synchronous acknowledgement,
  retained rows, no-op acknowledgement, and denied/absent-source refusal.
- The full base-to-candidate registration and build-file changes are additive.
  The public S1 registry/item-client/icon boundaries remain intact, the applet
  opens no bus and receives only an injected connection, and no host desktop,
  host bus, hardware, uinput, or network authority was exercised.
- Hosting remains absent: neither `src/shell/qml` nor `src/shell/runtime` has a
  base-to-candidate product change or a production import/reference for
  `QindaQt.Shell.StatusNotifier` / `qindaqt.applets.status-notifier`. The
  candidate only registers and stages the applet ahead of the later dispatcher
  lane, matching its explicit non-claim.

## Commands and results

### Immutable tree checks

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
```

Results: the candidate, tree, and parent matched the values above. Status
produced no output before review and after all review work. The old-candidate
scratch checkout likewise resolved exactly to
`b10692c98f202e5bfc616fd9bfa0767f5bcda57e` and remained clean.

### Configure

Debug:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-applet-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release used the identical command with `-B .../release` and
`-DCMAKE_BUILD_TYPE=Release`. Both exited `0`.

### Focused builds

For each profile:

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
  qindaqt-shell qindaqt-desktop-session-probe \
  qindaqt_controls_font_pinning_tests qindaqt-shell-preview \
  qindaqt_global_menu_qmlplugin qindaqt_shell_launcher_qmlplugin
```

Results: Debug `0`; Release `0`, with strict warnings enabled.

### Prior-P1 candidate reproduction

The scratch reproduction under `<ROOT>/repro-degraded` was extended outside
the product worktree to assert immediate controller projection, retained-row
acknowledgement, and subsequent valid replacement:

```sh
cmake -S <ROOT>/repro-degraded -B <ROOT>/repro-degraded/build -G Ninja
cmake --build <ROOT>/repro-degraded/build --parallel 3
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  <ROOT>/repro-degraded/build/repro
```

Result: exit `0`.

```text
initial adapter=1 controller=ready
after-malformed adapter=3 reason=malformed-item-replacement controller=degraded controllerReason=malformed-item-replacement
after-ack adapter=1 controller=ready rows=1
after-valid adapter=1 reason= controller=ready controllerReason=
```

### Exact `b10692c` negative controls

```sh
git worktree add --detach <ROOT>/b10692c-checkout \
  b10692c98f202e5bfc616fd9bfa0767f5bcda57e
cmake -S <ROOT>/b10692c-checkout -B <ROOT>/b10692c-build -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build <ROOT>/b10692c-build --parallel 3 \
  --target qindaqt_status_notifier_applet_adapter_tests
cmake -S <ROOT>/b10692c-negative-control \
  -B <ROOT>/b10692c-negative-control/build -G Ninja
cmake --build <ROOT>/b10692c-negative-control/build --parallel 3
```

All setup/build commands exited `0`. Two external scratch probes used the old
candidate's real watcher/monitor/registry/adapter/controller on private buses:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  <ROOT>/b10692c-negative-control/build/negative-control malformed
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  <ROOT>/b10692c-negative-control/build/negative-control capacity
```

Both exited `1`, as required for the negative control. Observed:

```text
malformed adapter=3 reason=malformed-item-replacement controller=ready rows=1
capacity adapter=3 reason=item-capacity-exceeded controller=ready items=64
```

### Focused and adjacent CTest selectors

Every selector below was run in Debug and Release with:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1
```

Commands and results per profile:

```sh
ctest --test-dir <BUILD> -R '^qindaqt\.controls-font-pinning$' \
  --output-on-failure --no-tests=error
# Debug 1/1, Release 1/1; exit 0.

ctest --test-dir <BUILD> -R '^qindaqt\.(status-notifier-|applet)' \
  --output-on-failure --no-tests=error
# Debug 21/21, Release 21/21; exit 0.

ctest --test-dir <BUILD> -R '^qindaqt\.shell-runtime-component-closure$' \
  --output-on-failure --no-tests=error
# Debug 1/1, Release 1/1; exit 0.

ctest --test-dir <BUILD> \
  -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' \
  --output-on-failure --no-tests=error
# Debug 3/3, Release 3/3; exit 0.
```

The two repaired adapter functions and two controller acknowledgement functions
were also invoked by name in each profile. Each executable reported `4 passed,
0 failed, 0 skipped` (two selected functions plus init/cleanup), exit `0`.

No nested-compositor, host-D-Bus, hardware, uinput, or network row was run.

### Boundary and static gates

```sh
cmake \
  -DQINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR=/home/cabewse/work_SPaC3/container-wm-workers/tray-applet-codex-review/src/shell/status_notifier/applet \
  -DQINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY=<ROOT>/boundary-poison-r2 \
  -P tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir <ROOT>/site-r2
./tools/check-source-shape
git diff --check
git diff --check 893805724933b307e165fdefc485df1ae4a13015..544d1c30c2d61a5b3e12e7002ba274326dcc736c
git diff --check b10692c98f202e5bfc616fd9bfa0767f5bcda57e..544d1c30c2d61a5b3e12e7002ba274326dcc736c
python3 -m json.tool data/applet-policy/default.json >/dev/null
python3 -m json.tool data/applets/status-notifier.json >/dev/null
```

Results: all exited `0`. The boundary gate validated 11 files and its poison
rejections; docs validated 139 Markdown files/navigation; MkDocs strict built
successfully; source-shape checked 2433 files with zero allowlisted skips and
nine non-fatal pre-existing decomposition-review warnings; both changed JSON
documents parsed successfully.

## Verdict

The exact candidate repairs the rejected degraded-state behavior, supplies the
documented fail-closed acknowledgement/recovery boundary, preserves fencing and
ownership, and keeps hosting absent. No blocking or nonblocking defect was
found.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
