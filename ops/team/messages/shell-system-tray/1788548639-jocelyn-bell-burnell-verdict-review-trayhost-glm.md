# Tray S3 production shell hosting — exact-candidate review verdict

- Reviewer persona: Jocelyn Bell Burnell, independent shell reviewer
- Provider/model: Z.AI GLM (`zai-coding-plan/glm-5.3`)
- Candidate SHA: `a257d7334c281ba55608854b9c88de42077916e2` (`worker/tray-hosting`, "Host the Status Notifier tray applet in the production shell")
- Candidate tree SHA: `321c1e1ea254856ebddca1c1221e40d67a897699`
- Parent SHA: `5157a1e0ce2c22b2637869fc1d28b2680852d24b`
- Base SHA: `5157a1e0ce2c22b2637869fc1d28b2680852d24b`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-hosting-glm-review` (detached at the candidate; `git status --porcelain` empty before, during, and after the review; `git rev-parse HEAD` re-verified at the end)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-trayhost-glm`
- Scope reviewed: the exact base-to-candidate diff (42 files, +1245/−110), read-first materials (`AGENTS.md`, module-boundaries, coding-practices, status-tray, applet-runtime, testing-harness sections touched, implementer handoff `ops/team/messages/shell-system-tray/1788544336-williamina-fleming-handoff.md`).

## Findings ledger

### P0 — none

### P1 — none

### P2 — none

### P3 — 1 finding

**P3-1: No direct offscreen rendering row for the tray's vertical strip.**
`src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml:187` defines a dedicated `verticalStrip` (Column) selected by `root.vertical`; no automated row anywhere in `tests/` instantiates the module (or any compiled applet module) with `vertical: true` — `grep -rn "vertical: true" tests/` matches nothing repo-wide. The wiki claim "in horizontal rows and vertical columns" (`docs/wiki/shell/status-tray.md:16,35-37`) is structurally true and contractually validated — the manifest declares both orientations (`data/applets/status-notifier.json` placements.orientations), the resolver row proves `ready` placement on `nextstep-inspired.json`'s vertical right panel (zone/entryPoint/grants asserted for all ten profiles in `tests/applet_runtime/tst_applet_instance_resolver.cpp:353`), `PanelAppletColumn.qml:37` sets `vertical: true`, and the strips share the same delegate — but the direct offscreen execution evidence covers only the horizontal strip (`tst_StatusNotifierProductionPanelKeyboard.qml`). This matches the repository-wide evidence baseline for every compiled applet (clipboard/audio/bluetooth/power/global-menu alike), so it is a precision note, not a defect: a vertical-strip offscreen row would strengthen the hosting claim. Nonblocking.

## Composition review (lane question 1) — findings: no defects

- `src/shell/runtime/statusnotifierappletcomposition.cpp:24-58` `statusNotifierGrants()` fails closed on every gate (missing manifest, non-builtin host, unregistered entry point, policy evaluation failure → both capabilities withheld) and mirrors the accepted `clipboardappletcomposition.cpp` pattern exactly; `entryPoint.value` is a plain `QString` member (`src/applets/include/qindaqt/applets/manifest_types.h:51-56`), so the no-optional hazard I probed does not exist.
- Watcher and adapter are constructed but started only under `status-items.read` (`statusnotifierappletcomposition.cpp:88-101`); the read-denial negative control (`tests/.../tst_status_notifier_applet_composition_private_bus.cpp:170-228`, ephemeral private bus, scratch deny-policy under the build tree) proves observation is withheld, phase is `unavailable` with reason `status-items-read-not-granted`, and acknowledgement degrades to a no-op. No silent QSKIPs: direct run reports `4 passed, 0 failed, 0 skipped`.
- Only the `StatusNotifierAppletController` facade crosses into QML (`runtimepanelwindowfactory.cpp:135-138` initial property, inherited via the bounded depth-4 parent walk in `BuiltinAppletContent.qml:113-128` — proven end-to-end by the production-panel row dispatching the exact generation-fenced key through a controller fake set on the row). No connection, registry pointer, wire payload, or bus endpoint reaches QML; the boundary gate now also polices the composition pair (no own bus connections, no QDBusInterface/PendingCall/ServiceWatcher, no QProcess, no compositor/platform/services reach, no registry/item-client/icon internals) with its own item-client-bypass poison case, and it passed standalone in its documented no-configure form.
- Lifetime/threading: everything lives on the GUI thread; the AGENT-CONTRACT destruction order (controller → adapter → watcher) matches both the member order and the explicit idempotent `stop()` sequence in the destructor; `ShellRuntimeApplication::resetRuntime()` (shellruntimeapplication.cpp:464-491) resets the window factory (all QML consumers) before resetting `m_statusNotifierApplet`, and `initializeServiceAppletCompositions()` (line 300) runs before the factory is constructed (line ~375). A foreign-owned watcher name logs a warning and fails into truthful degraded state rather than aborting shell startup.
- Exactly-once dispatch, generation fencing, malformed-replacement degradation with last-known-good retention, acknowledgement recovery, and owner-loss clearing are all exercised against the real composition over an ephemeral private bus (including a genuinely malformed title containing a literal C0 `0x01` byte — verified in the raw file, the diff rendering hides it).

## Hosted-QML offscreen review (lane question 2) — findings: no new defects

- `qindaqt.status-notifier-applet-production-panel-keyboard-offscreen` runs the real source dispatcher (`PanelAppletRow` → `AppletChip` → `BuiltinAppletContent` via relative source import) hosting the compiled module under `QT_FATAL_WARNINGS=1` with `DBUS_SESSION_BUS_ADDRESS`/`DISPLAY`/`WAYLAND_DISPLAY` unset: Tab reaches the delegate, Return dispatches exactly one `activateItem(":1.42", "/StatusNotifierItem", 3)`, `Accessible.role/name` asserted, `popupType === Popup.Window` asserted, Escape closes with zero dispatches. Not source-only or tautological.
- The S2 keyboard row's split into two single-open functions (Shift+F10+dispatch, Menu+Escape) preserves the original coverage and adds the `Popup.Window` assertion; the offscreen-backend limitation justifying the split is documented at `tests/.../qml/tst_StatusNotifierAppletKeyboard.qml:222-225`. The harness now pins deterministic fonts in `applicationAvailable()` (clipboard precedent), removing an ordering dependency on an unrelated Controls test.
- Overflow truth (bounded at 24 with counted `overflowText` chip) is covered in the compiled-module row and the pure model row. See P3-1 for the vertical-strip note.

## Packaging and relocation review (lane question 3) — findings: no defects

- `qindaqt_install_status_notifier_applet_runtime()` (`src/shell/StatusNotifierRuntimeInstall.cmake:49-81`) exactly mirrors the accepted clipboard helper (same destinations, same `${CMAKE_BINARY_DIR}/qml/...` qmldir/qmltypes paths that match the module's `OUTPUT_DIRECTORY` at `src/shell/status_notifier/applet/CMakeLists.txt:49-50`) and is staged for every shell-carrying component; the closure script now requires the StatusNotifier qmldir and both QML files from every staged shell component. No install conflict materialized: `qindaqt.shell-runtime-component-closure` passed in both profiles (9.43 s Debug / 7.66 s Release, installing each component alone and launching the staged shell with cleared ambient variables).
- The DesktopVirtual staging moved from `tests/session/PanelVisibilityTests.cmake` into the shared `DesktopVirtualAppletModules.cmake` inventory exactly as the base commit's AGENT-NOTE prescribed. For a STATIC module the shared function stages qmldir/qmltypes/QML files (the statically linked plugin lives in the shell binary), which is the same contract the audio/bluetooth/power entries already have; `desktop.virtual.stage-closure` (data-driven from the same inventory) passed 3/3 in both profiles.
- Dispatcher count: nine hosted renderers for ten registered entry points (task-list deferred, legacy `system-tray` staying `implementation-unavailable`); `check_launcher_contract_text.cmake` require-text updated "eight"→"nine" and the row passed.
- Profiles: exactly one additive `status-notifier` line per stock family (all ten), each zone-matched to its notification-center slot (including the `bare: true` macOS variant), no existing entry touched or reordered — verified line-by-line in the diff and enforced by the new resolver row for all ten profiles with ready/entryPoint/grants/zone assertions.
- Installed source poison: `qindaqt.status-notifier-applet-runtime-installed-package` stages `StatusNotifierAppletRuntime` alone, requires shell + manifest/profile/theme/policy + complete generated module + static archives + shared QML backings, then runs the staged shell `--list` under poisoned `XDG_DATA_*`/`QINDAQT_*` sources with `LD_LIBRARY_PATH`/bus/display cleared and asserts the tray resolves. Passed in both profiles.

## Base-defect repair claims — independently verified

1. `run_shell_component_closure.cmake` `\;` defect: mechanically reproduced with scratch CMake probes under `<ROOT>/scratch` — inside a CMake quoted argument `"${p1}\;${p2}"` the `\;` survives as a literal backslash-semicolon in ONE list element (so ctest's spawned `-D` argument keeps it a single argv), which makes the base script's `foreach(... IN LISTS ...) file(READ)` read a nonexistent `\;`-joined path → fatal. The candidate's `string(REPLACE "\\;" ";")` normalization is the correct minimal repair, and the passing closure row now demonstrably reads both install modules and enforces the StatusNotifier module per component.
2. `entryPoints()` ordering: `BuiltinAppletRegistry::entryPoints()` sorts (`src/applet_runtime/src/builtin_applet_registry.cpp:38-43`); the base expectation listed `task-list` before `status-notifier` (verified via `git show 5157a1e0:tests/applet_runtime/tst_applet_instance_resolver.cpp`) — unsorted, so the base row was red; the candidate's reordered expectation is sorted truth.

## Environment note (not a candidate defect)

The host `/tmp` tmpfs (16G) is 100% full from unrelated shared-tenant content; the first parallel build attempt died at ~step 1200/4057 in both trees with `fatal error: error writing to /tmp/...: No space left on device`. Resumed sequentially with `TMPDIR=<ROOT>/tmp` on the root filesystem (383G free): both trees then built clean. Nothing in the candidate depends on /tmp.

## Commands and results

Environment for every ctest run: `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`.

Configure (exact recipe, system-KWin initial cache, `<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/review-trayhost-glm`):

- Debug: `cmake -S <worktree> -B <ROOT>/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` — exit 0.
- Release: same with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release` — exit 0.

Builds: full default target set (`cmake --build <ROOT>/{debug,release} --parallel 3`), required because the closure/installed-package rows `cmake --install` the whole build root per component (see environment note for the one interrupted attempt): Debug exit 0, Release exit 0, zero warnings under the `-Werror` strict set.

Debug tests (`<ROOT>/debug`):

- `ctest -R '^qindaqt\.(status-notifier-|applet|shell-runtime-|launcher-panel-dispatcher|notification-center-applet-offscreen)' --output-on-failure --no-tests=error` — **29/29 passed, exit 0**. Key rows: `status-notifier-applet-composition-private-bus` (also run directly: `4 passed, 0 failed, 0 skipped`), `status-notifier-applet-production-panel-keyboard-offscreen`, `status-notifier-applet-runtime-installed-package` (4.20 s), `status-notifier-applet-installed-package` (6.75 s), `status-notifier-applet-boundary-policy`, `qindaqt.shell-runtime-component-closure` (9.43 s).
- `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' --output-on-failure --no-tests=error` — **3/3 passed, exit 0**.
- `ctest -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error` — **20/20 passed, exit 0**.
- `ctest -R 'global-menu-production-panel-keyboard-qml-offscreen|shell-capture-matrix' --output-on-failure --no-tests=error` — **2/2 passed, exit 0**.

Release tests (`<ROOT>/release`): identical selectors — **29/29** (exit 0), **3/3** (exit 0), **20/20** (exit 0), **2/2** (exit 0). Counts match the implementer's evidence exactly.

Static gates (from the worktree root):

- `./tools/validate-docs` — exit 0 (142 documents).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0.
- `git diff --check` — exit 0.
- `python3 -m json.tool` on all ten changed `data/profiles/*.json` — all OK.

Boundary gate standalone (documented no-configure form, scratch poison dir under `<ROOT>`):

```
cmake -DQINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR=<repo>/src/shell/status_notifier/applet \
  -DQINDAQT_STATUS_NOTIFIER_COMPOSITION_SOURCE_DIR=<repo>/src/shell/runtime \
  -DQINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY=<ROOT>/scratch/poison-probe \
  -P tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake
```
— exit 0: "Validated 11 Status Notifier Applet source/QML files, the composition pair, and poison probe rejection".

Constraints honored: no `tests/session` nested-compositor rows, no host D-Bus session/system services, no hardware/uinput, no network, no product-path edits (scratch reproductions only under `<ROOT>/scratch`); worktree clean before and after.

## Verdict

The candidate delivers the claimed outcome: the production shell hosts the Status Notifier tray applet behind the documented seams with fail-closed capability gating, generation-fenced exactly-once dispatch, truthful degradation/acknowledgement, owner-loss clearing, keyboard-capable popup-window context menus through the real dispatcher, complete shell-component packaging with relocation and source-poison proofs, and purely additive profile placement. All executed evidence passes in Debug and Release; documentation is accurate to the code; both base defects are real and correctly repaired. One nonblocking precision note (P3-1).

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/1
