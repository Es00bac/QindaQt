# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_about_computer_info_tests
    tst_about_computer_info.cpp
)
target_link_libraries(
    qindaqt_about_computer_info_tests
    PRIVATE qindaqt_settings_about_computer Qt6::Test
)
qindaqt_enable_warnings(qindaqt_about_computer_info_tests)
add_test(NAME qindaqt.settings-about-computer-info
         COMMAND qindaqt_about_computer_info_tests)
set_tests_properties(qindaqt.settings-about-computer-info PROPERTIES
    LABELS "settings;about-computer;model")

qt_add_executable(
    qindaqt_about_computer_settings_model_tests
    tst_about_computer_settings_model.cpp
)
target_link_libraries(
    qindaqt_about_computer_settings_model_tests
    PRIVATE qindaqt_settings_about_computer Qt6::Gui Qt6::Test
)
qindaqt_enable_warnings(qindaqt_about_computer_settings_model_tests)
add_test(NAME qindaqt.settings-about-computer-model
         COMMAND qindaqt_about_computer_settings_model_tests)
set_tests_properties(qindaqt.settings-about-computer-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "offscreen;settings;about-computer;model")

qt_add_executable(
    qindaqt_about_computer_page_tests
    tst_about_computer_page.cpp
)
target_compile_definitions(
    qindaqt_about_computer_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_ABOUT_COMPUTER_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/about_computer/qml/AboutComputerPage.qml"
)
target_link_libraries(
    qindaqt_about_computer_page_tests PRIVATE
    qindaqt_settings_about_computer qindaqt_settings_appearance
    QindaQt::TokensQml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_about_computer_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_about_computer_qmlplugin
)
qindaqt_enable_warnings(qindaqt_about_computer_page_tests)
add_test(NAME qindaqt.settings-about-computer-page
         COMMAND qindaqt_about_computer_page_tests)
set_tests_properties(qindaqt.settings-about-computer-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;about-computer;accessibility")

add_test(NAME qindaqt.settings-about-computer-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.settings-about-computer-boundary PROPERTIES
    LABELS "settings;about-computer;boundary")

add_test(NAME qindaqt.settings-about-computer-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake")
set_tests_properties(qindaqt.settings-about-computer-boundary-poison PROPERTIES
    LABELS "settings;about-computer;boundary;poison")

add_test(NAME qindaqt.settings-about-computer-installed-route
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-about-computer-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake")
set_tests_properties(qindaqt.settings-about-computer-installed-route PROPERTIES
    LABELS "offscreen;settings;about-computer;package" RUN_SERIAL TRUE TIMEOUT 45)
