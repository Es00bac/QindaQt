# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_shell_window_actions_tests tst_shellwindowactions.cpp)
target_link_libraries(
    qindaqt_shell_window_actions_tests
    PRIVATE QindaQt::CompositorShellActions Qt6::Test
)
add_test(NAME compositor.shell-window-actions COMMAND qindaqt_shell_window_actions_tests)

qt_add_executable(qindaqt_shell_window_identity_tests tst_shellwindowidentity.cpp)
target_link_libraries(
    qindaqt_shell_window_identity_tests
    PRIVATE QindaQt::CompositorShellActions Qt6::Test
)
add_test(NAME compositor.shell-window-identity COMMAND qindaqt_shell_window_identity_tests)

if(TARGET qindaqt_compositor AND TARGET qindaqt-wm)
    find_package(LayerShellQt 6.6.6 REQUIRED)
    find_package(KWayland 6.6.6 EXACT REQUIRED)
    find_program(QINDAQT_SHELL_ACTIONS_DBUS_RUN_SESSION dbus-run-session)
    if(QINDAQT_SHELL_ACTIONS_DBUS_RUN_SESSION)
        qt_add_executable(
            qindaqt_shell_window_actions_live_probe
            shellwindowactionsliveprobe.cpp
            shellwindowactionsliveclients.cpp
            shellwindowactionsliveclients.h
            shellwindowactionslivecontract.cpp
            shellwindowactionslivecontract.h
        )
        target_link_libraries(
            qindaqt_shell_window_actions_live_probe
            PRIVATE
                QindaQt::ShellWindowActionsClient
                LayerShellQt::Interface
                Plasma::KWaylandClient
                Qt6::DBus
                Qt6::Gui
        )
        set_target_properties(
            qindaqt_shell_window_actions_live_probe
            PROPERTIES CXX_EXTENSIONS OFF
        )
        target_compile_definitions(
            qindaqt_shell_window_actions_live_probe
            PRIVATE
                QINDAQT_COMPOSITOR_SHELL_DESCRIPTOR=\"${PROJECT_SOURCE_DIR}/compositor/dbus/org.qindaqt.CompositorShell1.xml\"
        )
        qindaqt_enable_warnings(qindaqt_shell_window_actions_live_probe)
        add_dependencies(
            qindaqt_shell_window_actions_live_probe qindaqt_compositor qindaqt-wm
        )
        add_test(
            NAME compositor.kwin-shell-window-actions
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_LIST_DIR}/test_shell_window_actions_nested.py"
                "$<TARGET_FILE:qindaqt-wm>"
                "$<TARGET_FILE:qindaqt_shell_window_actions_live_probe>"
                "${QINDAQT_SHELL_ACTIONS_DBUS_RUN_SESSION}"
                "${QINDAQT_KWIN_WAYLAND}"
                --plugin-root "${CMAKE_BINARY_DIR}/plugins"
                --scratch-root "${CMAKE_BINARY_DIR}"
        )
        set_tests_properties(
            compositor.kwin-shell-window-actions
            PROPERTIES
                TIMEOUT 35
                RUN_SERIAL TRUE
                LABELS "integration;compositor;security;wayland"
        )
    endif()
endif()
