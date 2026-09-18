# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_windows_settings_model_tests
    tst_windows_settings_model.cpp windows_settings_test_support.h
)
target_link_libraries(
    qindaqt_windows_settings_model_tests
    PRIVATE qindaqt_settings_windows Qt6::Test
)
qindaqt_enable_warnings(qindaqt_windows_settings_model_tests)
add_test(NAME qindaqt.settings-windows-model
         COMMAND qindaqt_windows_settings_model_tests)
set_tests_properties(qindaqt.settings-windows-model PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;windows;model")

qt_add_executable(
    qindaqt_windows_page_tests
    tst_windows_page.cpp stub_windows_settings_model.h
)
target_compile_definitions(
    qindaqt_windows_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_WINDOWS_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/windows/qml/WindowsPage.qml"
)
target_link_libraries(
    qindaqt_windows_page_tests PRIVATE
    qindaqt_settings_windows qindaqt_settings_appearance
    QindaQt::TokensQml qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_windows_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_windows_qmlplugin
)
qindaqt_enable_warnings(qindaqt_windows_page_tests)
add_test(NAME qindaqt.settings-windows-page
         COMMAND qindaqt_windows_page_tests)
set_tests_properties(qindaqt.settings-windows-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;windows")

add_test(NAME qindaqt.settings-windows-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.settings-windows-boundary PROPERTIES
    LABELS "settings;windows;boundary")

add_test(NAME qindaqt.settings-windows-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake")
set_tests_properties(qindaqt.settings-windows-boundary-poison PROPERTIES
    LABELS "settings;windows;boundary;poison")

add_test(NAME qindaqt.settings-windows-installed-route
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-windows-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake")
set_tests_properties(qindaqt.settings-windows-installed-route PROPERTIES
    LABELS "offscreen;settings;windows;package" RUN_SERIAL TRUE TIMEOUT 45)
