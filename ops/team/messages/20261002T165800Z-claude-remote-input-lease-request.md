# Claude remote-input: material fact and build-slot request

2026-10-02T16:58:00Z

Preserved first vertical slice (unbuilt) on hubs:
- desktop worker/pf-claude-remote-input-20261002 at dfb2a18a949cb208f1344815679be6fc20e22679
- fork worker/pf-claude-remote-input-20261002 at b579064ee54610937f61203c90b5346297a8e2fb

Coordination-point edits (additive, exact paths):
- src/services/portal/CMakeLists.txt: `add_subdirectory(remote_input)`
- src/services/portal/foundation/CMakeLists.txt: composition links `QindaQt::PortalRemoteInput`
- src/services/portal/src/foundation_composition.cpp: owns `RemoteInput::CompositorEis` + `RemoteDesktopAdaptor`
- tests/services/portal/CMakeLists.txt: `add_subdirectory(remote_input)`
- mkdocs.yml, docs/wiki/adr/index.md, docs/wiki/architecture/module-boundaries.md (one row), portal-foundation.md (one link)
No selector, `.portal`, ScreenCast/capture, power or shortcut path changed.

Build-slot request (not started; no competition with the Power build):
1. Desktop: `cmake --preset dev` (if build/dev absent) then
   `cmake --build build/dev --target qindaqt_portal_remote_desktop_tests xdg-desktop-portal-qindaqt qindaqt_portal_access_tests`
   and `ctest --test-dir build/dev -R '^qindaqt\.portal-(remote-desktop|access)$' --output-on-failure --no-tests=error`.
2. Fork: build target `eis` in an existing configured fork build tree.
Please grant the next slot; I continue source work (InputCapture) meanwhile.
