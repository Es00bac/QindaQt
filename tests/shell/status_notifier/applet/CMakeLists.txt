# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Test Quick QuickTest)

# 1. Model unit tests (pure projection)
qt_add_executable(
    qindaqt_status_notifier_applet_model_tests
    tst_status_notifier_applet_model.cpp
)
target_link_libraries(
    qindaqt_status_notifier_applet_model_tests
    PRIVATE
        QindaQt::ShellStatusNotifierApplet
        QindaQt::StatusNotifier
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_status_notifier_applet_model_tests)
add_test(
    NAME qindaqt.status-notifier-applet-model
    COMMAND qindaqt_status_notifier_applet_model_tests
)
set_tests_properties(
    qindaqt.status-notifier-applet-model
    PROPERTIES LABELS "shell;status-notifier"
)

# 2. Controller unit tests (scripted seam)
qt_add_executable(
    qindaqt_status_notifier_applet_controller_tests
    tst_status_notifier_applet_controller.cpp
    status_notifier_applet_test_fakes.h
)
target_link_libraries(
    qindaqt_status_notifier_applet_controller_tests
    PRIVATE
        QindaQt::ShellStatusNotifierAppletRuntime
        QindaQt::StatusNotifierIcon
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_status_notifier_applet_controller_tests)
add_test(
    NAME qindaqt.status-notifier-applet-controller
    COMMAND qindaqt_status_notifier_applet_controller_tests
)
set_tests_properties(
    qindaqt.status-notifier-applet-controller
    PROPERTIES LABELS "shell;status-notifier"
)

# 3. Adapter tests: real registry + monitor + watcher composition over a
# private session bus (never the host bus).
qt_add_executable(
    qindaqt_status_notifier_applet_adapter_tests
    tst_status_notifier_applet_adapter.cpp
    ../status_notifier_fake_item_test_support.h
    ../status_notifier_private_bus_test_support.h
)
target_include_directories(
    qindaqt_status_notifier_applet_adapter_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(
    qindaqt_status_notifier_applet_adapter_tests
    PRIVATE
        QindaQt::ShellStatusNotifierAppletRuntime
        QindaQt::StatusNotifierWatcher
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_status_notifier_applet_adapter_tests)
add_test(
    NAME qindaqt.status-notifier-applet-adapter
    COMMAND qindaqt_status_notifier_applet_adapter_tests
)
set_tests_properties(
    qindaqt.status-notifier-applet-adapter
    PROPERTIES LABELS "shell;status-notifier"
)

# AGENT-GUARD: every QML row runs through the compiled harness executable
# whose build pulls these prerequisites, so a focused build of the declared
# test targets can never leave QML plugins unbuilt (a bare qmltestrunner row
# has no executable prerequisite at all).
add_custom_target(
    qindaqt_status_notifier_applet_qml_test_prerequisites
    DEPENDS
        qindaqt_shell_status_notifier_applet_runtimeplugin
        qindaqt_controls_qmlplugin
        qindaqt_tokens_qmlplugin
)

# 4. QML rows through the theme-published compiled harness. The applet's QML
# consumes QST-1 tokens that exist only after a theme is published; the
# harness publishes qinda-light before any test, giving the rows real layout
# for genuine pointer-event delivery and warning-free token evaluation. These
# rows run under QT_FATAL_WARNINGS=1: standalone rendering must be
# warning-clean.
qt_add_executable(qindaqt_status_notifier_applet_qml_tests qml_interactive_main.cpp)
target_link_libraries(
    qindaqt_status_notifier_applet_qml_tests
    PRIVATE
        qindaqt_controls_test_support
        QindaQt::ShellStatusNotifierAppletRuntime
        qindaqt_shell_status_notifier_applet_runtimeplugin
        Qt6::Core
        Qt6::Quick
        Qt6::QuickTest
        Qt6::Test
)
target_compile_features(qindaqt_status_notifier_applet_qml_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_status_notifier_applet_qml_tests PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_status_notifier_applet_qml_tests)
qt_import_qml_plugins(qindaqt_status_notifier_applet_qml_tests)
add_dependencies(
    qindaqt_status_notifier_applet_qml_tests
    qindaqt_status_notifier_applet_qml_test_prerequisites
)

function(qindaqt_add_status_notifier_applet_qml_test test_name test_input)
    add_test(
        NAME ${test_name}
        COMMAND qindaqt_status_notifier_applet_qml_tests -input "${test_input}"
    )
    set_tests_properties(
        ${test_name}
        PROPERTIES
            LABELS "offscreen"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
            TIMEOUT 300
    )
endfunction()

qindaqt_add_status_notifier_applet_qml_test(
    qindaqt.status-notifier-applet-qml-offscreen
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_StatusNotifierApplet.qml"
)
qindaqt_add_status_notifier_applet_qml_test(
    qindaqt.status-notifier-applet-qml-keyboard-offscreen
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_StatusNotifierAppletKeyboard.qml"
)
qindaqt_add_status_notifier_applet_qml_test(
    qindaqt.status-notifier-applet-qml-accessibility-offscreen
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_StatusNotifierAppletAccessibility.qml"
)

# 5. Boundary and source policy test
add_test(
    NAME qindaqt.status-notifier-applet-boundary-policy
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR=${CMAKE_SOURCE_DIR}/src/shell/status_notifier/applet"
        "-DQINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY=${CMAKE_BINARY_DIR}/tests/status_notifier_applet_poison_probe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_status_notifier_applet_boundary.cmake"
)

# 6. Installed component consumer/probe: validates the
# StatusNotifierAppletRuntime install rules end-to-end (public boundary,
# archives, QML module, plugin, manifest) from a fresh staged prefix,
# including the relocated-consumer RPATH truth.
add_test(
    NAME qindaqt.status-notifier-applet-installed-package
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-status-notifier-applet-stage"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        "-DQINDAQT_CONTROLS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Controls"
        "-DQINDAQT_TOKENS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens"
        "-DQINDAQT_CONTROLS_LIBRARY=$<TARGET_FILE:qindaqt_controls_qml>"
        "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
        "-DQINDAQT_STATUS_NOTIFIER_LIBRARY=$<TARGET_FILE:qindaqt_status_notifier>"
        "-DQINDAQT_STATUS_NOTIFIER_ITEM_CLIENT_LIBRARY=$<TARGET_FILE:qindaqt_status_notifier_item_client>"
        "-DQINDAQT_STATUS_NOTIFIER_ICON_LIBRARY=$<TARGET_FILE:qindaqt_status_notifier_icon>"
        "-DQINDAQT_THEMES_LIBRARY=$<TARGET_FILE:qindaqt_themes>"
        "-DQINDAQT_DESIGN_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_design_tokens>"
        "-DQINDAQT_STATUS_NOTIFIER_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src/shell/status_notifier/include"
        "-DQINDAQT_STATUS_NOTIFIER_ITEM_CLIENT_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src/shell/status_notifier/item_client/include"
        "-DQINDAQT_STATUS_NOTIFIER_ICON_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src/shell/status_notifier/icon/include"
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
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_status_notifier_applet.cmake"
)
set_tests_properties(
    qindaqt.status-notifier-applet-installed-package
    PROPERTIES LABELS "package" RUN_SERIAL TRUE TIMEOUT 300
)
