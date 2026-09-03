# Global Menu G1 candidate handoff — Radia Perlman

- Candidate commit: `7c27ee5b1b50746e59f70360d89b0e959328dd47`
- Candidate tree: `22faf3384afd57dd41f23f560725f605c1fc198f`
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Branch: `worker/global-menu-g1`
- Feature: QQ-004.06 Global application menu — production transports

## Changed paths

- `docs/wiki/adr/0056-adopt-standard-appmenu-dbusmenu-transports.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/global-menu.md`
- `mkdocs.yml`
- `src/shell/global_menu/CMakeLists.txt`
- `src/shell/global_menu/composition/CMakeLists.txt`
- `src/shell/global_menu/composition/include/qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h`
- `src/shell/global_menu/composition/include/qindaqt/shell/global_menu/composition/registrar_window_id_source.h`
- `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp`
- `src/shell/global_menu/dbusmenu/CMakeLists.txt`
- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h`
- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_decoder.h`
- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_client.cpp`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_decoder.cpp`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_wire.cpp`
- `src/shell/global_menu/registrar/CMakeLists.txt`
- `src/shell/global_menu/registrar/include/qindaqt/shell/global_menu/registrar/appmenu_registrar.h`
- `src/shell/global_menu/registrar/include/qindaqt/shell/global_menu/registrar/qt_bus_credential_source.h`
- `src/shell/global_menu/registrar/include/qindaqt/shell/global_menu/registrar/registrar_registry.h`
- `src/shell/global_menu/registrar/include/qindaqt/shell/global_menu/registrar/registrar_wire.h`
- `src/shell/global_menu/registrar/src/appmenu_registrar.cpp`
- `src/shell/global_menu/registrar/src/appmenu_registrar_object.cpp`
- `src/shell/global_menu/registrar/src/appmenu_registrar_object_p.h`
- `src/shell/global_menu/registrar/src/qt_bus_credential_source.cpp`
- `src/shell/global_menu/registrar/src/registrar_registry.cpp`
- `src/shell/global_menu/registrar/src/registrar_wire.cpp`
- `tests/shell/global_menu/CMakeLists.txt`
- `tests/shell/global_menu/boundary/CMakeLists.txt`
- `tests/shell/global_menu/boundary/check_global_menu_transport_boundary.py`
- `tests/shell/global_menu/dbusmenu/CMakeLists.txt`
- `tests/shell/global_menu/dbusmenu/fake_dbusmenu_exporter.h`
- `tests/shell/global_menu/dbusmenu/tst_dbusmenu_client.cpp`
- `tests/shell/global_menu/dbusmenu/tst_dbusmenu_decoder.cpp`
- `tests/shell/global_menu/registrar/CMakeLists.txt`
- `tests/shell/global_menu/registrar/tst_appmenu_registrar.cpp`
- `tests/shell/global_menu/transport_composition/CMakeLists.txt`
- `tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp`

## Verification evidence

- Debug configure with the lane's exact cache/options: exit 0.
- Debug focused build of the 11 global-menu library/test targets: exit 0; final incremental build completed 9 actions.
- `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/global-menu-g1/debug -R '^qindaqt\.global-menu-' --output-on-failure --no-tests=error`: exit 0, 16/16 passed.
- Release configure with the lane's exact cache/options and `-DCMAKE_BUILD_TYPE=Release`: exit 0. CMake emitted the repository's existing mixed-prefix safe-RPATH warnings; generation completed.
- Release focused build of the same 11 targets: exit 0; clean build completed 101 actions and the final hardened rebuild completed 33 actions.
- `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/global-menu-g1/release -R '^qindaqt\.global-menu-' --output-on-failure --no-tests=error`: exit 0, 16/16 passed.
- During hardening, the five-new-row Debug selector first exited 8 with 4/5 passed because its new asynchronous replacement assertion dereferenced the intentionally unavailable transition snapshot. The assertion was corrected to wait for availability; the exact failed row then exited 0 with 1/1 passed, followed by the final 16/16 Debug and Release runs above.
- `./tools/validate-docs`: exit 0, validated 117 Markdown documents and `mkdocs.yml` navigation.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/global-menu-g1/site`: exit 0.
- `./tools/check-source-shape`: exit 0, checked 1,789 files and skipped 0. It reported only the existing review-threshold warnings in `tests/compositor/CMakeLists.txt` and `tests/services/display_color_model/tst_color_model.cpp`; no owned file reached a threshold.
- `git diff --check` and `git diff --cached --check`: exit 0.
- No JSON was changed, so no JSON parser gate applied.

## Bounded caveats

- This candidate deliberately does not instantiate the transports in `src/shell/runtime`, alter QML, wire the applet registry/manifest, or add submenu popup presentation; those remain the later shell-composition lane.
- `GetGroupProperties` and property-update signals are validated invalidations followed by a complete revisioned `GetLayout` reread. Unrevisioned deltas never overwrite canonical truth.
- Evidence uses injected fakes and fresh private `dbus-run-session` brokers only. It makes no host-session, compositor, foreign-toolkit, hardware, network, or installed-desktop claim.

## Requested next action

Independent exact review of `7c27ee5b1b50746e59f70360d89b0e959328dd47`, then manager integration.
