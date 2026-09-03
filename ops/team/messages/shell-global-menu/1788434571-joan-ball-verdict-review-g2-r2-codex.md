# Joan Ball — independent shell repair recheck

- Persona: **Joan Ball**, independent shell reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `24240e904a4e117c8b48698272465e37c1c25e80`
- Tree SHA: `a82e057af60532b1710ab271d89b52fd7318bf37`
- Parent SHA: `4e40aa67217bdf3850a7f899ca14fdf07f9e3ad5`
- Base SHA: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Rejected ancestor: `f7a49c5e916f210665e72e6fccdfa134b0ed43c3`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex`
- Reviewed repair diff: `git diff f7a49c5..24240e9` — 14 files, 730 insertions, 17 deletions
- Whole candidate diff: `git diff d9aec19..24240e9` — 54 files, 2,786 insertions, 152 deletions

## Findings ledger

### P0

None.

### P1

None. Prior P1-01 is closed.

The popup is now explicitly an independent window at
`src/shell/global_menu/applet/qml/GlobalMenuPopup.qml:97-105`, while the panel
correctly retains `Qt.WindowDoesNotAcceptFocus` and
`KeyboardInteractivityNone`. The registered production-row test at
`tests/shell/global_menu/qml/tst_GlobalMenuProductionPanelKeyboard.qml:54-116`
uses the real `PanelAppletRow` -> `AppletChip` -> `BuiltinAppletContent` route,
tabs to the real menu entry, opens and traverses the recursive popup with keys,
activates `alpha` exactly once, and observes closure. Its CTest registration at
`tests/shell/global_menu/qml/CMakeLists.txt:67-80` sets
`QT_FATAL_WARNINGS=1` and the test requires `Popup.Window` before continuing.
The adjacent submenu row covers Escape/Left closure, zero activation, and
focus-loss closure.

The new test passes on the exact candidate in Debug and Release and fails
against an explicit scratch build of `f7a49c5` at its `Popup.Window` assertion:
actual popup type `0` (`Popup.Item`), expected `1` (`Popup.Window`). The panel
dispatcher QML and `GlobalMenuApplet.qml` are byte-identical across the repair;
only `GlobalMenuPopup.qml` changed, so the ancestor run isolates the repair.

### P2

None. Prior P2-01 is closed.

`qindaqt_global_menu_hostile_provider` is a separate `QProcess` on the private
`dbus-run-session` bus. It registers window 77 from its own bus connection and
exports both the fake dbusmenu object and an event-count probe. The test verifies
the child PID is positive and differs from the test/shell PID, then exercises:

- a compositor PID that disagrees with the registrar peer's daemon-derived PID;
- an invalidation followed by a lower identity revision whose PID would
  otherwise match the child; and
- unavailable/empty facade state plus zero provider `Event` calls after an
  attempted activation in both variants.

The control at
`tests/shell/global_menu/runtime_composition/tst_global_menu_runtime_composition.cpp:196-253`
uses the same registrar/coordinator path with matching PID, reaches available,
and observes exactly one event. Thus the hostile PID assertions at lines
255-292 are not vacuous: removing the equality rejection in
`src/shell/global_menu/ownership/src/provider_authenticator.cpp:106-118` makes
that same child eligible for the positive path, contradicting the required
unavailable/zero-event assertions. The stale variant independently contradicts
acceptance of a regressed identity revision at lines 294-335.

### P3

#### P3-01 — Canonical overview pages still call Global Menu live wiring future

The owning Global Menu page correctly says G2 is composed in `qindaqt-shell`,
but two canonical overview/current-scope descriptions were not advanced with
the implementation:

- `docs/wiki/index.md:99-102` says transport and live wiring remain future; and
- `docs/wiki/shell/panel-surfaces.md:94-110` says only clock and notification
  center are live and Global Menu remains in a future milestone.

Reproduction:

```console
$ rg -n 'transport and live wiring remain future|Global menu, live' docs/wiki/index.md docs/wiki/shell/panel-surfaces.md
docs/wiki/shell/panel-surfaces.md:108:producers and hide animation also remain acceptance work. Global menu, live
docs/wiki/index.md:101:  fail-closed export foundation; transport and live wiring remain future
$ git diff --name-only d9aec19..24240e9 -- docs/wiki/index.md docs/wiki/shell/panel-surfaces.md docs/wiki/shell/global-menu.md
docs/wiki/shell/global-menu.md
```

Observed: the canonical navigation summary and production-panel current-scope
page contradict the delivered G2 state. Expected: those summaries distinguish
the now-live bounded Global Menu composition from the still-future foreign
toolkit and installed nested-session work. This is nonblocking documentation
precision; it does not invalidate the executed product behavior.

## Review-question evidence

1. **Keyboard path:** `Popup.Window` supplies the independent keyboard-capable
   surface without widening layer-shell interactivity or changing
   `RuntimePanel.qml`/`src/shell_surface/**`. The real panel-row test passes
   under fatal warnings in both build types, and the Escape/focus-loss adjacent
   cases pass. Because the repair did not change the panel or shell-surface
   focus policy, the hostile client-focus-return condition was not triggered.
2. **Distinct-process ownership:** the private-bus row launches a real child,
   verifies the distinct PID, rejects both PID mismatch and stale revision,
   keeps the facade unavailable/empty, and records zero remote events. Its
   matching-PID control proves the same path can publish and activate.
3. **Regressions:** all 21 Global Menu rows and all 10 requested adjacent
   applet/runtime rows pass independently in Debug and Release under the
   prescribed isolated outer bus environment. Component closure is included.

## Commands and results

### Immutable candidate checks

```console
$ git rev-parse HEAD
24240e904a4e117c8b48698272465e37c1c25e80
$ git rev-parse HEAD^{tree}
a82e057af60532b1710ab271d89b52fd7318bf37
$ git rev-parse HEAD^
4e40aa67217bdf3850a7f899ca14fdf07f9e3ad5
$ git merge-base f7a49c5 24240e9
f7a49c5e916f210665e72e6fccdfa134b0ed43c3
$ git status --porcelain
$ echo $?
0
```

The main worktree was clean before review and after all review work. No product
path was edited, committed, amended, or rebased. Scratch ancestor source/build
artifacts are confined beneath the assigned build root.

### Configure

The prescribed command was run with `<ROOT>` equal to
`/home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex`:

```console
$ cmake -S . -B <ROOT>/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
exit 0
$ cmake -S . -B <ROOT>/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
exit 0
```

Both configurations completed. CMake emitted the repository's existing
dependency-root runtime-path warnings.

### Focused builds

The following exact target vector was built once in Debug and once in Release:

```console
$ cmake --build <B> --parallel 3 --target qindaqt-shell qindaqt-shell-preview qindaqt_global_menu_qml qindaqt_global_menu_qmlplugin qindaqt_global_menu_protocol_tests qindaqt_global_menu_ownership_tests qindaqt_global_menu_lineage_tests qindaqt_global_menu_exporter_tests qindaqt_global_menu_qt_widgets_adapter_tests qindaqt_global_menu_applet_access_tests qindaqt_global_menu_composition_tests qindaqt_global_menu_registrar_tests qindaqt_global_menu_dbusmenu_decoder_tests qindaqt_global_menu_dbusmenu_client_tests qindaqt_global_menu_transport_composition_tests qindaqt_global_menu_hostile_provider qindaqt_global_menu_runtime_composition_tests qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests qindaqt_shell_runtime_options_tests qindaqt_shell_launcher_qmlplugin qindaqt_launcher_installed_probe qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
```

- Debug: exit 0, 28/28 incremental actions.
- Release: exit 0, 28/28 incremental actions.

### Repair-specific tests

```console
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> -R '^(qindaqt\.global-menu-production-panel-keyboard-qml-offscreen|qindaqt\.global-menu-runtime-composition-private-bus)$' --output-on-failure --no-tests=error
```

- Debug: exit 0, 2/2 passed.
- Release: exit 0, 2/2 passed.

### Required Global Menu and adjacent tests

```console
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> -R '^qindaqt\.global-menu-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 21/21 passed in 3.28 seconds.
- Release: exit 0, 21/21 passed in 3.14 seconds.

```console
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> -R '^(qindaqt\.applet-(manifest|catalog|runtime-resolution)$|qindaqt\.shell-runtime-(options|catalog|component-closure)$|qindaqt\.(audio|bluetooth|power)-applet-installed-package$|qindaqt\.launcher-installed-package$)' --output-on-failure --no-tests=error
```

- Debug: exit 0, 10/10 passed in 16.98 seconds.
- Release: exit 0, 10/10 passed in 8.58 seconds.

### Exact ancestor keyboard negative control

```console
$ git worktree add --detach <ROOT>/ancestor-src f7a49c5e916f210665e72e6fccdfa134b0ed43c3
HEAD is now at f7a49c5e Compose Global Menu in the production shell
$ cmake -S <ROOT>/ancestor-src -B <ROOT>/ancestor-debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
exit 0
$ cmake --build <ROOT>/ancestor-debug --parallel 3 --target qindaqt_global_menu_qml qindaqt_global_menu_qmlplugin
exit 0, 26/26 actions
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 /usr/lib64/qt6/bin/qmltestrunner -import <ROOT>/ancestor-src/tests/shell/qml/imports -import <ROOT>/ancestor-debug/qml -input /home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition-codex-review/tests/shell/global_menu/qml/tst_GlobalMenuProductionPanelKeyboard.qml
FAIL: actual popup type 0, expected 1, line 104
Totals: 2 passed, 1 failed
exit 1
```

This intentionally failing run is the candidate's new test body against the
exact ancestor module. `src/shell/qml/**` and `GlobalMenuApplet.qml` have no
repair diff; only the popup module differs.

### Static gates

```console
$ ./tools/validate-docs
Validated 127 Markdown documents and mkdocs.yml navigation.
exit 0
$ /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site
Documentation built in 1.47 seconds.
exit 0
$ ./tools/check-source-shape
Checked 2053 source files; skipped 0 allowlisted files.
exit 0
$ git diff --check && git diff d9aec19..24240e9 --check && git diff f7a49c5..24240e9 --check
exit 0
$ python3 -m json.tool data/applet-policy/default.json >/dev/null
exit 0
```

The source-shape gate repeated three pre-existing threshold warnings in
`tests/compositor/CMakeLists.txt` (500),
`tests/services/display_color_model/tst_color_model.cpp` (539), and
`tests/shell/audio_applet/tst_audio_applet_controller.cpp` (563). The candidate
does not change them.

No `tests/session` or nested-compositor row, host system/session service,
hardware, uinput, or network test was run.

## Verdict

The two blocking findings are repaired with executable, non-vacuous evidence.
The remaining P3 is a bounded canonical-documentation precision issue and does
not block integration.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/1
