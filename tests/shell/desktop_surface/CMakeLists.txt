# SPDX-License-Identifier: GPL-3.0-or-later

# Desktop surface tests: the new-folder controller over a redirected
# QStandardPaths HOME, the controller's resolution and zero-window contract
# over the real manifest catalog and capability policy, and the compiled QML
# module offscreen over stub facades. Nothing here reaches a compositor, a
# session bus, or the user's real Desktop directory.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)

set(desktop_surface_test_environment
    "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

if(NOT QINDAQT_SOURCE_ROOT)
    set(QINDAQT_SOURCE_ROOT "${PROJECT_SOURCE_DIR}")
endif()

function(qindaqt_add_desktop_surface_test target source test_name)
    cmake_parse_arguments(ARG "OFFSCREEN" "" "SUPPORT" ${ARGN})
    qt_add_executable(${target} ${source} ${ARG_SUPPORT})
    target_compile_definitions(
        ${target}
        PRIVATE
            QINDAQT_SOURCE_DIR="${QINDAQT_SOURCE_ROOT}"
    )
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellDesktopSurfaceQml
            qindaqt_desktop_surface_qmlplugin
            QindaQt::ShellIcons
            qindaqt_shell_iconsplugin
            QindaQt::TokensQml
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
            Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(
        ${test_name}
        PROPERTIES
            LABELS "shell;desktop-surface"
            ENVIRONMENT_MODIFICATION "${desktop_surface_test_environment}"
    )
    if(ARG_OFFSCREEN)
        set_tests_properties(
            ${test_name}
            PROPERTIES
                ENVIRONMENT
                    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
        )
        target_compile_definitions(
            ${target}
            PRIVATE
                QINDAQT_DESKTOP_SURFACE_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        )
        add_dependencies(
            ${target}
            qindaqt_desktop_surface_qmlplugin
            qindaqt_shell_iconsplugin
            qindaqt_tokens_qmlplugin
        )
        qt_import_qml_plugins(${target})
    endif()
endfunction()

qindaqt_add_desktop_surface_test(qindaqt_desktop_surface_new_folder_tests
    tst_new_folder_controller.cpp qindaqt.desktop-surface-new-folder)
qindaqt_add_desktop_surface_test(qindaqt_desktop_surface_controller_tests
    tst_desktop_surface_controller.cpp qindaqt.desktop-surface-controller)
# The controller maps QQuickWindows, so the lane needs the offscreen platform
# on headless builders. QT_FATAL_WARNINGS deliberately stays unset: the
# layer-shell "not a wayland window" warning is the expected off-Wayland path.
set_tests_properties(
    qindaqt.desktop-surface-controller
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)
qindaqt_add_desktop_surface_test(
    qindaqt_desktop_surface_qml_tests
    tst_desktop_surface_qml.cpp
    qindaqt.desktop-surface-offscreen-qml
    SUPPORT desktop_surface_qml_test_support.h
    OFFSCREEN
)
