# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: Use Qt's imported runner target. A bare `qmltestrunner` resolves
# to Qt 5 on mixed-Qt distributions and cannot load this Qt 6 module.
find_package(Qt6 REQUIRED COMPONENTS QuickTest)

# The production-panel row imports every built-in applet eagerly. Give those
# unrelated modules their shared doubles while deliberately leaving
# GlobalMenu absent so this row resolves the real compiled implementation.
set(global_menu_panel_imports "${CMAKE_CURRENT_BINARY_DIR}/panel-imports")
file(REMOVE_RECURSE "${global_menu_panel_imports}/QindaQt")
file(COPY "${PROJECT_SOURCE_DIR}/tests/shell/qml/imports/QindaQt"
     DESTINATION "${global_menu_panel_imports}")
file(REMOVE_RECURSE
     "${global_menu_panel_imports}/QindaQt/Shell/GlobalMenu")

add_test(
    NAME qindaqt.global-menu-applet-qml-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuApplet.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-qml-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.global-menu-applet-qml-overflow-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuAppletOverflow.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-qml-overflow-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.global-menu-applet-qml-vertical-boundary-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuAppletVerticalBoundary.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-qml-vertical-boundary-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.global-menu-applet-qml-accessibility-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuAppletAccessibility.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-qml-accessibility-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.global-menu-applet-submenu-qml-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuAppletSubmenu.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-submenu-qml-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.global-menu-production-panel-keyboard-qml-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${PROJECT_SOURCE_DIR}/tests/shell/qml/imports"
        -import "${CMAKE_BINARY_DIR}/qml"
        -import "${global_menu_panel_imports}"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuProductionPanelKeyboard.qml"
)
set_tests_properties(
    qindaqt.global-menu-production-panel-keyboard-qml-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
        LABELS "shell;global-menu;runtime;offscreen;keyboard;hostile"
)
