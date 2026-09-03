# Marjorie Lee Browne — StatusNotifier tray S1 second-repair recheck

- Persona: **Marjorie Lee Browne**, independent shell reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `0e5fed95535a578c269b86cbfbe7f291f698819b`
- Candidate tree SHA: `5adaf207e77f88b77b2e1c40726a72edb9416925`
- Parent SHA: `a360af742d8a3781c7f809a29a00b4a24ba33084`
- Repair base / rejected ancestor SHA: `4c8e47b28d2d92711bf433c8d2a5afc9be59030f`
- Original product base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex`
- Review surface: full candidate, focused repair diff `git diff 4c8e47b..0e5fed9`
- Worktree integrity: `git status --porcelain` was empty before review and empty after review. The seven relative icon-test fixture directories created by direct binary execution were moved, without deletion, to `<ROOT>/review-icon-fixtures-final`. No product path was edited, committed, amended, or rebased.

## Verdict

**REJECT.** The repair correctly closes the prior generation-reuse and decoder-prose findings, and every registered tray row passes in Debug and Release. A private-bus ownership control nevertheless shows that an unrelated connection can forge the bus daemon's `NameOwnerChanged` signal and retire a still-connected owner's watcher registration and registry item. This violates the exact-owner contract.

`VERDICT REJECT P0/P1/P2/P3=0/1/0/0`

## Findings ledger

### P0

None.

### P1

#### P1-1 — An unrelated bus connection can forge owner loss

- Contract: `docs/wiki/shell/status-tray.md:18-30,35-43,133-143,177-185` and ADR-0032 require items to remain keyed to the exact live unique-name owner and retire only when that name departs. The module boundary requires exact-owner transport.
- Cause: both production subscriptions omit the signal sender. `src/shell/status_notifier/watcher/src/status_notifier_watcher_service.cpp:107-119` and `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp:17-32` connect to path `/org/freedesktop/DBus`, interface `org.freedesktop.DBus`, and member `NameOwnerChanged` with an empty service filter. Their handlers at watcher `:313-323` and monitor `:228-254` trust the three payload strings without authenticating the sender; the watcher also ignores `oldOwner`.
- Reproduction: `<ROOT>/repros/tray_candidate_repros.cpp:249-287` starts one private bus, registers a legitimate item from one connection, keeps that connection live, then has a different connection send `NameOwnerChanged(owner, owner, "")`.

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros/build --parallel 3
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros/build/tray_candidate_repros \
  secondConnectionCannotForgeOwnerLoss -v1
```

Build exit **0**; control exit **1**. Observed while the legitimate item connection remained live: `target live=false`, registry `count=0`, watcher registrations `=0`. Expected: `target live=true`, count `1`, and one watcher registration. The daemon accepted and routed the unrelated connection's signal, so path/interface/member filtering alone is not sender authentication.

### P2

None.

### P3

None.

## Repair closure and regression review

### Second-verdict P1-1: closed

The monitor now keeps `m_ownerGenerations` independently of per-path slots, removes an entry only on exact owner loss, and clears the ledger on watcher-epoch reset. The registered `preservesOwnerGenerationAfterLastPathRetires` control passed on the candidate (3/3 QTest functions) and the exact same candidate test source was rebuilt against an extracted `4c8e47b` source tree, where it failed:

```text
Actual   (replacement.generation): 2
Expected (generation)            : 1
Loc: tst_status_notifier_monitor.cpp(202)
Totals: 2 passed, 1 failed
```

An independent scratch equivalent also failed against `4c8e47b` with owner generation before `1`, after `2`, and passes against the candidate with before `1`, after `1`.

### Second-verdict P3-1: closed

The implementation comment, public header, and owning wiki now consistently distinguish three cases: unknown properties are ignored; wrong-typed presentation-bearing recognized properties fail closed; wrong-typed recorded-only `WindowId`, `OverlayIconName`, `ItemIsMenu`, and `Menu` facts are safe-dropped. The registered `productionProtocolCommentsStayCurrent` control passed on the candidate (3/3). The exact candidate test source rebuilt against extracted `4c8e47b` failed at its forbidden old phrase assertion:

```text
!decoderText.contains("top-level type does not match the wire") returned FALSE
Loc: tst_status_notifier_values.cpp(490)
Totals: 2 passed, 1 failed
```

### Earlier closures and adjacent regressions

- The prior scratch harness rebuilt against the exact candidate and passed all eleven earlier behavioral controls plus init/cleanup: 13 passed, 0 failed, 0 skipped. It reconfirmed the payload-free host-unregistered signal; signed `(int,int)` actions; simultaneous two-path sharing; root path; wrong-typed required-string rejection; theme-root containment; bounded themed image and fallback dimensions; and idempotent degraded start.
- Focused candidate controls passed: watcher foreign-name/no-takeover 3/3; registry spoofed owner, duplicate live identity, and stale post-loss reply 5/5; item-client oversized/count-poison pixmaps, wrong types, and late-reply fencing 6/6; icon byte/dimension poison, root escape, and renderer bounds 5/5.
- The new P1 is distinct from watcher-name takeover: `refusesRegistrationsWhileNameOwnedElsewhere` still proves the second watcher cannot claim the well-known watcher name. The defect is acceptance of an unauthenticated lifecycle signal after a legitimate owner has been admitted.
- DBusMenu reuse was not adopted. The candidate history from `ce9228d` is linear, contains no merge commit, and adds no dbusmenu parser/client or private cross-module dependency. The item client records only `ItemWireDetails::menuObjectPath`; rendering remains explicitly deferred.
- The focused review used only unit/value tests, local files under the assigned build root, and per-test private `dbus-daemon --session` fixtures. No `tests/session`, nested compositor, host session/system bus service, hardware, uinput, or network row ran.

## Exact build and test evidence

### Identity, history, and diff

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base 4c8e47b HEAD
git status --porcelain
git log --graph --decorate --oneline --parents --boundary 4c8e47b..HEAD
git diff --find-renames --find-copies 4c8e47b..HEAD
git rev-list --min-parents=2 ce9228d..HEAD
```

Results: the exact candidate/tree/parent/base are recorded above; initial status was empty. The repair is linear (`4c8e47b -> a360af74 -> 0e5fed95`), and no merge resolution exists to audit. The focused diff changes the owner ledger, registered controls, and aligned prose plus coordination records.

### Configure

Both prescribed commands exited 0. CMake printed the repository's existing mixed Qt search-path warnings.

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

### Focused builds

This exact target command exited 0 in both profiles:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_status_notifier qindaqt_status_notifier_watcher \
  qindaqt_status_notifier_item_client qindaqt_status_notifier_icon \
  qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests \
  qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests \
  qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests \
  qindaqt_status_notifier_values_tests
```

The Debug incremental build emitted `ninja: warning: premature end of file; recovering` for its existing Ninja log, then rebuilt and linked every affected target successfully. Release completed without that warning.

### Discovery and tray selectors

The literal requested discovery pipeline exited 0 in both profiles:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  sh -c "ctest --test-dir <ROOT>/<profile> -N | grep -i -E 'status-notifier|tray'"
```

Because `<ROOT>` itself contains `tray`, it also selected unrelated CTest missing-executable diagnostics from the focused build. The test-name-only refinement exited 0 and listed exactly rows 151–157 in both profiles:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  sh -c "ctest --test-dir <ROOT>/<profile> -N 2>/dev/null | \
    grep -i -E '^  Test +#[0-9]+: .*(status-notifier|tray)'"
```

Rows: values, registry, presentation, watcher, item-client, monitor, icon.

Both exact selectors exited 0 with 7/7 CTest rows passed, 0 failed, 0 skipped:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' \
  --output-on-failure --no-tests=error
```

Direct `-silent` runs under the same environment produced identical per-profile QTest totals: values 18, registry 25, presentation 9, watcher 13, item-client 14, monitor 11, icon 13 = **103 passed, 0 failed, 0 skipped**. There are no tray QML rows; `QT_FATAL_WARNINGS=1` was still set for the entire selector.

### Exact rejected-ancestor assertion verification

An immutable `git archive` of `4c8e47b` was extracted under `<ROOT>/ancestor-4c8-src` and configured under `<ROOT>/ancestor-4c8-debug` with the prescribed Debug recipe. The four tray libraries built successfully. The two candidate test source files were then mechanically overlaid into that scratch source tree and the monitor/values test targets rebuilt. No repository product path was changed.

```sh
git archive 4c8e47b | tar -x -C <ROOT>/ancestor-4c8-src
cmake -S <ROOT>/ancestor-4c8-src -B <ROOT>/ancestor-4c8-debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build <ROOT>/ancestor-4c8-debug --parallel 3 --target \
  qindaqt_status_notifier qindaqt_status_notifier_watcher \
  qindaqt_status_notifier_item_client qindaqt_status_notifier_icon
git archive HEAD tests/shell/status_notifier/tst_status_notifier_monitor.cpp \
  tests/shell/status_notifier/tst_status_notifier_values.cpp | \
  tar -x -C <ROOT>/ancestor-4c8-src
cmake --build <ROOT>/ancestor-4c8-debug --parallel 3 --target \
  qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_values_tests
```

All setup/build commands exited 0. Under the required private-bus environment, `preservesOwnerGenerationAfterLastPathRetires -v1` exited 1 with generation 2 versus 1, and `productionProtocolCommentsStayCurrent -v1` exited 1 on the old forbidden prose. Thus both new assertions independently reject the exact ancestor behavior.

### Static gates

- `./tools/validate-docs` — exit 0; 116 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0; 1,778 source files checked. It printed only the two unrelated pre-existing decomposition warnings: `tests/compositor/CMakeLists.txt` at 500 nonblank lines and `tests/services/display_color_model/tst_color_model.cpp` at 539. The item-client implementation is 496 nonblank lines.
- `git diff --check` — exit 0.
- `git diff --check 4c8e47b..HEAD` — exit 0.
- `git diff --name-only 4c8e47b..HEAD -- '*.json'` — exit 0 with empty output; `python3 -m json.tool` is not applicable because no JSON changed.

## Final tree integrity

```text
git rev-parse HEAD        = 0e5fed95535a578c269b86cbfbe7f291f698819b
git rev-parse HEAD^{tree} = 5adaf207e77f88b77b2e1c40726a72edb9416925
git rev-parse HEAD^       = a360af742d8a3781c7f809a29a00b4a24ba33084
git rev-parse 4c8e47b     = 4c8e47b28d2d92711bf433c8d2a5afc9be59030f
git rev-parse ce9228d     = ce9228d9694622d503d92a38d01986f8f124f188
git status --porcelain    = empty
```

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
