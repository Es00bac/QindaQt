# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_startup_test target)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    qt_add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE qindaqt_settings_startup Qt6::Core Qt6::Test)
    qindaqt_enable_warnings(${target})
endfunction()

qindaqt_startup_test(
    qindaqt_settings_startup_store_test
    SOURCES tst_xdg_autostart_store.cpp
)
add_test(NAME qindaqt.settings-startup-store
         COMMAND qindaqt_settings_startup_store_test)
set_tests_properties(
    qindaqt.settings-startup-store PROPERTIES
    LABELS "settings;startup" TIMEOUT 60
)

qindaqt_startup_test(
    qindaqt_settings_startup_model_test
    SOURCES tst_startup_settings_model.cpp
)
add_test(NAME qindaqt.settings-startup-model
         COMMAND qindaqt_settings_startup_model_test)
set_tests_properties(
    qindaqt.settings-startup-model PROPERTIES
    LABELS "settings;startup" TIMEOUT 60
)

qt_add_executable(qindaqt_settings_startup_page_test tst_startup_page.cpp)
target_compile_definitions(qindaqt_settings_startup_page_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_STARTUP_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/startup/qml/StartupPage.qml")
target_link_libraries(qindaqt_settings_startup_page_test PRIVATE
    qindaqt_settings_startup qindaqt_settings_appearance
    QindaQt::TokensQml qindaqt_themes
    Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_settings_startup_page_test
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
    qindaqt_settings_startup_qmlplugin)
qindaqt_enable_warnings(qindaqt_settings_startup_page_test)
add_test(NAME qindaqt.settings-startup-page
    COMMAND qindaqt_settings_startup_page_test)
set_tests_properties(qindaqt.settings-startup-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;startup;accessibility")
