# Global Menu G2 shell composition handoff

- Candidate commit: `f7a49c5e916f210665e72e6fccdfa134b0ed43c3`
- Candidate tree: `4578cee85710d29ad877f40e4735ec3fdccc0085`
- Exact base: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Branch: `worker/global-menu-composition`
- Implementer: Milly Koss (`milly-koss-codex`)

## Outcome

The production shell now composes the accepted Global Menu model and AppMenu/
dbusmenu transports through its single exact-owner window-actions client. The
audited built-in owns the registrar on the injected session bus, authenticates
numeric and native announced menu addresses, clears stale truth on replacement
or loss, and reports registrar name collision as degraded. Production panels
host the compiled recursive Global Menu QML module with bounded keyboard- and
accessibility-complete submenus. Least-authority policy, built-in registration,
relocatable `GlobalMenuAppletRuntime` packaging, source poison, and the shared
shell component-closure proof land in the same candidate.

The existing manifest and stock profile placements already matched the lane's
accepted contract, so this candidate validates them without rewriting them.

## Changed paths

- `data/applet-policy/default.json`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/applet-manifest-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/global-menu.md`
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
- `tests/shell/global_menu/run_installed_global_menu.cmake`
- `tests/shell/global_menu/runtime_composition/CMakeLists.txt`
- `tests/shell/global_menu/runtime_composition/tst_global_menu_runtime_composition.cpp`
- `tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp`

## Acceptance evidence

All listed commands ran from the lane worktree unless they identify a build
directory. Every final acceptance command exited 0.

### Configure

Debug:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/global-menu-composition/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Exit 0.

Release used the identical command with build directory `release` and
`-DCMAKE_BUILD_TYPE=Release`; exit 0.

### Strict focused builds

The following ran once against each of `debug` and `release`, with exit 0 in
both profiles:

```sh
cmake --build <profile-build-dir> --parallel 3 --target \
  qindaqt-shell qindaqt-shell-preview qindaqt_global_menu_qml \
  qindaqt_global_menu_qmlplugin qindaqt_global_menu_protocol_tests \
  qindaqt_global_menu_ownership_tests qindaqt_global_menu_lineage_tests \
  qindaqt_global_menu_exporter_tests \
  qindaqt_global_menu_qt_widgets_adapter_tests \
  qindaqt_global_menu_applet_access_tests \
  qindaqt_global_menu_composition_tests \
  qindaqt_global_menu_registrar_tests \
  qindaqt_global_menu_dbusmenu_decoder_tests \
  qindaqt_global_menu_dbusmenu_client_tests \
  qindaqt_global_menu_transport_composition_tests \
  qindaqt_global_menu_runtime_composition_tests \
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
  qindaqt_applet_instance_resolver_tests qindaqt_shell_runtime_options_tests \
  qindaqt_shell_launcher_qmlplugin qindaqt_launcher_installed_probe \
  qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
```

### Tests

Post-candidate combined selector, run in both profiles:

```sh
ctest --test-dir <profile-build-dir> \
  -R '^(qindaqt\.global-menu-|qindaqt\.applet-(manifest|catalog|runtime-resolution)$|qindaqt\.shell-runtime-(options|catalog|component-closure)$|qindaqt\.(audio|bluetooth|power)-applet-installed-package$|qindaqt\.launcher-installed-package$)' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 30/30 passed.
- Release: exit 0, 30/30 passed.

Corroborating pre-candidate split runs also exited 0 in both profiles:

```sh
ctest --test-dir <profile-build-dir> -R '^qindaqt\.global-menu-' \
  --output-on-failure --no-tests=error
```

- Debug: 20/20 passed.
- Release: 20/20 passed.

```sh
ctest --test-dir <profile-build-dir> \
  -R '^(qindaqt\.applet-(manifest|catalog|runtime-resolution)|qindaqt\.shell-runtime-(options|catalog|component-closure)|qindaqt\.(audio|bluetooth|power)-applet-installed-package|qindaqt\.launcher-installed-package)$' \
  --output-on-failure --no-tests=error
```

- Debug: 10/10 passed.
- Release: 10/10 passed.

The QML rows set `QT_FATAL_WARNINGS=1`; transport/runtime integration uses
private `dbus-run-session` buses. No host-desktop row ran.

### Static and documentation gates

```sh
./tools/validate-docs
```

Exit 0; validated 127 Markdown documents and navigation.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/global-menu-composition/site
```

Exit 0.

```sh
./tools/check-source-shape
```

Exit 0; checked 2051 files. It reported only the three pre-existing threshold
warnings in `tests/compositor/CMakeLists.txt`,
`tests/services/display_color_model/tst_color_model.cpp`, and
`tests/shell/audio_applet/tst_audio_applet_controller.cpp`.

```sh
git diff --check
python3 -m json.tool data/applet-policy/default.json > /dev/null
```

Both exited 0.

Repair-loop commands that are not acceptance evidence did expose and lead to
fixes for an initially mistyped target, a missing enum stringifier, unbuilt QML
plugins/probes, and premature installed/component-closure runs. Those attempts
exited 1; the normalized post-candidate build and 30-row selectors above are
the final evidence.

## Bounded caveats

- This candidate does not implement or qualify GTK/foreign-toolkit exporters.
- It does not claim an installed login session, nested compositor, real host
  D-Bus, hardware, uinput, or network qualification.
- It does not introduce payload-bearing canonical deltas; the authenticated
  full-tree snapshot remains authoritative.
- Existing top-panel profile placement and the accepted manifest were verified
  but deliberately left byte-for-byte unchanged.

## Requested next action

Independent exact review of candidate
`f7a49c5e916f210665e72e6fccdfa134b0ed43c3`, then manager integration.
