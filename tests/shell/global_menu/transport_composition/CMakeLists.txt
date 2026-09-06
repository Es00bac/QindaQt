# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

function(add_global_menu_transport_composition_test target source test_name)
    qt_add_executable(
        ${target}
        ../dbusmenu/fake_dbusmenu_exporter.h
        transport_composition_fixture.h
        ${source}
    )
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../dbusmenu")
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::GlobalMenuTransportComposition
            QindaQt::GlobalMenuRegistrar
            QindaQt::GlobalMenuDbusMenu
            Qt6::Core
            Qt6::DBus
            Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(
        NAME ${test_name}
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:${target}>
    )
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 20)
endfunction()

add_global_menu_transport_composition_test(
    qindaqt_global_menu_transport_registration_tests
    tst_global_menu_transport_registration.cpp
    qindaqt.global-menu-transport-registration-private-bus
)
add_global_menu_transport_composition_test(
    qindaqt_global_menu_transport_churn_tests
    tst_global_menu_transport_churn.cpp
    qindaqt.global-menu-transport-churn-private-bus
)
