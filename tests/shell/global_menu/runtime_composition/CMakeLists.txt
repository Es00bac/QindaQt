# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

qt_add_executable(
    qindaqt_global_menu_runtime_composition_tests
    ../dbusmenu/fake_dbusmenu_exporter.h
    tst_global_menu_runtime_composition.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.cpp"
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.h"
)
target_include_directories(
    qindaqt_global_menu_runtime_composition_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../dbusmenu"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime"
)
target_compile_definitions(
    qindaqt_global_menu_runtime_composition_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_global_menu_runtime_composition_tests
    PRIVATE
        QindaQt::AppletHost
        QindaQt::AppletRuntime
        QindaQt::GlobalMenuApplet
        QindaQt::GlobalMenuDbusMenu
        QindaQt::GlobalMenuRegistrar
        QindaQt::GlobalMenuTransportComposition
        QindaQt::ShellWindowActionsClient
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_runtime_composition_tests)
add_test(
    NAME qindaqt.global-menu-runtime-composition-private-bus
    COMMAND "${QINDAQT_DBUS_RUN_SESSION}" --
            $<TARGET_FILE:qindaqt_global_menu_runtime_composition_tests>
)
set_tests_properties(
    qindaqt.global-menu-runtime-composition-private-bus
    PROPERTIES TIMEOUT 20 LABELS "shell;global-menu;runtime;private-bus;hostile"
)
