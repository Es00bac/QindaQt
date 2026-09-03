# Handoff — tray-s1 production StatusNotifier transports

- Worker: Jean Sammet (`jean-sammet`), Moonshot Kimi `kimi-code/kimi-for-coding`
- Feature: QQ-004.11 Status-notifier tray (WIRED foundation → production transports)
- Candidate commit: `9d1a30b02729c0aba6deba9a43fa67418d283e76`
- Candidate tree: `cea5680be27dd9c31c9afd8482fee8ef02c55fe4`
- Exact base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch: `worker/tray-s1`; worktree `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1`

## Changed paths (sorted, candidate vs base)

- `docs/wiki/architecture/module-boundaries.md` (additive rows: watcher, item_client, icon)
- `docs/wiki/development/testing-harness.md` (status-notifier proof section updated)
- `docs/wiki/shell/status-tray.md` (production-transport sections)
- `src/shell/status_notifier/CMakeLists.txt`
- `src/shell/status_notifier/icon/**` (locator + renderer)
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_limits.h`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_types.h`
- `src/shell/status_notifier/item_client/**` (async item reader + registry monitor)
- `src/shell/status_notifier/src/status_notifier_registry.cpp`
- `src/shell/status_notifier/watcher/**` (StatusNotifierWatcher service)
- `tests/shell/status_notifier/CMakeLists.txt`
- `tests/shell/status_notifier/status_notifier_fake_item_test_support.h`
- `tests/shell/status_notifier/status_notifier_private_bus_test_support.h`
- `tests/shell/status_notifier/tst_status_notifier_icon.cpp`
- `tests/shell/status_notifier/tst_status_notifier_item_client.cpp`
- `tests/shell/status_notifier/tst_status_notifier_monitor.cpp`
- `tests/shell/status_notifier/tst_status_notifier_watcher.cpp`

Note: the candidate tip is two commits over base — `b77373c6` preserved the
interrupted in-progress work verbatim; `9d1a30b0` completes and repairs it
(decoder hardening, monitor wiring/signal connects, private-bus test
lifetime fixes, fake-item header split, docs).

## Evidence (all run in this worktree, build root
`/home/cabewse/work_SPaC3/builds/qindaqt/tray-s1`)

Focused builds (warnings-as-errors, strict), exit 0 both profiles:

```
cmake --build <ROOT>/debug  --parallel 3 --target qindaqt_status_notifier qindaqt_status_notifier_watcher qindaqt_status_notifier_item_client qindaqt_status_notifier_icon qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests qindaqt_status_notifier_values_tests
```

(same command against `<ROOT>/release`; release configured with the lane
recipe, `-DCMAKE_BUILD_TYPE=Release`.)

Test rows, `ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' --output-on-failure --no-tests=error`:

- Debug: 7/7 rows passed (values, registry, presentation, watcher, item-client, monitor, icon).
- Release: 7/7 rows passed.

Per-row test-function totals (each binary's `Totals:` line):

- Debug: watcher 11, item-client 12, monitor 8, icon 11, registry 25, presentation 9, values 17 → 93 passed, 0 failed, 0 skipped.
- Release: watcher 11, item-client 12, monitor 8, icon 11, registry 25, presentation 9, values 17 → 93 passed, 0 failed, 0 skipped.

Static gates (worktree root), exit 0:

- `./tools/validate-docs`
- `mkdocs build --strict --site-dir <ROOT>/site` (docs venv)
- `./tools/check-source-shape` (no warnings on this lane's files)
- `git diff --check`

No host-desktop interaction: every transport row spawns its own private
`dbus-daemon --session`; no nested compositor, no uinput, no network, no
hardware.

## Bounded caveats

- `Menu` is recorded (`ItemWireDetails::menuObjectPath`) but not rendered;
  dbusmenu is the Global Menu G1 lane's shared adapter, composed later.
- Evidence is private-bus with scripted fakes: no host session bus, no real
  third-party items (e.g. KDE apps), no rendered panel tray, no
  assistive-technology bridge.
- `QDBusConnection::disconnectFromBus` does not close a connection while a
  copy is alive; the owner-loss rows drop their last reference first. This
  is a test-fake lifecycle fact, not a production claim.
- No ADR added: no process boundary changed (transports implement the
  ADR-0032 sink on an injected connection).

## Requested next action

Independent exact review of `9d1a30b02729c0aba6deba9a43fa67418d283e76`, then
manager integration.
