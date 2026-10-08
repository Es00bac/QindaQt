# SPDX-License-Identifier: GPL-3.0-or-later
foreach(row IN ITEMS model catalog)
    if(row STREQUAL "model")
        set(source tst_printing_settings_model.cpp)
    else()
        set(source tst_printing_tool_catalog.cpp)
    endif()
    qt_add_executable(qindaqt_settings_printing_${row}_tests ${source} printing_test_support.h)
    target_link_libraries(qindaqt_settings_printing_${row}_tests PRIVATE
        qindaqt_settings_printing Qt6::Test)
    qindaqt_enable_warnings(qindaqt_settings_printing_${row}_tests)
    add_test(NAME qindaqt.settings-printing-${row}
        COMMAND qindaqt_settings_printing_${row}_tests)
    set_tests_properties(qindaqt.settings-printing-${row} PROPERTIES
        LABELS "settings;printing;${row}"
        ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")
endforeach()

qt_add_executable(qindaqt_settings_printing_page_tests tst_printing_page.cpp printing_test_support.h)
target_compile_definitions(qindaqt_settings_printing_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_PRINTING_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/printing/qml/PrintersScannersPage.qml")
target_link_libraries(qindaqt_settings_printing_page_tests PRIVATE
    qindaqt_settings_printing qindaqt_settings_appearance QindaQt::TokensQml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_settings_printing_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_printing_qmlplugin)
qindaqt_enable_warnings(qindaqt_settings_printing_page_tests)
set(printing_page_environment
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")
add_test(NAME qindaqt.settings-printing-page COMMAND qindaqt_settings_printing_page_tests)
set_tests_properties(qindaqt.settings-printing-page PROPERTIES
    LABELS "offscreen;settings;printing;keyboard;layout"
    ENVIRONMENT "${printing_page_environment}")
add_test(NAME qindaqt.settings-printing-page-dpi2 COMMAND qindaqt_settings_printing_page_tests)
set_tests_properties(qindaqt.settings-printing-page-dpi2 PROPERTIES
    LABELS "offscreen;settings;printing;keyboard;layout;dpi2"
    ENVIRONMENT "${printing_page_environment};QT_SCALE_FACTOR=2")

add_test(NAME qindaqt.settings-printing-installed-route
    COMMAND "${CMAKE_COMMAND}"
        -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
        -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-printing-stage
        -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
        -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
        -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
        -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
        -DCONFIGURATION=$<CONFIG>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_printing_route.cmake")
set_tests_properties(qindaqt.settings-printing-installed-route PROPERTIES
    LABELS "offscreen;settings;printing;package;poison" RUN_SERIAL TRUE TIMEOUT 90)
