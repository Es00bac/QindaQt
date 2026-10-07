# Ordinary media candidate gates and independent-review handoff

- Worker: everyday_media_delivery; Codex collaboration worker, no exposed model override inferred.
- Exact production/source candidate: 64dcfa1a2b15c065c97d43e0f30e5c6bca173633, descendant of repaired readonly823738ab1.
- Requested next: Astra immutable ce338 versus current consumer fixtures, exact repaired source acceptance, then manager-only integration and affected integrated checks. Handoff is not a stopping point.
- Resource state: all compiler/private-session-bus/offscreen/private-KWin resources explicitly released to Astra before this snapshot. No active desktop, system-bus device action, physical USB mutation or installation performed.
- Product outcome: one public owner inventory/action client; private ordinary admission/UDisks convergence; fixed graphical-owner launch adapter; File Manager device sidebar and all-controller revoked-location guards; native chooser media rows, read-only Save refusal and post-modal selection checks.
- Strict full focused build: exit0; production File Manager, Removable Media, portal chooser, public/client/owner and selected tests compiled with repository strict warnings. Final29-row CTest: exit0,29/29 passed,339 Qt checks,0 failed,0 skipped,0 blacklisted. Native actual frontend/private-KWin row:1/1 and9 Qt checks. Consumer source regressions: File Manager11Qt, compact/desktop UI5Qt, chooser12Qt. Owning UDisks16Qt, actions8Qt, inventory11Qt, fixed-owner launch5Qt.
- SDK: final installed-only Core/DBus consumer1/1 exit0; withheld staged media_client.h produced expected fatal missing-header compile error; header restored by bounded trap, rebuild+1/1 exit0.
- Static: source diff check, repository link checker520 and strict MkDocs exit0 at source freeze; documentation/evidence snapshot reruns both.
- Remaining bounded gates: independent exact acceptance; manager integrated gates; Portage component/payload/session journey; physical mounted/unmounted insertion/open/eject and data-loss qualification. Production NativeLock transport startup error classification is separately root-owned and is not fixed by consumer-fixture readiness. No ED04-complete or hardware claim.

## Verification recipe

All commands ran in the isolated worker worktree; output/stages stayed ignored.
The initial fixed capture path mismatch was resolved by matching actual installed
/usr/libexec metadata, preserving the configure guard.

```sh
cmake -S . -B .cache/media-ordinary-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=libexec -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_SHELL=OFF -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_BUILD_VIEWER=OFF -DQINDAQT_BUILD_OBS_BRIDGE=OFF -DQINDAQT_BUILD_QINDALUTRIS=OFF -DQINDAQT_BUILD_SYSTEM_MONITOR=OFF -DQINDAQT_BUILD_REMOVABLE_MEDIA=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DBUILD_TESTING=ON
cmake --build .cache/media-ordinary-build --target qindaqt-file-manager qindaqt-portal-chooser qindaqt-removable-media qindaqt_media_protocol_codec_tests qindaqt_media_protocol_hostile_tests qindaqt_media_protocol_validation_tests qindaqt_media_inventory_tests qindaqt_media_actions_tests qindaqt_media_owner_launcher_tests qindaqt_media_policy_tests qindaqt_media_udisks_tests qindaqt_media_notifications_tests qindaqt_media_qml_tests qindaqt_file_manager_media_tests qindaqt_file_manager_media_ui_tests qindaqt_file_manager_history_tests qindaqt_file_manager_controller_tests qindaqt_file_manager_controller_network_tests qindaqt_file_manager_controller_network_mutation_tests qindaqt_file_manager_icon_zoom_tests qindaqt_file_manager_listing_order_tests qindaqt_file_manager_name_filter_tests qindaqt_file_manager_views_ui_tests qindaqt_file_manager_browsing_ui_tests qindaqt_file_manager_applications_ui_tests qindaqt_file_manager_network_hub_ui_tests qindaqt_file_manager_viewport_tests qindaqt_portal_media_chooser_tests qindaqt_portal_chooser_policy_tests qindaqt_portal_chooser_request_tests qindaqt_portal_native_chooser_tests -- -j24 -l24
ctest --test-dir .cache/media-ordinary-build -R '^qindaqt\.(removable-media.*|file-manager-(media|media-ui|navigation-history|navigation-controller(-network(-mutation)?)?|icon-zoom|listing-order|name-filter|views-ui|browsing-ui|applications-ui|network-hub-ui|viewport|views-qml)|portal-(media-chooser|chooser-policy|chooser-requests|native-choosers))$' --output-on-failure -V -j1
cmake -S tests/services/removable_media_client/standalone -B .cache/media-ordinary-sdk-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_LIBDIR=lib64 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build .cache/media-ordinary-sdk-build --target qindaqt_removable_media_client -- -j24 -l24
cmake --install .cache/media-ordinary-sdk-build --prefix "$PWD/.cache/media-ordinary-sdk-stage/usr"
cmake -S tests/services/removable_media_client/installed_consumer -B .cache/media-ordinary-sdk-consumer-build -G Ninja -DMEDIA_STAGE="$PWD/.cache/media-ordinary-sdk-stage/usr" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build .cache/media-ordinary-sdk-consumer-build -- -j24 -l24
ctest --test-dir .cache/media-ordinary-sdk-consumer-build --output-on-failure
```

The final incremental native fixture build reused the same strict build tree.
Header poison used --clean-first with only the staged header withheld; no
source or private include fallback was added. Native fixtures scrub ambient
display variables, own a private bus and compositor, and set a nonexistent
system bus. Fake UDisks never touches host devices.

## Preserved logs and regression identities

- .cache/media-ordinary-configure-prefix.log, .cache/media-ordinary-build-repaired.log, .cache/media-ordinary-final-build.log
- .cache/media-ordinary-final-ctest.log, .cache/media-ordinary-final-last-test.log, .cache/media-ordinary-final-counts.json
- .cache/media-ordinary-final-sdk-build.log, .cache/media-ordinary-final-sdk-install.log, .cache/media-ordinary-final-sdk-consumer-build.log, .cache/media-ordinary-final-sdk-consumer-ctest.log
- .cache/media-ordinary-final-sdk-poison.log, .cache/media-ordinary-final-sdk-restored-build.log, .cache/media-ordinary-final-sdk-restored-ctest.log
- Original failures: .cache/media-ordinary-build.log (missing definition/shadow), .cache/media-ordinary-ctest.log (delegated UI lookup), .cache/media-ordinary-native-chooser.log (pre-spawn lock-object startup race). Direct ignored diagnostics .cache/media-native-lock-introspection.log and .cache/media-native-lock-diagnostic.log retain available qualified receipt evidence.
- Immutable old baseline for semantic negative replay: ce33807aa37b7d01118a53b5c2dca54648a2956f.
- tests/apps/file_manager/tst_media_presenter.cpp SHA2564ea421aae016c25a5f2c496621725fc475c0f3e12fc9a3f593f64d46956d1e5d.
- tests/services/portal/choosers/tst_media_chooser.cpp SHA2560c6d45581a81c814297a7d33fc4f622762947b679e6ac4b44d20cf06d84dc0ca.
- Built current consumer binaries: .cache/media-ordinary-build/tests/apps/file_manager/qindaqt_file_manager_media_tests and .cache/media-ordinary-build/tests/services/portal/choosers/qindaqt_portal_media_chooser_tests.
- Every count above comes directly from completed final logs. The separate documentation-only snapshot changes no production/test source.

## Changed paths since readonly acceptance

- docs/wiki/apps/file-manager.md
- docs/wiki/apps/removable-media.md
- docs/wiki/architecture/module-boundaries.md
- docs/wiki/architecture/removable-media-client.md
- docs/wiki/reference/portal-choosers.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T214718Z-media-actions-consumers-source.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T215550Z-media-readonly-repair-gates.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T220331Z-media-ordinary-source-freeze.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T220731Z-media-block-replacement-source-finding.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T221646Z-media-ordinary-review-repair-freeze.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T222544Z-media-strict-build-repair.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T223047Z-media-ui-fixture-repair.md
- ops/team/messages/everyday-desktop-implementation-20261007/20261007T224056Z-media-native-readiness-freeze.md
- ops/team/workers/ed-media-delivery-sol-20261007.md
- src/apps/file_manager/CMakeLists.txt
- src/apps/file_manager/main.cpp
- src/apps/file_manager/model/navigation_controller.cpp
- src/apps/file_manager/model/navigation_controller.h
- src/apps/file_manager/model/navigation_media.cpp
- src/apps/file_manager/model/navigation_presentation.cpp
- src/apps/file_manager/runtime/folder_navigations.cpp
- src/apps/file_manager/runtime/folder_navigations.h
- src/apps/file_manager/runtime/media_composition.cpp
- src/apps/file_manager/runtime/media_composition.h
- src/apps/file_manager/runtime/media_presenter.cpp
- src/apps/file_manager/runtime/media_presenter.h
- src/apps/file_manager/ui/Main.qml
- src/apps/file_manager/ui/MediaDeviceSection.qml
- src/apps/file_manager/ui/PlacesSidebar.qml
- src/apps/removable_media/CMakeLists.txt
- src/apps/removable_media/media_backend.h
- src/apps/removable_media/media_controller.cpp
- src/apps/removable_media/media_controller.h
- src/apps/removable_media/media_exporter.cpp
- src/apps/removable_media/media_exporter.h
- src/apps/removable_media/media_exporter_actions.cpp
- src/apps/removable_media/media_public_projection.cpp
- src/apps/removable_media/tests/tst_media_udisks.cpp
- src/apps/removable_media/udisks_backend.cpp
- src/apps/removable_media/udisks_backend.h
- src/apps/removable_media/udisks_convergence.cpp
- src/apps/removable_media/udisks_operations.cpp
- src/services/portal/choosers/CMakeLists.txt
- src/services/portal/choosers/file_chooser_dialog.cpp
- src/services/portal/choosers/file_chooser_dialog.h
- src/services/portal/choosers/main.cpp
- src/services/portal/choosers/media_composition.cpp
- src/services/portal/choosers/media_composition.h
- src/services/portal/choosers/media_presenter.cpp
- src/services/portal/choosers/media_presenter.h
- src/services/portal/choosers/media_sidebar.cpp
- src/services/portal/choosers/media_sidebar.h
- src/services/removable_media_client/CMakeLists.txt
- src/services/removable_media_client/include/qindaqt/services/removable_media_client/desktop_owner_launcher.h
- src/services/removable_media_client/include/qindaqt/services/removable_media_client/media_client.h
- src/services/removable_media_client/include/qindaqt/services/removable_media_client/media_source.h
- src/services/removable_media_client/src/desktop_owner_launcher.cpp
- src/services/removable_media_client/src/media_client.cpp
- src/services/removable_media_client/src/media_client_actions.cpp
- tests/apps/file_manager/CMakeLists.txt
- tests/apps/file_manager/tst_media_presenter.cpp
- tests/apps/file_manager/tst_media_ui.cpp
- tests/services/portal/choosers/CMakeLists.txt
- tests/services/portal/choosers/tst_media_chooser.cpp
- tests/services/portal/choosers/tst_native_choosers.cpp
- tests/services/removable_media_client/CMakeLists.txt
- tests/services/removable_media_client/media_source_fixture.h
- tests/services/removable_media_client/standalone/CMakeLists.txt
- tests/services/removable_media_client/tst_media_actions.cpp
- tests/services/removable_media_client/tst_owner_launcher.cpp
