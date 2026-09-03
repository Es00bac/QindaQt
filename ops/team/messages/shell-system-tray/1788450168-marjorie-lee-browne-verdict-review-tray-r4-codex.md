# Marjorie Lee Browne — StatusNotifier tray S1 third-repair recheck

- Persona: **Marjorie Lee Browne**, independent shell reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `68009bb47fccb7c6e674c2cee86ba874decd97f6`
- Candidate tree SHA: `0c7f22090313948ab517e0021b0372e29e4927bf`
- Parent SHA: `a8803649d5dd38ea3dcb14c454fd07905fd744e4`
- Repair base / rejected ancestor SHA: `0e5fed95535a578c269b86cbfbe7f291f698819b`
- Original product base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex`
- Review surface: exact repair diff `git diff 0e5fed9..68009bb`, with the whole candidate lineage and prior closures retained in view
- Worktree integrity: `git status --porcelain` was empty before review and empty after all builds, tests, and static gates. No product path was edited, committed, amended, or rebased. Scratch ancestor sources and test artifacts stayed under the assigned build root.

## Verdict

**ACCEPT.** The repair closes the forged owner-loss defect with daemon-sender authentication in both production subscribers and exact unique-name loss-tuple validation in both handlers. The registered control is non-vacuous against `0e5fed9`, genuine disconnect retirement still works, all earlier controls remain closed, and no P0–P3 defect was found.

`VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Repair closure and regression review

### Third-verdict P1-1: closed

- `src/shell/status_notifier/watcher/src/status_notifier_watcher_service.cpp:107-127` now installs the `NameOwnerChanged` subscription with service `org.freedesktop.DBus` and fails startup closed if that match cannot be installed. Its handler at `:311-320` accepts only `name == oldOwner`, a valid unique name, and an empty `newOwner`.
- `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp:17-34` uses the same daemon sender filter. Its handler at `:229-255` applies the same exact loss-tuple checks before emitting the generation-fenced owner-loss event.
- The prior independent reproducer `secondConnectionCannotForgeOwnerLoss` rebuilt against the candidate and exited **0**: 3 passed, 0 failed, 0 skipped. With the legitimate connection still live it observed `target live=true`, registry count `1`, watcher registrations `1`.
- The registered `rejectsPeerForgedOwnerLoss` control exited **0** on the candidate: 3 passed, 0 failed, 0 skipped. The exact candidate test source was then mechanically overlaid on an immutable `git archive` of `0e5fed9`; it exited **1** there at `tst_status_notifier_monitor.cpp:325`, where `registry.isOwnerLive(owner)` was false: 2 passed, 1 failed. This proves the assertion rejects the ancestor behavior rather than passing vacuously.
- The adjacent genuine-disconnect control `removesItemAndFreesOwnerOnDisconnect` exited **0** on the candidate: 3 passed, 0 failed, 0 skipped. It confirms the authenticated daemon signal still removes the item and frees the bounded owner slot.

### Earlier closures and requested regressions

- The complete prior independent reproducer passed **14/14** QTest functions with no failures or skips. This reconfirmed standard watcher properties, idempotent degraded startup, host-unregistered signaling, signed action coordinates, simultaneous and sequential same-owner path lineage, root paths, strict recognized-property types, theme-root containment, and decoded/fallback icon bounds in addition to forged-loss rejection.
- The full registered tray suite passed in Debug and Release. Per profile: values 18, registry 25, presentation 9, watcher 13, item-client 14, monitor 12, icon 13 = **104 passed, 0 failed, 0 skipped**. This includes watcher no-takeover truth, registry exact-owner/generation fencing, hostile item-property and pixmap bounds, and icon containment/dimension bounds.
- DBusMenu reuse was not adopted in this candidate. `git rev-list --min-parents=2 ce9228d..HEAD` produced no merge commits, the focused repair diff contains no dbusmenu/global-menu path, and the item client continues only to record `menuObjectPath`; the owning wiki still explicitly defers rendering/composition.
- The review used unit/value tests, local build-root files, and test-spawned private session buses only. No `tests/session`, nested compositor, host session/system service, hardware, uinput, or network row ran.

## Exact commands and results

### Identity, history, and diff

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base 0e5fed9 HEAD
git merge-base ce9228d HEAD
git status --porcelain
git log --graph --decorate --oneline --parents --boundary 0e5fed9..HEAD
git rev-list --min-parents=2 ce9228d..HEAD
git diff --stat 0e5fed9..HEAD
git diff --name-status 0e5fed9..HEAD
git diff --check 0e5fed9..HEAD
```

All commands exited **0**. The exact candidate/tree/parent/bases are recorded above; status was empty. The repair history is linear (`0e5fed9 -> a8803649 -> 68009bb4`), and there is no merge resolution. The product repair commit changes two production subscriptions and handlers, one registered monitor test, their public comments, and the two relevant wiki sections; the intermediate commit contains coordination records only.

### Prescribed configure

Both commands exited **0**. They emitted the repository's existing mixed Qt runtime/dependent search-path warnings.

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

### Focused build

This exact command exited **0** in Debug and Release:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_status_notifier qindaqt_status_notifier_watcher \
  qindaqt_status_notifier_item_client qindaqt_status_notifier_icon \
  qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests \
  qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests \
  qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests \
  qindaqt_status_notifier_values_tests
```

The Debug incremental build printed `ninja: warning: premature end of file; recovering` for its existing Ninja log and then successfully rebuilt and linked all affected targets. Release completed successfully without that warning.

### Discovery and tray selectors

The literal requested discovery pipeline exited **0** in both profiles:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  sh -c "ctest --test-dir <ROOT>/<profile> -N | grep -i -E 'status-notifier|tray'"
```

Because the build-root path contains `tray`, this literal pipeline also matched CTest's unrelated missing-executable diagnostics from the intentionally focused build. The test-name-only refinement exited **0** and listed exactly rows 151–157 in each profile:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  sh -c "ctest --test-dir <ROOT>/<profile> -N 2>/dev/null | \
    grep -i -E '^  Test +#[0-9]+: .*(status-notifier|tray)'"
```

Rows: values, registry, presentation, watcher, item-client, monitor, icon.

The selector below exited **0** in Debug and Release with **7/7 CTest rows passed**, 0 failed:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' \
  --output-on-failure --no-tests=error
```

Direct `-silent` runs under the same environment produced **104 passed, 0 failed, 0 skipped** in each profile. There are no tray QML rows; `QT_FATAL_WARNINGS=1` was still set for the full selector.

### Independent repair and ancestor controls

```sh
cmake -S <ROOT>/repros -B <ROOT>/repros/build -G Ninja
cmake --build <ROOT>/repros/build --parallel 3
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  <ROOT>/repros/build/tray_candidate_repros \
  secondConnectionCannotForgeOwnerLoss -v1
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  <ROOT>/debug/tests/shell/status_notifier/qindaqt_status_notifier_monitor_tests \
  rejectsPeerForgedOwnerLoss -v1
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  <ROOT>/debug/tests/shell/status_notifier/qindaqt_status_notifier_monitor_tests \
  removesItemAndFreesOwnerOnDisconnect -v1
```

Configure/build and all three candidate controls exited **0**, each focused control reporting 3 passed, 0 failed, 0 skipped.

The ancestor assertion setup remained under `<ROOT>`:

```sh
mkdir -p <ROOT>/ancestor-0e-src
git archive 0e5fed9 | tar -x -C <ROOT>/ancestor-0e-src
git archive HEAD tests/shell/status_notifier/tst_status_notifier_monitor.cpp | \
  tar -x -C <ROOT>/ancestor-0e-src
cmake -S <ROOT>/ancestor-0e-src -B <ROOT>/ancestor-0e-debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build <ROOT>/ancestor-0e-debug --parallel 3 --target \
  qindaqt_status_notifier_monitor_tests
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  <ROOT>/ancestor-0e-debug/tests/shell/status_notifier/qindaqt_status_notifier_monitor_tests \
  rejectsPeerForgedOwnerLoss -v1
```

Setup, configure, and build exited **0**. The final control exited **1 as expected** with 2 passed and 1 failed at the live-owner assertion, proving the registered test detects `0e5fed9`.

The complete independent regression executable was also run:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  <ROOT>/repros/build/tray_candidate_repros -silent
```

Exit **0**: 14 passed, 0 failed, 0 skipped.

### Static gates

- `./tools/validate-docs` — exit **0**; 116 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` — exit **0**.
- `./tools/check-source-shape` — exit **0**; 1,778 source files checked. It reported only the two unrelated existing threshold warnings: `tests/compositor/CMakeLists.txt` at 500 nonblank lines and `tests/services/display_color_model/tst_color_model.cpp` at 539.
- `git diff --check` — exit **0**.
- `git diff --check 0e5fed9..HEAD` — exit **0**.
- `git diff --name-only 0e5fed9..HEAD -- '*.json'` — exit **0** with empty output; `python3 -m json.tool` was not applicable because no JSON changed.

## Final tree integrity

```text
git rev-parse HEAD        = 68009bb47fccb7c6e674c2cee86ba874decd97f6
git rev-parse HEAD^{tree} = 0c7f22090313948ab517e0021b0372e29e4927bf
git rev-parse HEAD^       = a8803649d5dd38ea3dcb14c454fd07905fd744e4
git rev-parse 0e5fed9     = 0e5fed95535a578c269b86cbfbe7f291f698819b
git rev-parse ce9228d     = ce9228d9694622d503d92a38d01986f8f124f188
git status --porcelain    = empty
```

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
