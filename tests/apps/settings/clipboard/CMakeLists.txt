# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_clipboard_settings_preference_tests
    tst_clipboard_settings_preference.cpp clipboard_settings_test_support.h
)
target_link_libraries(
    qindaqt_clipboard_settings_preference_tests
    PRIVATE qindaqt_settings_clipboard Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_settings_preference_tests)
add_test(NAME qindaqt.settings-clipboard-preference
         COMMAND qindaqt_clipboard_settings_preference_tests)
set_tests_properties(qindaqt.settings-clipboard-preference PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;clipboard;model")

qt_add_executable(
    qindaqt_clipboard_settings_service_tests
    tst_clipboard_settings_service.cpp clipboard_settings_test_support.h
)
target_link_libraries(
    qindaqt_clipboard_settings_service_tests
    PRIVATE qindaqt_settings_clipboard Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_settings_service_tests)
add_test(NAME qindaqt.settings-clipboard-service
         COMMAND qindaqt_clipboard_settings_service_tests)
set_tests_properties(qindaqt.settings-clipboard-service PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;clipboard;model;adversarial")

qt_add_executable(
    qindaqt_clipboard_page_tests
    tst_clipboard_page.cpp stub_clipboard_settings_model.h
)
target_compile_definitions(
    qindaqt_clipboard_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_CLIPBOARD_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/clipboard/qml/ClipboardPage.qml"
)
target_link_libraries(
    qindaqt_clipboard_page_tests PRIVATE
    qindaqt_settings_clipboard qindaqt_settings_appearance
    QindaQt::TokensQml qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_clipboard_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_clipboard_qmlplugin
)
qindaqt_enable_warnings(qindaqt_clipboard_page_tests)
add_test(NAME qindaqt.settings-clipboard-page COMMAND qindaqt_clipboard_page_tests)
set_tests_properties(qindaqt.settings-clipboard-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;clipboard;accessibility")

add_test(NAME qindaqt.settings-clipboard-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.settings-clipboard-boundary PROPERTIES
    LABELS "settings;clipboard;boundary")

add_test(NAME qindaqt.settings-clipboard-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake")
set_tests_properties(qindaqt.settings-clipboard-boundary-poison PROPERTIES
    LABELS "settings;clipboard;boundary;poison")

add_test(NAME qindaqt.settings-clipboard-installed-route
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-clipboard-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake")
set_tests_properties(qindaqt.settings-clipboard-installed-route PROPERTIES
    LABELS "offscreen;settings;clipboard;package" RUN_SERIAL TRUE TIMEOUT 45)
