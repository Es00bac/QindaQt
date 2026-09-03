# Milly Koss repair handoff — Global Menu G2

- Candidate commit: `24240e904a4e117c8b48698272465e37c1c25e80`.
- Candidate tree: `a82e057af60532b1710ab271d89b52fd7318bf37`.
- Exact base: `d9aec19cd2eab555373d683c304a7514ba123a0f`.
- Rejected ancestor: `f7a49c5e916f210665e72e6fccdfa134b0ed43c3`.
- Closing commit for both findings: `24240e904a4e117c8b48698272465e37c1c25e80`.
- Requested next action: independent exact review by Joan Ball, then manager integration.

## Finding closure

- P1-01: `GlobalMenuPopup` is now an independent `Popup.Window`, with active-window/focus-loss closure preserved. Registered test `qindaqt.global-menu-production-panel-keyboard-qml-offscreen` hosts the real `PanelAppletRow` → `AppletChip` → `BuiltinAppletContent` route, requires the effective popup type, and completes Tab → Down → Down → Right → Space with one activation and closure under `QT_FATAL_WARNINGS=1`. The adjacent submenu row retains Escape, Left, depth, accessibility, and focus-loss coverage.
- P2-01: `qindaqt.global-menu-runtime-composition-private-bus` now starts `qindaqt_global_menu_hostile_provider` as a distinct real process on the private bus. The test confirms its PID differs from the shell test, then covers compositor-PID mismatch and a regressed revision whose facts would otherwise match that child. Both variants keep the facade unavailable/empty and the child probe reports zero dbusmenu `Event` calls.
- Both finding-specific tests carry `AGENT-NOTE:` markers naming the reviewer finding.

## Changed paths in the exact candidate, sorted

- `data/applet-policy/default.json`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/applet-manifest-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/global-menu.md`
- `ops/team/messages/shell-global-menu/1788423221-milly-koss-codex-claim.md`
- `ops/team/messages/shell-global-menu/1788424642-milly-koss-codex-midpoint.md`
- `ops/team/messages/shell-global-menu/1788425602-milly-koss-codex-verification.md`
- `ops/team/messages/shell-global-menu/1788425714-milly-koss-codex-handoff.md`
- `ops/team/workers/milly-koss-codex.md`
- `src/applet_runtime/src/builtin_applet_registry.cpp`
- `src/shell/CMakeLists.txt`
- `src/shell/global_menu/applet/CMakeLists.txt`
- `src/shell/global_menu/applet/include/qindaqt/shell/global_menu/applet/globalmenuappletaccess.h`
- `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml`
- `src/shell/global_menu/applet/qml/GlobalMenuPopup.qml`
- `src/shell/global_menu/applet/src/globalmenuappletaccess.cpp`
- `src/shell/global_menu/composition/CMakeLists.txt`
- `src/shell/global_menu/composition/include/qindaqt/shell/global_menu/composition/announced_menu_address_source.h`
- `src/shell/global_menu/composition/include/qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h`
- `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp`
- `src/shell/global_menu/ownership/include/qindaqt/shell/global_menu/ownership/active_window_source.h`
- `src/shell/global_menu/ownership/include/qindaqt/shell/global_menu/ownership/provider_authenticator.h`
- `src/shell/qml/BuiltinAppletContent.qml`
- `src/shell/qml/PanelAppletColumn.qml`
- `src/shell/qml/PanelAppletRow.qml`
- `src/shell/qml/PanelContent.qml`
- `src/shell/qml/RuntimePanel.qml`
- `src/shell/runtime/globalmenuappletcomposition.cpp`
- `src/shell/runtime/globalmenuappletcomposition.h`
- `src/shell/runtime/runtimepanelwindowfactory.cpp`
- `src/shell/runtime/runtimepanelwindowfactory.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `tests/applet_runtime/tst_applet_instance_resolver.cpp`
- `tests/applets/tst_catalog.cpp`
- `tests/shell/audio_applet/run_shell_component_closure.cmake`
- `tests/shell/global_menu/CMakeLists.txt`
- `tests/shell/global_menu/applet/tst_globalmenuappletaccess.cpp`
- `tests/shell/global_menu/boundary/CMakeLists.txt`
- `tests/shell/global_menu/boundary/check_global_menu_runtime_boundary.py`
- `tests/shell/global_menu/qml/CMakeLists.txt`
- `tests/shell/global_menu/qml/tst_GlobalMenuApplet.qml`
- `tests/shell/global_menu/qml/tst_GlobalMenuAppletAccessibility.qml`
- `tests/shell/global_menu/qml/tst_GlobalMenuAppletOverflow.qml`
- `tests/shell/global_menu/qml/tst_GlobalMenuAppletSubmenu.qml`
- `tests/shell/global_menu/qml/tst_GlobalMenuAppletVerticalBoundary.qml`
- `tests/shell/global_menu/qml/tst_GlobalMenuProductionPanelKeyboard.qml`
- `tests/shell/global_menu/run_installed_global_menu.cmake`
- `tests/shell/global_menu/runtime_composition/CMakeLists.txt`
- `tests/shell/global_menu/runtime_composition/hostile_menu_provider.cpp`
- `tests/shell/global_menu/runtime_composition/tst_global_menu_runtime_composition.cpp`
- `tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp`

## Commands and results

### Exact reviewer reproductions

Before repair, on the product tree matching `f7a49c5`:

```sh
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/repros/check_production_popup_keyboard.py /home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-g2-codex/repros/check_cross_process_ownership_test.py /home/cabewse/work_SPaC3/container-wm-workers/global-menu-composition
```

Both exited 1 with Joan's exact failures. After repair, the same commands each exited 0. They were repeated after the candidate commit and again both exited 0.

### Configure

The assigned recipe was run verbatim for:

- Debug root `/home/cabewse/work_SPaC3/builds/qindaqt/global-menu-composition/debug`: exit 0.
- Release root `/home/cabewse/work_SPaC3/builds/qindaqt/global-menu-composition/release`: exit 0.

Both used the pinned 6.6.5 initial cache, `BUILD_TESTING=ON`, all three shell/plugin build switches, host-uinput disabled, and strict warnings enabled. CMake emitted the repository's existing dependency-root runtime-path warnings but completed.

### Focused builds

```sh
cmake --build <B> --parallel 3 --target qindaqt-shell qindaqt-shell-preview qindaqt_global_menu_qml qindaqt_global_menu_qmlplugin qindaqt_global_menu_protocol_tests qindaqt_global_menu_ownership_tests qindaqt_global_menu_lineage_tests qindaqt_global_menu_exporter_tests qindaqt_global_menu_qt_widgets_adapter_tests qindaqt_global_menu_applet_access_tests qindaqt_global_menu_composition_tests qindaqt_global_menu_registrar_tests qindaqt_global_menu_dbusmenu_decoder_tests qindaqt_global_menu_dbusmenu_client_tests qindaqt_global_menu_transport_composition_tests qindaqt_global_menu_hostile_provider qindaqt_global_menu_runtime_composition_tests qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests qindaqt_shell_runtime_options_tests qindaqt_shell_launcher_qmlplugin qindaqt_launcher_installed_probe qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
```

- Debug: exit 0; 12/12 incremental actions after exact reconfigure.
- Release: exit 0; 28/28 incremental actions after exact reconfigure.

Earlier repair-loop focused builds of the same owned targets also exited 0. One optional `qindaqt_global_menu_qml_qmllint` diagnostic expanded to 316 unrelated whole-tree type-registration actions, so it was intentionally interrupted in both profiles (exit 130) to obey the lane's bounded-build rule; it is not an acceptance gate. The strict QML cache build and fatal-warning runtime rows below completed in both profiles.

### Required tests

Every final invocation used:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <B> ... --output-on-failure --no-tests=error
```

Global Menu selector `-R '^qindaqt\.global-menu-'`:

- Debug: exit 0, 21/21 passed, 3.31 seconds.
- Release: exit 0, 21/21 passed, 3.15 seconds.

Applet integrity, every shell-runtime row including component closure, and sibling installed-package selector:

```sh
-R '^(qindaqt\.applet-(manifest|catalog|runtime-resolution)$|qindaqt\.shell-runtime-(options|catalog|component-closure)$|qindaqt\.(audio|bluetooth|power)-applet-installed-package$|qindaqt\.launcher-installed-package$)'
```

- Debug: exit 0, 10/10 passed, 17.82 seconds.
- Release: exit 0, 10/10 passed, 9.04 seconds.

Focused repair-loop evidence also reached: runtime composition 1/1, production-panel keyboard 1/1, and submenu plus production-panel keyboard 2/2 after the initial deliberately failing implementation iterations were corrected.

### Static gates

- `./tools/validate-docs`: exit 0; validated 127 Markdown documents and navigation.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/global-menu-composition/site`: exit 0; built in 1.47 seconds.
- `./tools/check-source-shape`: exit 0; checked 2,053 files and skipped none. It repeated the three pre-existing threshold warnings in `tests/compositor/CMakeLists.txt`, `tests/services/display_color_model/tst_color_model.cpp`, and `tests/shell/audio_applet/tst_audio_applet_controller.cpp`; the candidate changes none of them.
- `git diff --check`: exit 0.
- `git diff d9aec19cd2eab555373d683c304a7514ba123a0f..24240e904a4e117c8b48698272465e37c1c25e80 --check`: exit 0.
- `python3 -m json.tool data/applet-policy/default.json > /dev/null`: exit 0.

## Bounded caveats

- No nested compositor/session, host session/system bus, hardware, uinput, or network row was run.
- This remains private-bus, offscreen software-renderer, package-relocation, and static evidence. It does not claim a foreign-toolkit exporter, a real login session, installed nested-session behavior, live AT-SPI traversal, GPU rendering, or physical input.
- The production layer-shell panel remains deliberately non-focusable. The submenu's independent transient window is the keyboard-capable surface; this repair does not widen panel keyboard interactivity or add a global menu shortcut.
