# ED11 Printers & scanners exact native handoff

- Worker: everyday_media_delivery; exposed identity remains Codex collaboration worker with no observed model/provider override.
- Exact runtime/source candidate: `55113184ab9ca8b6a86eb074087aebb9cb6400a5`.
- Original source freeze: `83e261591a16126683fd3219a21b11d2807a6d83`.
- Base: `1a205444cff9264ac0b21a3968506133132db5bc`.
- Branch/worktree: `worker/everyday-printing-settings-20261007` / `everyday-printing-settings-20261007`, preserved on the qinda hub.
- Requested next action: root independently reviews the exact repaired candidate, then integrates accepted source and reruns affected gates. Later Portage-installed and physical journeys remain separately owned.
- Resource state: compiler, offscreen and staged SDK lease explicitly RELEASED to root before authoring this receipt. ED11 started no private bus/compositor or host service.

## Delivered boundary

Route 24 always exists and preserves all previous 23 indices. Three fixed owner actions use an injected public ApplicationCatalog adapter and literal argv starter. Refresh is metadata-only; deliberate launch replans the fixed identity, rejects unsupported plans and bounds argv. Start acknowledgement means submission, with missing/refusal/retry distinct from window, CUPS, device or job readiness. No QML command/service/package action, private CUPS implementation, printing/scanning engine or storage-policy change.

The compiled shared Printing module joins own-prefix Settings preflight. Compact/normal/DPI 2 page fixtures exercise actual public Controls, keyboard traversal and plain labels. Relocated fixtures require the active Loader Ready witness and refuse the missing installed Printing module while the developer copy remains available.

## Exact source repairs and retained failures

Original `83e261` strict production build passed, but full focused CTest was **16/18, exit 8**. All five new Printing gates passed. Three route-search Qt failures assumed five Input destinations although the existing Controllers page makes six; one palette failure omitted existing Portal permissions. The initial driver also supplied an overlong XDG_RUNTIME_DIR; the final driver uses a short private temporary directory, preserving all admission policies.

Fixture-only `4887bfa6e7a4211da4acb70fa4dbd873c0dfdd75` preserved those failures and reran **16/18, exit 8**. Its exact equality check exposed existing metadata/page order drift: InputPage has Controllers last, while metadata had Controllers first. Palette equality exposed Portal permissions at the actual General-before-Hardware sidebar position.

Root explicitly approved the final repair. Commit `55113184` changes only these three paths relative to `4887bfa6`:

- `src/apps/settings_center/settings_route_search_metadata.cpp`: moves the unchanged Controllers block last to satisfy its exact-page-order contract; five-subpages comment becomes six.
- `tests/apps/settings_center/tst_settings_route_search.cpp`: six destinations in actual page order, with Shortcuts at index 3; strict equality retained.
- `tests/apps/settings_center/tst_settings_command_palette.cpp`: exact stable sidebar order places Portal permissions at index 3.

IDs, titles, keywords, matching policy and Input owner source remain unchanged. Original logs and test expectations are preserved in history.

## Direct final gates

Detached ignored verification source HEAD was exactly `55113184ab9ca8b6a86eb074087aebb9cb6400a5`, independently checked against worker HEAD. Strict Debug configuration uses /usr, KDE/libexec, Qt 6.11.1, ccache and actual Portage MAKEOPTS `-j24 -l24`; host session and system buses are blocked, XDG roots are private, and offscreen rendering uses software. No hardware or host service action ran.

| Gate | Direct result |
| --- | --- |
| Strict production Settings plus nine owning executable targets | exit 0 |
| Complete focused native/CLI/relocated CTest | 18/18, exit 0; 100.68 s |
| Qt totals from 11 actual outputs | 87 passed, 0 failed/skipped/blacklisted |
| Final installed public-catalog consumer | 1/1 CTest, 11 Qt passed, exit 0 |
| Deliberately corrupted installed public catalog header | build exit 1 with exact intended marker |
| Restored public header rebuild and consumer | build exit 0; 1/1, 11 Qt passed, exit 0 |
| Documentation links/navigation | 526 Markdown documents, exit 0 |
| Strict MkDocs | exit 0 |
| New production source shape | 11 files, 0 issues, exit 0 |
| New test source shape | 6 files, 0 issues, exit 0 |
| Git diff whitespace check | exit 0 |

The 18 actual CTest rows are five Printing gates; registry, navigation controller, route search, navigation layout, navigation interaction and command palette; unknown-route/missing-theme rejection and desktop identity; all-route construction, missing-witness rejection, witness fallback and installed Settings routes. JUnit directly reports 18 tests, 0 failures, 0 disabled and 0 skipped.

## Reproduction

In the assigned worktree, the exact source is `.cache/printing-exact-83e` (directory name retained; final HEAD is 55113184) and build is `.cache/printing-native-build`. The full configure command is preserved in the original native claim and configure log. Actual targets:

```sh
cmake --build .cache/printing-native-build --target \
  qindaqt-settings \
  qindaqt_settings_printing_model_tests \
  qindaqt_settings_printing_catalog_tests \
  qindaqt_settings_printing_page_tests \
  qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test \
  qindaqt_settings_route_search_test \
  qindaqt_settings_navigation_layout_test \
  qindaqt_settings_navigation_interaction_test \
  qindaqt_settings_command_palette_test -- -j24 -l24
```

Run CTest with DISPLAY and WAYLAND_DISPLAY unset, QT_QPA_PLATFORM=offscreen, QT_QUICK_BACKEND=software, both DBUS addresses set to unix:path=/nonexistent, private XDG config/data/system-data/cache roots, and the short private XDG_RUNTIME_DIR recorded in `short-runtime-path.txt`:

```sh
ctest --test-dir .cache/printing-native-build -j1 --output-on-failure \
  --output-junit .cache/printing-native-logs/ctest-551.xml \
  -R 'qindaqt\.settings-(printing-.*|route-registry|navigation-controller|route-search|navigation-layout|navigation-interaction|command-palette|app-(rejects-unknown-route|rejects-missing-theme|desktop-identity|route-construction(-rejects-missing-witness)?|route-witness-fallback|installed-routes))$'
```

Final documentation commands:

```sh
python3 tools/validate-docs
mkdocs build --strict --site-dir .cache/printing-source-checks/site-551
python3 tools/check-source-shape --root src/apps/settings/printing --json
python3 tools/check-source-shape --root tests/apps/settings/printing --json
git diff --check
```

## Public SDK provenance and fixture correction

The Printing ports are app-local contracts, not an installed ABI promise. The staged SDK check compiles actual production Printing catalog/model/ports sources and its unchanged catalog QtTest against the public producer exports.

A first ignored standalone producer wrapper failed configuration because the existing Launcher CMake deferred into a root/src directory not present in that reduced wrapper. Failure log is retained. The corrected gate uses the actual production-generated launcher and ApplicationCatalog install scripts with component QindaQt and an ignored stage prefix; no producer source or installed-export policy was changed.

The stage contains twelve public headers and two static archives only. Actual compiler dependencies show four staged public headers and **zero producer source-header leaks**. Actual link commands use both staged archives; all 14 staged payloads match final source/build bytes. The standalone consumer links Core/Test, with no Gui/Qml/DBus libraries in ldd. Final provenance includes exact source HEAD, hashes, dependencies and link commands.

The corruption negative replaces installed `application_directory_scan.h` with an explicit #error, then restores it under an EXIT trap before evaluating the failure. This proves use of that installed header; it is not a literal removed-header qualification. The separate Printing module gate actually withholds the installed directory and proves preflight refusal/restoration.

## Durable raw evidence locations

All ignored evidence is preserved under the assigned worktree:

- `.cache/printing-native-logs/configure-83e.log`, `build-83e.log`, `ctest-83e.log`, `ctest-83e.xml`, `LastTest-83e.log`.
- `.cache/printing-native-logs/build-488.log`, `ctest-488.log`, `ctest-488.xml`, `LastTest-488.log`.
- `.cache/printing-native-logs/build-551.log`, `ctest-551.log`, `ctest-551.xml`, `LastTest-551.log`, `gate-counts-551.json`.
- `.cache/printing-native-logs/sdk-producer-configure-83e.log`, `sdk-stage-launcher-488.log`, `sdk-stage-catalog-488.log`.
- `.cache/printing-native-logs/sdk-consumer-build-551.log`, `sdk-consumer-ctest-551.log`, `LastTest-sdk-positive-551.log`, `sdk-header-poison-551.log`, `sdk-header-poison-marker-551.log`, `sdk-header-restored-build-551.log`, `sdk-header-restored-ctest-551.log`, `LastTest-sdk-551.log`.
- `.cache/printing-native-logs/sdk-provenance-551.json`, `sdk-deps-551.log`, `sdk-link-commands-551.log`, `sdk-ldd-551.log`.
- `.cache/printing-native-logs/docs-links-551.log`, `mkdocs-strict-551.log`, `printing-shape-551.json`, `printing-test-shape-551.json`.
- `.cache/printing-sdk/consumer/CMakeLists.txt` preserves the exact standalone consumer recipe; consumer-build and stage remain ignored and uninstalled.
- `.cache/printing-later-inventory/runtime-readiness.json`, `simple-scan-49.1-pretend.log` preserve sanitized later-gate inventory and dependency refusal.

## Remaining bounded gates

Independent exact source acceptance/integration are still required. No successful installed tool launch, CUPS readiness, printer test page/job cancellation, scan/output/save/unplug or physical hardware receipt is claimed. ADR-0356 remains Proposed until root's accepted decision record.

Readonly inventory found loaded but disabled/inactive CUPS service/socket/path, no active socket/default printers.conf, and zero classified USB printer/imaging/SANE records. These counts do not rule out network or unclassified devices. Scanner runtime is unqualified.

Stable exact `media-gfx/simple-scan-49.1::gentoo` Portage pretend exits 1 with an Avahi mdnsresponder-compat slot conflict; proposed closure is 18 packages and 75,413 KiB, including SANE/libadwaita/libgusb. Root/Platform own that later package repair. No package, configuration, world, service or device transaction occurred.

A handoff is not a stopping point: after preserving this record, the worker reads the current First-party queue and offers the next compatible source or bounded owner-delegation qualification packet.

## Exact candidate changed paths

- `docs/wiki/adr/0356-delegate-printer-and-scanner-settings-to-installed-tools.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/printers-scanners-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/reference/settings-completeness.md`
- `mkdocs.yml`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261007T230459Z-ed11-source-claim.md`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261007T232714Z-ed11-source-freeze.md`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261007T233552Z-ed11-later-gate-inventory-claim.md`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261007T233903Z-ed11-native-claim-and-package-finding.md`
- `ops/team/workers/ed-media-delivery-sol-20261007.md`
- `src/CMakeLists.txt`
- `src/apps/settings/printing/CMakeLists.txt`
- `src/apps/settings/printing/application_printing_tool_catalog.cpp`
- `src/apps/settings/printing/include/qindaqt/apps/settings_printing/application_printing_tool_catalog.h`
- `src/apps/settings/printing/include/qindaqt/apps/settings_printing/printing_settings_model.h`
- `src/apps/settings/printing/include/qindaqt/apps/settings_printing/printing_tool_ports.h`
- `src/apps/settings/printing/printing_route_composition.cpp`
- `src/apps/settings/printing/printing_route_composition.h`
- `src/apps/settings/printing/printing_settings_model.cpp`
- `src/apps/settings/printing/printing_tool_ports.cpp`
- `src/apps/settings/printing/qml/PrintersScannersPage.qml`
- `src/apps/settings/printing/qml/PrintingToolCard.qml`
- `src/apps/settings_center/AddedRouteComponents.qml`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `src/apps/settings_center/SettingsRouteSupplementalLoaders.qml`
- `src/apps/settings_center/main.cpp`
- `src/apps/settings_center/settings_route.cpp`
- `src/apps/settings_center/settings_route.h`
- `src/apps/settings_center/settings_route_registry.cpp`
- `src/apps/settings_center/settings_route_registry.h`
- `src/apps/settings_center/settings_route_search_metadata.cpp`
- `tests/CMakeLists.txt`
- `tests/apps/settings/printing/CMakeLists.txt`
- `tests/apps/settings/printing/check_installed_printing_route.cmake`
- `tests/apps/settings/printing/printing_test_support.h`
- `tests/apps/settings/printing/tst_printing_page.cpp`
- `tests/apps/settings/printing/tst_printing_settings_model.cpp`
- `tests/apps/settings/printing/tst_printing_tool_catalog.cpp`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/check_route_construction.cmake`
- `tests/apps/settings_center/tst_settings_command_palette.cpp`
- `tests/apps/settings_center/tst_settings_navigation_controller.cpp`
- `tests/apps/settings_center/tst_settings_route_registry.cpp`
- `tests/apps/settings_center/tst_settings_route_search.cpp`
