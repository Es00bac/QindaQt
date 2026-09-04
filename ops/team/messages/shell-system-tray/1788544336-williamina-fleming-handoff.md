# Williamina Fleming — handoff: Tray S3 production shell hosting of the status-notifier applet

- Candidate commit: `a257d7334c281ba55608854b9c88de42077916e2`
- Candidate tree: `321c1e1ea254856ebddca1c1221e40d67a897699`
- Exact base: `5157a1e0ce2c22b2637869fc1d28b2680852d24b` (`main` tip at claim)
- Branch: `worker/tray-hosting`; worktree `/home/cabewse/work_SPaC3/container-wm-workers/tray-hosting`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/tray-hosting` (`debug`, `release`)

## Changed paths (sorted, candidate vs base)

```
data/profiles/gnome-inspired.json
data/profiles/macos-inspired.json
data/profiles/mate-inspired.json
data/profiles/minimal.json
data/profiles/nextstep-inspired.json
data/profiles/qindaqt.json
data/profiles/unity-inspired.json
data/profiles/windows-classic.json
data/profiles/windows-modern.json
data/profiles/xfce-inspired.json
docs/wiki/architecture/module-boundaries.md
docs/wiki/development/testing-harness.md
docs/wiki/shell/applet-runtime.md
docs/wiki/shell/status-tray.md
src/shell/CMakeLists.txt
src/shell/StatusNotifierRuntimeInstall.cmake
src/shell/qml/BuiltinAppletContent.qml
src/shell/qml/PanelAppletColumn.qml
src/shell/qml/PanelAppletRow.qml
src/shell/qml/PanelContent.qml
src/shell/qml/RuntimePanel.qml
src/shell/runtime/runtimepanelwindowfactory.cpp
src/shell/runtime/runtimepanelwindowfactory.h
src/shell/runtime/shellruntimeapplication.cpp
src/shell/runtime/shellruntimeapplication.h
src/shell/runtime/statusnotifierappletcomposition.cpp
src/shell/runtime/statusnotifierappletcomposition.h
src/shell/status_notifier/applet/qml/StatusNotifierItemDelegate.qml
tests/applet_runtime/tst_applet_instance_resolver.cpp
tests/applets/tst_catalog.cpp
tests/session/DesktopVirtualAppletModules.cmake
tests/session/PanelVisibilityTests.cmake
tests/shell/audio_applet/run_shell_component_closure.cmake
tests/shell/clipboard_applet/CMakeLists.txt
tests/shell/launcher/check_launcher_contract_text.cmake
tests/shell/status_notifier/applet/CMakeLists.txt
tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake
tests/shell/status_notifier/applet/qml/tst_StatusNotifierAppletKeyboard.qml
tests/shell/status_notifier/applet/qml/tst_StatusNotifierProductionPanelKeyboard.qml
tests/shell/status_notifier/applet/qml_interactive_main.cpp
tests/shell/status_notifier/applet/run_installed_status_notifier_runtime.cmake
tests/shell/status_notifier/applet/tst_status_notifier_applet_composition_private_bus.cpp
```

## What landed

- `StatusNotifierAppletComposition` (`src/shell/runtime/statusnotifierappletcomposition.{h,cpp}`):
  owns the S1 `StatusNotifierWatcherService` and the S2
  `StatusNotifierMonitorAdapter` (exact-owner registry, item monitor, icon
  renderer) on the injected session bus; evaluates audited manifest/policy
  grants fail-closed; starts watcher+adapter only with `status-items.read`;
  exposes only the controller facade; the acknowledgement transition routes
  through the composed adapter seam. Nothing else gains bus authority.
- Presentation: `BuiltinAppletContent.qml` hosts the compiled
  `QindaQt.Shell.StatusNotifier` module in horizontal rows and vertical
  columns plus the deterministic preview; panel rows/column/content/panel
  carry the one facade property; the item delegate's context menu is now a
  keyboard-capable `Popup.Window` with Escape closing. The dispatcher renders
  all nine hosted built-ins (`applet-runtime.md` and the launcher contract
  count updated).
- Packaging: `qindaqt_install_status_notifier_applet_runtime()` in
  `StatusNotifierRuntimeInstall.cmake` stages the module into every
  shell-carrying component; DesktopVirtual staging moved from
  `PanelVisibilityTests.cmake` into the shared
  `DesktopVirtualAppletModules.cmake` inventory as S2's AGENT-NOTE prescribed;
  the component-closure script additionally requires the StatusNotifier module
  from every staged shell component.
- Profiles: one `status-notifier` entry per stock family (all ten), zone
  matched to the notification-center slot; every existing entry untouched.
- Tests: new `qindaqt.status-notifier-applet-composition-private-bus` (real
  production composition over an ephemeral private bus: empty→ready
  population, exactly-one wire Activate, malformed-replacement degradation,
  acknowledgement recovery, owner-loss clearing, plus the explicit read-denial
  negative control), new
  `qindaqt.status-notifier-applet-production-panel-keyboard-offscreen` (real
  source dispatcher under `QT_FATAL_WARNINGS=1` with host display/bus unset:
  Tab traversal, Return activation with the exact generation-fenced key,
  accessible role/name, `popupType == Popup.Window`, Escape closure), new
  `qindaqt.status-notifier-applet-runtime-installed-package` (source-poisoned
  shell-component stage), boundary gate extended with the composition pair and
  an item-client-bypass poison case, resolver placement test for all ten stock
  profiles, catalog capability assertion.

## Evidence (all commands actually run; exit 0 unless noted)

Debug (`/home/cabewse/work_SPaC3/builds/qindaqt/tray-hosting/debug`):

- Focused builds: `cmake --build … --target qindaqt-shell qindaqt-shell-preview`
  and every tray/applet test executable listed in the lane — exit 0 under
  strict warnings.
- `ctest -R '^qindaqt\.(status-notifier-|applet|shell-runtime-|launcher-panel-dispatcher|notification-center-applet-offscreen)' --output-on-failure --no-tests=error`
  under `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:
  **29/29 passed**.
- `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' --no-tests=error`:
  **3/3 passed**.
- Adjacent consumers: `ctest -R '^qindaqt\.clipboard-applet-'` **20/20**;
  `qindaqt.global-menu-production-panel-keyboard-qml-offscreen` **1/1**;
  `qindaqt.shell-capture-matrix` **1/1**.
- Nested rows, run once each, serially, with
  `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop` after
  `pgrep -f 'socket qindaqt-parent-wayland'` showed the lane free:
  `desktop.virtual.boot.1080p` **Passed** (3.11 s),
  `desktop.virtual.panel-visibility.single-1080p` **Passed** (43.56 s),
  `desktop.virtual.panel-visibility.single-wuxga` **Passed** (49.51 s).
  No compositor/shell survivors afterward (`pgrep` clean). The host
  `kwin_wayland` on `wayland-0` was never touched.

Release (`/home/cabewse/work_SPaC3/builds/qindaqt/tray-hosting/release`):

- Same target set built clean under strict warnings (954 steps, exit 0).
- Focused selector: **29/29 passed**; desktop.virtual trio **3/3**;
  clipboard **20/20**; global-menu production row + shell-capture matrix
  **2/2**.

Static gates (from the worktree root):

- `./tools/validate-docs` — exit 0 (142 documents).
- `mkdocs build --strict --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0.
- `git diff --check` — exit 0.
- `python3 -m json.tool` on every changed JSON (all ten profiles) — exit 0.

## Base-tree defects repaired in this candidate (both fail on `5157a1e0`)

1. `qindaqt.shell-runtime-component-closure`: the base registration passes
   `QINDAQT_SHELL_INSTALL_CMAKE` with `\;`, which arrives as a literal
   backslash-semicolon in one list element, so the guard never read
   `StatusNotifierRuntimeInstall.cmake`. Repaired script-side in the owned
   `run_shell_component_closure.cmake` by normalizing `\;` → `;` (AGENT-NOTE
   records why).
2. `qindaqt.applet-runtime-resolution`: `BuiltinAppletRegistry::entryPoints()`
   returns sorted entry points; the base expectation listed `task-list` before
   `status-notifier`. Expectation reordered to sorted truth.

## Test repairs within the lane's owned paths

- The S2 QML harness never called `pinDeterministicFonts()`; its rows only
  passed after an unrelated Controls test had written the pinned theme copies
  into the shared build directory. The harness now pins fonts in
  `applicationAvailable()` (clipboard harness precedent).
- The S2 keyboard QML test's reopen-after-close step cannot survive the
  offscreen backend's inability to reactivate a parent window after a
  transient native popup is destroyed; with the production-required
  `Popup.Window` context menu it is split into two single-open test functions
  with equal coverage (open+dispatch via Shift+F10; open+Escape via Menu key).

## Cross-module additive repair outside the lane's owned paths

`tests/shell/clipboard_applet/CMakeLists.txt`: the clipboard production-panel
harness compiles the source dispatcher, which now imports
`QindaQt.Shell.StatusNotifier`; the harness therefore links
`QindaQt::ShellStatusNotifierAppletRuntime` and its plugin (two additive list
entries, plus one prerequisite dependency). Without it the clipboard
production-panel-keyboard row aborts at compile. Clipboard rows are 20/20 in
both profiles with the repair.

## Bounded caveats (deliberately not claimed)

- No live host-session evidence: every bus row runs on ephemeral private
  daemons with scripted fake items; no claim about real third-party items on a
  user session.
- No dbusmenu entry activation (the menu preview stays read-only; that is the
  Global Menu composition lane), no assistive-technology bridge behavior.
- The legacy `system-tray` manifest still resolves `implementation-unavailable`
  by design; windows-classic and xfce-inspired keep their legacy tray entries
  alongside the new hosted applet.
- Nested rows were run once each in Debug per the lane instruction, not in
  Release.
- `desktop.virtual.panel-visibility.*` passed here; main's HANDOFF previously
  recorded them red on an older revision — the current base already carries
  the repair.

## Requested next action

Independent exact review of candidate
`a257d7334c281ba55608854b9c88de42077916e2`, then manager integration.
