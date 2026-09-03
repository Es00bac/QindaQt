# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

qt_add_executable(
    qindaqt_global_menu_dbusmenu_decoder_tests
    fake_dbusmenu_exporter.h
    tst_dbusmenu_decoder.cpp
)
target_link_libraries(
    qindaqt_global_menu_dbusmenu_decoder_tests
    PRIVATE QindaQt::GlobalMenuDbusMenu QindaQt::GlobalMenuProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_dbusmenu_decoder_tests)
add_test(
    NAME qindaqt.global-menu-dbusmenu-decoder
    COMMAND qindaqt_global_menu_dbusmenu_decoder_tests
)

qt_add_executable(
    qindaqt_global_menu_dbusmenu_server_tests
    tst_dbusmenu_server.cpp
)
target_link_libraries(
    qindaqt_global_menu_dbusmenu_server_tests
    PRIVATE QindaQt::GlobalMenuDbusMenu QindaQt::GlobalMenuProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_dbusmenu_server_tests)
add_test(
    NAME qindaqt.global-menu-dbusmenu-server
    COMMAND qindaqt_global_menu_dbusmenu_server_tests
)

qt_add_executable(
    qindaqt_global_menu_dbusmenu_client_tests
    fake_dbusmenu_exporter.h
    tst_dbusmenu_client.cpp
)
target_link_libraries(
    qindaqt_global_menu_dbusmenu_client_tests
    PRIVATE QindaQt::GlobalMenuDbusMenu QindaQt::GlobalMenuProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_dbusmenu_client_tests)
add_test(
    NAME qindaqt.global-menu-dbusmenu-private-bus
    COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_global_menu_dbusmenu_client_tests>
)
set_tests_properties(qindaqt.global-menu-dbusmenu-private-bus PROPERTIES TIMEOUT 20)
