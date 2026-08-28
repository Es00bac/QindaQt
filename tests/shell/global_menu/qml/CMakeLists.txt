# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: Use Qt's imported runner target. A bare `qmltestrunner` resolves
# to Qt 5 on mixed-Qt distributions and cannot load this Qt 6 module.
find_package(Qt6 REQUIRED COMPONENTS QuickTest)

add_test(
    NAME qindaqt.global-menu-applet-qml-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuApplet.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-qml-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

add_test(
    NAME qindaqt.global-menu-applet-qml-overflow-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_GlobalMenuAppletOverflow.qml"
)
set_tests_properties(
    qindaqt.global-menu-applet-qml-overflow-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)
