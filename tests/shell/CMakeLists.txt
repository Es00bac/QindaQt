# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: Use Qt's imported runner target. A bare `qmltestrunner` resolves
# to Qt 5 on mixed-Qt distributions and cannot load this Qt 6 module.
find_package(Qt6 REQUIRED COMPONENTS QuickTest)

qt_add_executable(qindaqt_shell_capture_tests tst_shell_capture.cpp)

target_link_libraries(
    qindaqt_shell_capture_tests
    PRIVATE
        Qt6::Gui
        Qt6::Test
)

target_compile_definitions(
    qindaqt_shell_capture_tests
    PRIVATE QINDAQT_SHELL_PREVIEW_EXECUTABLE="$<TARGET_FILE:qindaqt-shell-preview>"
)

add_dependencies(qindaqt_shell_capture_tests qindaqt-shell-preview)
qindaqt_enable_warnings(qindaqt_shell_capture_tests)
add_test(NAME qindaqt.shell-capture-matrix COMMAND qindaqt_shell_capture_tests)

add_test(
    NAME qindaqt.decoration-hover
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_decorationbuttons.qml"
)

add_test(
    NAME qindaqt.notification-surfaces-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationsurfaces.qml"
)

set_tests_properties(
    qindaqt.decoration-hover
    qindaqt.notification-surfaces-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

if(TARGET qindaqt-shell)
    add_test(
        NAME qindaqt.shell-runtime-catalog
        COMMAND $<TARGET_FILE:qindaqt-shell> --list
    )
    set_tests_properties(
        qindaqt.shell-runtime-catalog
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
            PASS_REGULAR_EXPRESSION "clock - Clock"
    )
endif()
