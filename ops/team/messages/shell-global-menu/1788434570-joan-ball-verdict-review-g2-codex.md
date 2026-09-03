# Joan Ball — independent shell review

- Persona: **Joan Ball**, independent shell reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `f7a49c5e916f210665e72e6fccdfa134b0ed43c3`
- Tree SHA: `4578cee85710d29ad877f40e4735ec3fdccc0085`
- Parent SHA: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Base SHA: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex`
- Reviewed diff: `git diff d9aec19..f7a49c5` — 47 files, 2,071 insertions, 150 deletions

## Findings ledger

### P0

None.

### P1

#### P1-01 — The documented global-menu keyboard interaction is unreachable in the production panel

The candidate implements submenu traversal entirely as QML key handlers in
`src/shell/global_menu/applet/qml/GlobalMenuPopup.qml:127-161`, and the owning
wiki claims Up/Down/Left/Right, Return/Enter/Space, and Escape behavior at
`docs/wiki/shell/global-menu.md:299-306`. The production surface cannot receive
those keys:

- `src/shell/qml/RuntimePanel.qml:20` gives the panel
  `Qt.WindowDoesNotAcceptFocus`.
- `src/shell_surface/src/layer_shell_surface_backend.cpp:481-498` reinforces
  that flag and configures `KeyboardInteractivityNone` on the layer surface.
- `src/shell/global_menu/applet/qml/GlobalMenuPopup.qml:5` is a plain `Popup`
  with no `popupType: Popup.Window`. In the configured Qt 6.11.1,
  `/usr/include/qt6/QtQuickTemplates2/6.11.1/QtQuickTemplates2/private/qquickpopup_p_p.h:208`
  confirms the default is `QQuickPopup::Item`, so this popup does not create a
  separate keyboard-capable window.

Consequently, clicking a menu entry can show the item popup, but neither the
panel nor the popup has a Wayland keyboard-focus path. `focus: true` and
`forceActiveFocus()` only move QML focus inside a window after that window has
keyboard focus; they cannot override the window flag or layer-shell keyboard
interactivity.

The candidate's only keyboard proof does not compose the production surface.
`tests/shell/global_menu/qml/tst_GlobalMenuAppletSubmenu.qml:7` hosts the applet
under the focusable QtTest window, and lines 82-100 explicitly force QML focus
before injecting keys. That test passes while the production route remains
impossible.

Reproduction (scratch file is outside the worktree):

```console
$ python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/repros/check_production_popup_keyboard.py /home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition-codex-review
FAIL: the production popup cannot receive keyboard input
observed: .../src/shell/qml/RuntimePanel.qml:20 rejects focus
observed: .../src/shell_surface/src/layer_shell_surface_backend.cpp:498 disables layer-shell keyboard interactivity
observed: .../src/shell/global_menu/applet/qml/GlobalMenuPopup.qml:5 is an item popup with no independent popup window
observed: /usr/include/qt6/QtQuickTemplates2/6.11.1/QtQuickTemplates2/private/qquickpopup_p_p.h:208 confirms the Qt default
expected: a keyboard-capable popup surface or scoped keyboard interactivity while the menu is open
$ echo $?
1
```

Observed: production composition has no keyboard-capable surface for the
implemented handlers. Expected: the documented keyboard interaction works in
the production shell, typically through a separately focusable popup surface
or tightly scoped layer-shell keyboard interactivity while the menu is open.
A live nested-compositor reproduction was not run because this lane explicitly
forbids nested compositor rows; the deterministic source/Qt-default check above
establishes the missing input route without touching the host desktop.

### P2

#### P2-01 — The required distinct-process hostile ownership test is missing

The production code appears to perform the intended fail-closed join:

- `src/shell/runtime/globalmenuappletcomposition.cpp:95-132` obtains the exact
  compositor-projected AppMenu id and requires the current snapshot's UUID and
  PID to match the expected active window.
- `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp:88-100`
  selects the exact registrar entry, and lines 148-155 authenticate that entry's
  unique owner through bus credentials before binding.
- `tests/shell/global_menu/ownership/tst_menu_ownership.cpp:150-179` proves the
  isolated authenticator rejects synthetic PID mismatches.
- `src/shell_window_actions_client/src/shell_window_actions_client.cpp:223-251`
  rejects old-owner replies, epoch changes, revision regression, and changed
  bytes at equal revision. The focused client test at
  `tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp:225-247`
  exercises revision regression, and the adjacent private-bus test exercises
  owner replacement.

However, the candidate does not execute the hostile boundary required by this
review brief. In
`tests/shell/global_menu/runtime_composition/tst_global_menu_runtime_composition.cpp:69-80`,
the fake compositor authenticates the provider using
`QCoreApplication::applicationPid()`. The shell and provider at lines 122-127
are only two D-Bus connections in that same test process. The transport test
does the same at
`tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp:93-123`.
Neither test starts a second process, registers a foreign menu under its
distinct PID, or asserts both facade unavailability and zero `Event` calls.

Reproduction (scratch file is outside the worktree):

```console
$ python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/repros/check_cross_process_ownership_test.py /home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition-codex-review
FAIL: candidate integration tests never create a second process
observed: .../tests/shell/global_menu/runtime_composition/tst_global_menu_runtime_composition.cpp:78 authenticates the provider with the test runner PID
observed: .../tests/shell/global_menu/runtime_composition/tst_global_menu_runtime_composition.cpp:122-125 creates only another connection in that same process
observed: .../tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp:93-101 likewise creates only same-process connections
expected: a private-bus provider process with a distinct PID, plus mismatch and non-activation assertions
$ echo $?
1
```

Observed: the private-bus integration path proves only the positive same-PID
case; PID mismatch is covered only below the transport boundary with fake
credential values. Expected: a real second process on the private bus registers
the matching AppMenu window id, the compositor snapshot supplies a different
PID, the facade remains typed unavailable, and an attempted activation produces
no provider `Event`. This is a missing negative control, not a claim that the
inspected product join currently accepts the spoof.

### P3

None.

## Review-question evidence

1. **Exact AppMenu id and PID join:** source inspection shows the projected id
   selects one registrar record and its exact unique owner is credential-checked
   against the compositor PID before binding. Synthetic mismatch unit cases pass.
   The required real distinct-process hostile case is absent (P2-01).
2. **No display or activation after authority loss:** the coordinator calls
   `clearAuthority()` on missing focus/endpoint/authentication and before any
   rejected publish; `clearAuthority()` stops the client, clears selector state,
   and publishes unavailable at
   `global_menu_transport_coordinator.cpp:257-267`. Owner-loss and exactly-once
   positive activation rows pass. No distinct-process zero-Event negative proof
   exists (P2-01).
3. **Stale identity lineage:** the shared exact-owner client rejects owner/epoch/
   revision regression as cited above. Its unit and private-bus adjacent rows
   pass in the prescribed Debug and Release profiles.
4. **Registrar collision/teardown:** candidate runtime composition publishes
   `degraded`/`registrar-name-owned` on collision and calls registrar `stop()` on
   teardown; focused private-bus coverage passes.
5. **Popup bounds, accessibility, and exactly-once intent:** offscreen tests pass
   depth, navigation, focus-loss, accessibility, and one-call assertions, but
   their focusable QtTest host does not prove the production keyboard route
   (P1-01).

## Commands and results

### Immutable candidate checks

```console
$ git rev-parse HEAD
f7a49c5e916f210665e72e6fccdfa134b0ed43c3
$ git rev-parse HEAD^{tree}
4578cee85710d29ad877f40e4735ec3fdccc0085
$ git rev-parse HEAD^
d9aec19cd2eab555373d683c304a7514ba123a0f
$ git rev-parse d9aec19
d9aec19cd2eab555373d683c304a7514ba123a0f
$ git status --porcelain
$ echo $?
0
```

`git status --porcelain` was empty before review and again after all review
work. No product path was edited, committed, amended, or rebased.

### Configure

```console
$ cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
exit 0
$ cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
exit 0
```

Both configurations completed. CMake emitted the repository's dependency-root
runtime-path warnings; configuration did not fail.

### Focused builds

The following exact target vector was built once with `<B>` equal to the Debug
path and once with `<B>` equal to the Release path:

```console
$ cmake --build <B> --parallel 3 --target qindaqt-shell qindaqt-shell-preview qindaqt_global_menu_qml qindaqt_global_menu_qmlplugin qindaqt_global_menu_protocol_tests qindaqt_global_menu_ownership_tests qindaqt_global_menu_lineage_tests qindaqt_global_menu_exporter_tests qindaqt_global_menu_qt_widgets_adapter_tests qindaqt_global_menu_applet_access_tests qindaqt_global_menu_composition_tests qindaqt_global_menu_registrar_tests qindaqt_global_menu_dbusmenu_decoder_tests qindaqt_global_menu_dbusmenu_client_tests qindaqt_global_menu_transport_composition_tests qindaqt_global_menu_runtime_composition_tests qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests qindaqt_shell_runtime_options_tests qindaqt_shell_launcher_qmlplugin qindaqt_launcher_installed_probe qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
```

- `<B>=/home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/debug`: exit 0, 743/743 steps.
- `<B>=/home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/release`: exit 0, 743/743 steps.

Adjacent identity-client targets were also built in both profiles:

```console
$ cmake --build <B> --parallel 3 --target qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests
```

Debug exit 0; Release exit 0.

### Focused and adjacent tests

All test invocations used the required isolated outer environment:

```console
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> -R '^(qindaqt\.global-menu-|qindaqt\.applet-(manifest|catalog|runtime-resolution)$|qindaqt\.shell-runtime-(options|catalog|component-closure)$|qindaqt\.(audio|bluetooth|power)-applet-installed-package$|qindaqt\.launcher-installed-package$)' --output-on-failure --no-tests=error
```

- Debug: exit 0, 30/30 passed, 20.91 s.
- Release: exit 0, 30/30 passed, 11.65 s.

The global-menu-only selector was also run independently:

```console
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> -R '^qindaqt\.global-menu-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 20/20 passed, 2.88 s.
- Release: exit 0, 20/20 passed, 2.58 s.

Adjacent exact-owner identity-client selector:

```console
$ env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error
```

- Debug prescribed path: exit 0, 2/2 passed, 0.72 s.
- Release: exit 0, 2/2 passed, 0.78 s.

No `tests/session` nested-compositor row, `compositor.kwin-*` row, host system or
session service, hardware, uinput, or network test was run. The selected
`qindaqt.shell-runtime-options` row is the non-compositor options test explicitly
requested by the lane.

### Static gates

```console
$ ./tools/validate-docs
Validated 127 Markdown documents and mkdocs.yml navigation.
exit 0

$ /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/site
Documentation built in 1.64 seconds.
exit 0

$ ./tools/check-source-shape
Checked 2051 source files; skipped 0 allowlisted files.
exit 0
```

The source-shape gate emitted three existing threshold warnings at
`tests/compositor/CMakeLists.txt` (500),
`tests/services/display_color_model/tst_color_model.cpp` (539), and
`tests/shell/audio_applet/tst_audio_applet_controller.cpp` (563). None is
changed by this candidate.

```console
$ git diff --check && git diff d9aec19..f7a49c5 --check
exit 0
$ python3 -m json.tool data/applet-policy/default.json >/dev/null
exit 0
```

`data/applet-policy/default.json` is the only changed JSON file.

## Verdict

The candidate is not ready to integrate. The production surface cannot deliver
the keyboard events promised by the new global-menu UI, and the proof-bound
ownership boundary lacks the explicitly required distinct-process hostile
negative control.

VERDICT REJECT P0/P1/P2/P3=0/1/1/0
