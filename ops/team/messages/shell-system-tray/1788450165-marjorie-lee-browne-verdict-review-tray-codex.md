# Marjorie Lee Browne — independent StatusNotifier tray S1 exact-candidate review

- Persona: **Marjorie Lee Browne**, independent shell reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Reviewed at: 2026-09-03T06:35:39-06:00
- Exact candidate SHA: `9d1a30b02729c0aba6deba9a43fa67418d283e76`
- Candidate tree SHA: `cea5680be27dd9c31c9afd8482fee8ef02c55fe4`
- Parent SHA: `b77373c6581f3841e878f1d542ceecfbf762e2eb`
- Exact base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1-codex-review`
- Review diff: `git diff ce9228d..9d1a30b`
- Scratch reproduction root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros`
- Worktree state: `git status --porcelain` was empty before review and after all review work. No product path was edited.

## Verdict

**REJECT.** The exact candidate builds and its seven registered rows pass in both profiles, but eight P1 defects have private-bus or build-root-only unit reproductions. There is also one P2 idempotence defect and one P3 stale source comment.

`VERDICT REJECT P0/P1/P2/P3=0/8/1/1`

## Scope resolution and answers to the review questions

1. **Watcher ownership is only partially truthful.** The private-bus candidate row proves bare-path/service-name registration, the 64-item limit, item owner-loss retirement, foreign-name degradation, and release on `stop()`. However, the promised fourth protocol signal is absent (P1-1), and repeated `start()` changes its result while degraded (P2-1).
2. **Item facts are not fail-closed for wrong wire types, and timeout proof is not real.** The foundation rows prove text/control-character and pixmap descriptor bounds after values exist, and the client rows prove a 513-pixel pixmap and a nine-entry pixmap list are rejected. But integers in string properties are coerced into accepted presentation text (P1-5). The only alleged silent-item timeout test contacts a nonexistent owner and completes on the immediate D-Bus error; the public result has no typed timeout outcome (P1-6).
3. **DBusMenu is explicitly excluded by the lane's authoritative Outcome/Excluded sections.** The diff contains no dbusmenu decoder/client, menu-layout activation, revision logic, or applet registration, so there is no duplicate client relative to the candidate's own base. It records only `ItemWireDetails::menuObjectPath`, and `docs/wiki/shell/status-tray.md:272-276` keeps rendering/composition excluded. Therefore menu revision fencing and cross-connection menu-update tests are not applicable to this S1 candidate. The separate StatusNotifierItem activation transport is in scope and uses the wrong D-Bus signature (P1-2). Item property signal subscriptions themselves are filtered to the item's exact unique-name sender at `status_notifier_item_client.cpp:285-301`.
4. **Icon behavior is not confined or fully bounded.** Ordinary missing-icon fallback is deterministic in the registered row, but an `index.theme` directory can escape the injected root (P1-7), and decoded theme images/placeholders can exceed the 512-pixel image bound (P1-8).
5. **Registered tests and static gates are green but do not prove all documented claims.** Exactly seven `qindaqt.status-notifier-*` rows pass in Debug and Release under an unset host session/display environment and `QT_FATAL_WARNINGS=1`, totaling 93 QTest functions per profile. There are no QML/offscreen rows in this candidate, so an offscreen-QML selector is not applicable. All transport rows use private `dbus-daemon --session` fixtures; no `tests/session`, nested compositor, host bus, hardware, uinput, or network row was run. The applet remains documented as `implementation-unavailable`. Static gates pass, subject to two unrelated pre-existing source-shape warnings recorded below.

## Findings ledger

### P0

None.

### P1

#### P1-1 — The watcher never emits `StatusNotifierHostUnregistered`

- Contract: `docs/wiki/shell/status-tray.md:136-143` and `status_notifier_watcher_service.h:32-47` claim all four protocol signals and matching retirement signals.
- Cause: `src/shell/status_notifier/watcher/src/status_notifier_watcher_service.cpp:366-372` removes the host and emits only local `stateChanged()`, explicitly asserting that no host-unregistered signal exists. No exported or manually sent `StatusNotifierHostUnregistered` is present. `tst_status_notifier_watcher.cpp:214-242` registers a host but never disconnects it or observes the required signal.
- Reproduction (private bus, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros hostUnregisteredSignalIsMissing -v1
```

Observed: after the host owner disappeared, `registeredHosts()` became empty but the wire signal count was `0`. Expected: exactly `1` `StatusNotifierHostUnregistered` signal.

#### P1-2 — Activate/SecondaryActivate/ContextMenu use `(int,uint)` instead of the protocol `(int,int)` signature, and the fake makes the test tautological

- Cause: `src/shell/status_notifier/item_client/src/status_notifier_item_client.cpp:376-391` wraps `y` as `quint32`, producing D-Bus signature `iu`. The in-repo fake repeats the defect at `tests/shell/status_notifier/status_notifier_fake_item_test_support.h:109-115`, so `dispatchesRecordedIntents` cannot detect it.
- Reproduction (private bus, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros signedCoordinateMethodIsNotInvoked -v1
```

Observed: QtDBus reported `couldn't handle call to Activate, no slot matched`; a strict `Activate(int,int)` item recorded `0` invocations. Expected: `1`. This affects positive coordinates too because the wire type, not only the value, is wrong.

#### P1-3 — Two valid item paths from one owner reissue the owner generation and wedge initial population

- Contract: ADR-0032 and `docs/wiki/shell/status-tray.md:24-62` issue one current generation per unique-name owner and allow multiple `(owner,path)` keys.
- Cause: `StatusNotifierItemMonitor::watchItemOwner` calls `beginOwnerGeneration` for every new path at `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp:389-418`. The second path rebases the still-live owner, invalidates the first client, and the first client's fenced timeout emits nothing, leaving `m_populationOutstanding` nonzero.
- Reproduction (one private-bus connection owning `/One` and `/Two`, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros twoPathsFromOneOwnerWedgePopulation -v1
```

Observed after 500 ms with a 100 ms fetch timeout: registry count `1`, `initialPopulationComplete=false`. Expected: count `2`, completion true.

#### P1-4 — The monitor drops the valid root object path `/`

- Contract: ADR-0032 and `docs/wiki/shell/status-tray.md:20-22` explicitly accept `/`; the watcher also accepts and advertises it.
- Cause: `StatusNotifierItemMonitor::parseServiceId` rejects a slash at the last character (`separator >= serviceId.size() - 1`) at `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp:463-473`. For the wire ID `:1.N/`, that slash is the complete valid object path.
- Reproduction (private bus, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros rootObjectPathIsDroppedByMonitor -v1
```

Observed: registry count `0` and `initialPopulationComplete=true`. Expected: the valid root-path item yields count `1`.

#### P1-5 — Wrong-typed string properties are coerced and accepted instead of ignored/rejected

- Contract: `status_notifier_item_client.h:59-64` and `docs/wiki/shell/status-tray.md:151-158` say unexpected types are ignored; the review contract requires a wrong-typed snapshot to fail closed wholesale.
- Cause: `src/shell/status_notifier/item_client/src/status_notifier_item_client.cpp:217-264` uses generic `QVariant::toString()`, `toBool()`, and `toUInt()` without exact meta-type checks. A wrong-typed required `Id` therefore becomes valid text rather than being ignored to its invalid default.
- Reproduction (private bus, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros wrongTypedSnapshotIsAccepted -v1
```

Observed for integer `Id=123` and `Title=456`: `validation.accepted=true`, identity `"123"`, title `"456"`. Expected: rejected snapshot.

#### P1-6 — The only timeout row is vacuous and the result cannot distinguish timeout from immediate error

- Contract: the review requires a bounded pending call with a typed timeout; `docs/wiki/shell/status-tray.md:177-187,231-235` claims fetch timeout coverage.
- Cause/API: `ItemDescriptorFetch` at `status_notifier_item_client.h:32-49` exposes only `replyReceived=false` for timeout, D-Bus error, or a fenced reply. `tst_status_notifier_item_client.cpp:346-373` labels a nonexistent unique name as an unanswered item, but the daemon immediately returns a no-owner error; the row asserts only the shared boolean.
- Reproduction (registered row itself, exit 0):

```sh
TIMEFORMAT='ELAPSED=%R'; time env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/dev/tests/shell/status_notifier/qindaqt_status_notifier_item_client_tests reportsUnansweredFetchesAsNotReceived -v1
```

Observed: the alleged 200 ms timeout row completed in `ELAPSED=0.060` seconds and passed. Expected proof: a live owner/object that withholds the reply until the configured deadline, plus a typed timeout result distinct from an immediate D-Bus error. This is the only claimed timeout proof, so its vacuity is P1 under the review severity rule.

#### P1-7 — Hostile `index.theme` directory names escape the injected icon root

- Contract: `status_notifier_icon_locator.h:22-27` promises that resolved paths remain under the canonical injected root; module boundaries make this confinement part of the public boundary.
- Cause: `src/shell/status_notifier/icon/src/status_notifier_icon_locator.cpp:111-151` accepts declared directory names without path validation, and `:160-184` checks only existence, never canonical containment.
- Reproduction (all files under the assigned build root, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros themeIndexCanEscapeInjectedRoot -v1
```

Observed: injected root canonicalized to `.../runtime/injected`, while `Directories=../outside` located `.../runtime/outside/escaped.png`. Expected: empty result/refusal because the canonical candidate is outside the injected root.

#### P1-8 — Theme-decoded and fallback images are not dimension-bounded

- Contract: `docs/wiki/shell/status-tray.md:188-197` and `status_notifier_icon_renderer.h:18-29` promise bounded `QImage` values; the shared maximum dimension is 512.
- Cause: `decodeThemeIcon` at `src/shell/status_notifier/icon/src/status_notifier_icon_renderer.cpp:88-106` caps compressed file bytes but never validates decoded dimensions. `fallbackIcon` at `:75-85` allocates the caller's positive size directly.
- Reproductions (build-root fixture only, each exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros themedImageSizeIsNotBounded -v1
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros placeholderSizeIsNotBounded -v1
```

Observed: a small encoded PNG decoded and returned as `QSize(513, 1)`; the fallback returned `QSize(513, 513)`. Expected: both dimensions bounded to at most 512 or a bounded fallback.

### P2

#### P2-1 — `start()` is not idempotent in `NameOwnedElsewhere`

- Contract: `status_notifier_watcher_service.h:62-67` says a foreign owner is not an error and returns true, while `:49-52` and `docs/wiki/shell/status-tray.md:148` say `start()` is idempotent.
- Cause: `src/shell/status_notifier/watcher/src/status_notifier_watcher_service.cpp:65-69` returns `m_state == Active` on subsequent calls, changing a successful degraded start from true to false.
- Reproduction (private bus, exit 1):

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros degradedStartIsNotIdempotent -v1
```

Observed: initial degraded start `true`, repeated start `false`, state `NameOwnedElsewhere`. Expected: the repeated call has the same successful result. Workaround: call once or inspect state directly.

### P3

#### P3-1 — Foundation comment still says the protocol constants are for a future adapter

`src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_limits.h:44-46` says the constants are for “future adapter use only” and adapters are “later milestones”, while this exact candidate adds and uses the watcher/item-client adapters. The foundation itself remains Qt Core-only, so this is stale precision rather than a dependency violation.

## Exact build and registered-test evidence

All commands below ran from the review worktree unless another directory is named.

### Configure

Both exited 0 (the generated build-tree output contained the repository's existing Qt library search-path warnings):

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/dev -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

### Focused builds

Both exited 0 (`ninja: no work to do` after exact reconfiguration):

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/dev --parallel 3 --target qindaqt_status_notifier qindaqt_status_notifier_watcher qindaqt_status_notifier_item_client qindaqt_status_notifier_icon qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests qindaqt_status_notifier_values_tests
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/release --parallel 3 --target qindaqt_status_notifier qindaqt_status_notifier_watcher qindaqt_status_notifier_item_client qindaqt_status_notifier_icon qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests qindaqt_status_notifier_values_tests
```

### Discovery and execution

These discovery commands each exited 0 and listed exactly seven rows (tests 151-157):

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/dev -N -R 'status-notifier|tray'
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/release -N -R 'status-notifier|tray'
```

Both selectors exited 0, 7/7 rows passed, 0 failed, 0 skipped:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/dev -R '^qindaqt\.status-notifier-' --output-on-failure --no-tests=error
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/release -R '^qindaqt\.status-notifier-' --output-on-failure --no-tests=error
```

Verbose reruns with the same environment and `-V --no-tests=error` also exited 0. Per-profile QTest totals were: values 17, registry 25, presentation 9, watcher 11, item-client 12, monitor 8, icon 11 = **93 passed, 0 failed, 0 skipped**.

## Static gates

- `./tools/validate-docs` — exit 0; 116 Markdown documents and `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/site` — exit 0.
- `./tools/check-source-shape` — exit 0; checked 1,778 files. It printed two unrelated existing warnings: `tests/compositor/CMakeLists.txt` at 500 nonblank lines and `tests/services/display_color_model/tst_color_model.cpp` at 539.
- `git diff --check` — exit 0.
- `git diff --check ce9228d..9d1a30b` — exit 0.
- `git diff --name-only ce9228d..9d1a30b -- '*.json'` — exit 0 with empty output; no changed JSON exists, so `python3 -m json.tool` is not applicable.

## Scratch reproduction harness and additional controls

The harness was created only under the assigned build root and links the exact Debug candidate libraries.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 3
```

Both exited 0 from `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros`.

An exploratory standard `org.freedesktop.DBus.Properties.Get` call against the watcher passed (exit 0), so the source's hyphenated adaptor annotation/comment was not promoted to a functional finding:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./build/tray_candidate_repros standardPropertiesInterfaceFails -v1
```

The other harness selectors and their exact exit/results are recorded beside P1-1 through P1-5, P1-7, P1-8, and P2-1. Every D-Bus reproduction spawned its own private bus. The icon reproductions read/write only beneath the assigned build root.

## Final tree integrity

```text
git rev-parse HEAD        = 9d1a30b02729c0aba6deba9a43fa67418d283e76
git rev-parse HEAD^{tree} = cea5680be27dd9c31c9afd8482fee8ef02c55fe4
git rev-parse HEAD^       = b77373c6581f3841e878f1d542ceecfbf762e2eb
git rev-parse ce9228d     = ce9228d9694622d503d92a38d01986f8f124f188
git status --porcelain    = empty
```

VERDICT REJECT P0/P1/P2/P3=0/8/1/1
