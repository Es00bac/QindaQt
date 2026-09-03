# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

qt_add_executable(qindaqt_global_menu_registrar_tests tst_appmenu_registrar.cpp)
target_link_libraries(
    qindaqt_global_menu_registrar_tests
    PRIVATE QindaQt::GlobalMenuRegistrar Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_registrar_tests)
add_test(
    NAME qindaqt.global-menu-registrar-private-bus
    COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_global_menu_registrar_tests>
)
set_tests_properties(qindaqt.global-menu-registrar-private-bus PROPERTIES TIMEOUT 20)
