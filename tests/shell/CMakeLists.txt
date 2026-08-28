# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: Use Qt's imported runner target. A bare `qmltestrunner` resolves
# to Qt 5 on mixed-Qt distributions and cannot load this Qt 6 module.
find_package(Qt6 REQUIRED COMPONENTS QuickTest)
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

qt_add_executable(
    qindaqt_notification_quieting_bridge_tests
    tst_notificationquietingsettingsbridge.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationquietingsettingsbridge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationquietingsettingsbridge.h"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/settingsroutelauncher.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/settingsroutelauncher.h"
)
target_include_directories(
    qindaqt_notification_quieting_bridge_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
)
target_link_libraries(
    qindaqt_notification_quieting_bridge_tests
    PRIVATE
        QindaQt::SettingsClient
        QindaQt::SettingsService
        QindaQt::NotificationPresentationPolicy
        Qt6::DBus
        Qt6::Test
)
target_compile_definitions(
    qindaqt_notification_quieting_bridge_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_notification_quieting_bridge_tests)
add_test(
    NAME qindaqt.notification-quieting-settings-bridge
    COMMAND qindaqt_notification_quieting_bridge_tests
)

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

add_test(
    NAME qindaqt.notification-focus-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationfocus.qml"
)

add_test(
    NAME qindaqt.notification-quieting-controls-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationquietingcontrols.qml"
)

add_test(
    NAME qindaqt.notification-center-applet-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationcenterapplet.qml"
)

set_tests_properties(
    qindaqt.decoration-hover
    qindaqt.notification-surfaces-offscreen
    qindaqt.notification-focus-offscreen
    qindaqt.notification-quieting-controls-offscreen
    qindaqt.notification-center-applet-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

if(TARGET qindaqt-shell)
    qt_add_executable(
        qindaqt_notification_center_entry_tests
        tst_notificationcenterentry.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcenterappletaccess.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcenterappletaccess.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcentershortcut.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcentershortcut.h"
    )
    target_include_directories(
        qindaqt_notification_center_entry_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_notification_center_entry_tests
        PRIVATE Qt6::Gui Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_notification_center_entry_tests)
    add_test(
        NAME qindaqt.notification-center-entry
        COMMAND qindaqt_notification_center_entry_tests
    )
    set_tests_properties(
        qindaqt.notification-center-entry
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(
        NAME qindaqt.shell-runtime-catalog
        COMMAND $<TARGET_FILE:qindaqt-shell> --list
    )
    set_tests_properties(
        qindaqt.shell-runtime-catalog
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
            PASS_REGULAR_EXPRESSION "notification-center - Notification Center"
    )
endif()
