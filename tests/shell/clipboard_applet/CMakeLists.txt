# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test Quick QuickTest Svg)

# 1. Model unit tests
qt_add_executable(qindaqt_clipboard_applet_model_tests tst_clipboard_applet_model.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_model_tests
    PRIVATE
        QindaQt::ShellClipboardApplet
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_model_tests)
add_test(
    NAME qindaqt.clipboard-applet-model
    COMMAND qindaqt_clipboard_applet_model_tests
)

# 2. Controller unit tests
qt_add_executable(qindaqt_clipboard_applet_controller_tests tst_clipboard_applet_controller.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_controller_tests
    PRIVATE
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_controller_tests)
add_test(
    NAME qindaqt.clipboard-applet-controller
    COMMAND qindaqt_clipboard_applet_controller_tests
)

# 3. Controller fencing tests (hostile seam attribution)
qt_add_executable(qindaqt_clipboard_applet_fencing_tests tst_clipboard_applet_fencing.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_fencing_tests
    PRIVATE
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_fencing_tests)
add_test(
    NAME qindaqt.clipboard-applet-fencing
    COMMAND qindaqt_clipboard_applet_fencing_tests
)

# 3b. Snapshot admission tests (hostile-input floor, lineage monotonicity,
# owner fencing, completion-id rejection, promote-tick exhaustion)
qt_add_executable(qindaqt_clipboard_applet_admission_tests tst_clipboard_applet_admission.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_admission_tests
    PRIVATE
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_admission_tests)
add_test(
    NAME qindaqt.clipboard-applet-admission
    COMMAND qindaqt_clipboard_applet_admission_tests
)

# 3c. Whole-snapshot invariants (C0 aggregate/authority/identity consistency)
qt_add_executable(
    qindaqt_clipboard_applet_snapshot_invariant_tests
    tst_clipboard_applet_snapshot_invariants.cpp
)
target_link_libraries(
    qindaqt_clipboard_applet_snapshot_invariant_tests
    PRIVATE
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_snapshot_invariant_tests)
add_test(
    NAME qindaqt.clipboard-applet-snapshot-invariants
    COMMAND qindaqt_clipboard_applet_snapshot_invariant_tests
)

# 4. Client seam tests
qt_add_executable(qindaqt_clipboard_applet_seam_tests tst_clipboard_applet_seam.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_seam_tests
    PRIVATE
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_seam_tests)
add_test(
    NAME qindaqt.clipboard-applet-seam
    COMMAND qindaqt_clipboard_applet_seam_tests
)

qt_add_executable(
    qindaqt_clipboard_applet_composition_private_bus_tests
    tst_clipboard_applet_composition_private_bus.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/clipboardappletcomposition.cpp"
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/clipboardappletcomposition.h"
)
target_include_directories(
    qindaqt_clipboard_applet_composition_private_bus_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/runtime"
)
target_link_libraries(
    qindaqt_clipboard_applet_composition_private_bus_tests
    PRIVATE
        QindaQt::AppletRuntime
        QindaQt::ClipboardClient
        QindaQt::SettingsClient
        QindaQt::ShellClipboardAppletRuntime
        Qt6::DBus
        Qt6::Test
)
target_compile_definitions(
    qindaqt_clipboard_applet_composition_private_bus_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(
    qindaqt_clipboard_applet_composition_private_bus_tests
)
add_test(
    NAME qindaqt.clipboard-applet-composition-private-bus
    COMMAND qindaqt_clipboard_applet_composition_private_bus_tests
            composesConsentOwnerOperationsAndCeilingPurge
)
set_tests_properties(
    qindaqt.clipboard-applet-composition-private-bus
    PROPERTIES LABELS "shell;clipboard;dbus;private-bus" TIMEOUT 30
)

foreach(consent_denial IN ITEMS missing malformed inherited-default ownerless)
    add_test(
        NAME qindaqt.clipboard-applet-consent-${consent_denial}
        COMMAND qindaqt_clipboard_applet_composition_private_bus_tests
                "withholdsDeniedConsentUntilFencedEmptySnapshot:${consent_denial}"
    )
    set_tests_properties(
        qindaqt.clipboard-applet-consent-${consent_denial}
        PROPERTIES LABELS "shell;clipboard;dbus;private-bus;negative-control" TIMEOUT 30
    )
endforeach()

# AGENT-GUARD: every QML row runs through the compiled harness executable
# whose build pulls these prerequisites, so a focused build of the declared
# test targets can never leave QML plugins or the manifest binaries unbuilt
# (a bare qmltestrunner row has no executable prerequisite at all).
add_custom_target(
    qindaqt_clipboard_applet_qml_test_prerequisites
    DEPENDS
        qindaqt_shell_clipboard_applet_runtimeplugin
        qindaqt_shell_audio_applet_runtimeplugin
        qindaqt_shell_bluetooth_applet_runtimeplugin
        qindaqt_shell_power_applet_runtimeplugin
        qindaqt_shell_status_notifier_applet_runtimeplugin
        qindaqt_shell_launcher_qmlplugin
        qindaqt_global_menu_qmlplugin
        qindaqt_controls_qmlplugin
        qindaqt_tokens_qmlplugin
        qindaqt_applet_manifest_tests
        qindaqt_applet_catalog_tests
)

# 5. QML rows through the theme-published compiled harness. The applet's QML
# consumes QST-1 tokens that exist only after a theme is published; the
# harness publishes qinda-light before any test, giving the rows real layout
# for genuine pointer-event delivery and warning-free token evaluation.
# AGENT-GUARD: the QML rows link every hosted applet runtime; configurations
# without the production shell (e.g. the compositor plugin dependency contract)
# must not define them, exactly like the other applet test trees.
if(TARGET qindaqt-shell)
    qt_add_executable(qindaqt_clipboard_applet_qml_tests qml_interactive_main.cpp)
    target_link_libraries(
        qindaqt_clipboard_applet_qml_tests
        PRIVATE
            qindaqt_controls_test_support
            QindaQt::ShellClipboardAppletRuntime
            QindaQt::ShellAudioAppletRuntime
            QindaQt::ShellBluetoothAppletRuntime
            QindaQt::ShellPowerAppletRuntime
            QindaQt::ShellLauncherQml
            QindaQt::ShellTaskListApplet
            QindaQt::GlobalMenuQml
            QindaQt::ShellStatusNotifierAppletRuntime
            QindaQt::ShellIcons
            qindaqt_shell_audio_applet_runtimeplugin
            qindaqt_shell_bluetooth_applet_runtimeplugin
            qindaqt_shell_clipboard_applet_runtimeplugin
            qindaqt_shell_power_applet_runtimeplugin
            qindaqt_shell_status_notifier_applet_runtimeplugin
            qindaqt_shell_task_list_appletplugin
            qindaqt_shell_iconsplugin
            Qt6::Core
            Qt6::Quick
            Qt6::QuickTest
            Qt6::Test
    )
    target_compile_features(qindaqt_clipboard_applet_qml_tests PRIVATE cxx_std_20)
    set_target_properties(qindaqt_clipboard_applet_qml_tests PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt_clipboard_applet_qml_tests)
    qt_import_qml_plugins(qindaqt_clipboard_applet_qml_tests)
    add_dependencies(
        qindaqt_clipboard_applet_qml_tests
        qindaqt_clipboard_applet_qml_test_prerequisites
        qindaqt_shell_task_list_appletplugin
    )

    function(qindaqt_add_clipboard_applet_qml_test test_name test_input)
        add_test(
            NAME ${test_name}
            COMMAND qindaqt_clipboard_applet_qml_tests -input "${test_input}"
        )
        set_tests_properties(
            ${test_name}
            PROPERTIES
                LABELS "offscreen"
                ENVIRONMENT
                    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
                TIMEOUT 300
        )
    endfunction()

    qindaqt_add_clipboard_applet_qml_test(
        qindaqt.clipboard-applet-qml-offscreen
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardApplet.qml"
    )
    qindaqt_add_clipboard_applet_qml_test(
        qindaqt.clipboard-applet-qml-accessibility-offscreen
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletAccessibility.qml"
    )
    qindaqt_add_clipboard_applet_qml_test(
        qindaqt.clipboard-applet-qml-keyboard-offscreen
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletKeyboard.qml"
    )
    qindaqt_add_clipboard_applet_qml_test(
        qindaqt.clipboard-applet-qml-interactive-offscreen
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletInteractive.qml"
    )
    qindaqt_add_clipboard_applet_qml_test(
        qindaqt.clipboard-applet-production-panel-keyboard-offscreen
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardProductionPanelKeyboard.qml"
    )
    qindaqt_add_clipboard_applet_qml_test(
        qindaqt.clipboard-applet-production-panel-return-offscreen
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardProductionPanelReturn.qml"
    )
    set_tests_properties(
        qindaqt.clipboard-applet-production-panel-keyboard-offscreen
        qindaqt.clipboard-applet-production-panel-return-offscreen
        PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
    )
endif()

# 6. Boundary and source policy test
add_test(
    NAME qindaqt.clipboard-applet-boundary-policy
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CLIPBOARD_APPLET_SOURCE_DIR=${CMAKE_SOURCE_DIR}/src/shell/clipboard_applet"
        "-DQINDAQT_CLIPBOARD_COMPOSITION_SOURCE_DIR=${CMAKE_SOURCE_DIR}/src/shell/runtime"
        "-DQINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY=${CMAKE_BINARY_DIR}/tests/clipboard_applet_poison_probe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_clipboard_applet_boundary.cmake"
)

# 7. Installed component consumer/probe: validates the ClipboardApplet install
# rules end-to-end (public boundary, archives, QML module, plugin, manifest)
# from a fresh staged prefix, including the relocated-consumer RPATH truth.
add_test(
    NAME qindaqt.clipboard-applet-installed-package
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-clipboard-applet-stage"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        "-DQINDAQT_CONTROLS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Controls"
        "-DQINDAQT_TOKENS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens"
        "-DQINDAQT_CONTROLS_LIBRARY=$<TARGET_FILE:qindaqt_controls_qml>"
        "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
        "-DQINDAQT_CLIPBOARD_MODEL_LIBRARY=$<TARGET_FILE:qindaqt_clipboard_model>"
        "-DQINDAQT_THEMES_LIBRARY=$<TARGET_FILE:qindaqt_themes>"
        "-DQINDAQT_DESIGN_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_design_tokens>"
        "-DQINDAQT_CLIPBOARD_MODEL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src/services/clipboard_model/include"
        "-DQINDAQT_THEMES_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src/themes/include"
        "-DQINDAQT_DESIGN_TOKENS_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src/design_tokens/include"
        "-DQINDAQT_THEME_FILE=${CMAKE_SOURCE_DIR}/data/themes/qinda-light.json"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_cpp_consumer.cpp"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_STATIC_LIBRARY_SUFFIX=${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "-DQINDAQT_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_clipboard_applet.cmake"
)
set_tests_properties(
    qindaqt.clipboard-applet-installed-package
    PROPERTIES LABELS "package" RUN_SERIAL TRUE TIMEOUT 300
)

add_test(
    NAME qindaqt.clipboard-applet-runtime-installed-package
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-clipboard-runtime"
        "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_QMLDIR=${QT6_INSTALL_QML}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_clipboard_runtime.cmake"
)
set_tests_properties(
    qindaqt.clipboard-applet-runtime-installed-package
    PROPERTIES LABELS "shell;clipboard;installed;poison" RUN_SERIAL TRUE TIMEOUT 90
)
