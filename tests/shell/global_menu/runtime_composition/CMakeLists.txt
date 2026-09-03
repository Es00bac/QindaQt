# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

qt_add_executable(
    qindaqt_global_menu_hostile_provider
    ../dbusmenu/fake_dbusmenu_exporter.h
    hostile_menu_provider.cpp
)
target_include_directories(
    qindaqt_global_menu_hostile_provider
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../dbusmenu"
)
target_link_libraries(
    qindaqt_global_menu_hostile_provider
    PRIVATE
        QindaQt::GlobalMenuDbusMenu
        QindaQt::GlobalMenuRegistrar
        Qt6::Core
        Qt6::DBus
)
qindaqt_enable_warnings(qindaqt_global_menu_hostile_provider)

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
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_HOSTILE_PROVIDER="$<TARGET_FILE:qindaqt_global_menu_hostile_provider>"
)
add_dependencies(
    qindaqt_global_menu_runtime_composition_tests
    qindaqt_global_menu_hostile_provider
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

qt_add_executable(
    qindaqt_file_manager_menu_export_tests
    tst_file_manager_menu_export.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.cpp"
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.h"
)
target_include_directories(
    qindaqt_file_manager_menu_export_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/runtime"
)
target_compile_definitions(
    qindaqt_file_manager_menu_export_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_BUILD_DIR="${CMAKE_BINARY_DIR}"
        QINDAQT_TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/file-manager-data"
        QINDAQT_FILE_MANAGER="$<TARGET_FILE:qindaqt-file-manager>"
)
add_dependencies(qindaqt_file_manager_menu_export_tests qindaqt-file-manager)
target_link_libraries(
    qindaqt_file_manager_menu_export_tests
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
qindaqt_enable_warnings(qindaqt_file_manager_menu_export_tests)
add_test(
    NAME qindaqt.file-manager-global-menu-shell-private-bus
    COMMAND "${QINDAQT_DBUS_RUN_SESSION}" --
            $<TARGET_FILE:qindaqt_file_manager_menu_export_tests>
)
set_tests_properties(
    qindaqt.file-manager-global-menu-shell-private-bus
    PROPERTIES
        TIMEOUT 30
        LABELS "file-manager;shell;global-menu;runtime;private-bus;offscreen"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)
