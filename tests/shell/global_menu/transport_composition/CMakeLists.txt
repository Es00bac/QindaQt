# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

qt_add_executable(
    qindaqt_global_menu_transport_composition_tests
    ../dbusmenu/fake_dbusmenu_exporter.h
    tst_global_menu_transport_composition.cpp
)
target_include_directories(
    qindaqt_global_menu_transport_composition_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../dbusmenu"
)
target_link_libraries(
    qindaqt_global_menu_transport_composition_tests
    PRIVATE
        QindaQt::GlobalMenuTransportComposition
        QindaQt::GlobalMenuRegistrar
        QindaQt::GlobalMenuDbusMenu
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_transport_composition_tests)
add_test(
    NAME qindaqt.global-menu-transport-composition-private-bus
    COMMAND "${QINDAQT_DBUS_RUN_SESSION}" --
            $<TARGET_FILE:qindaqt_global_menu_transport_composition_tests>
)
set_tests_properties(
    qindaqt.global-menu-transport-composition-private-bus PROPERTIES TIMEOUT 20
)
