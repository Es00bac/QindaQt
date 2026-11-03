# SPDX-License-Identifier: GPL-3.0-or-later
# QindaLink desktop-side tests. Rows are named qindaqt.link-* and use only
# QTemporaryDir state and ephemeral loopback ports (never 47160).

qt_add_executable(qindaqt_link_vectors_test tst_link_vectors.cpp)
target_link_libraries(qindaqt_link_vectors_test PRIVATE QindaQt::LinkProtocol Qt6::Test)
target_compile_definitions(
    qindaqt_link_vectors_test
    PRIVATE QINDAQT_LINK_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vectors"
)
qindaqt_enable_warnings(qindaqt_link_vectors_test)
add_test(NAME qindaqt.link-vectors COMMAND qindaqt_link_vectors_test)
set_tests_properties(qindaqt.link-vectors PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_pairing_test tst_link_pairing.cpp)
target_link_libraries(qindaqt_link_pairing_test PRIVATE QindaQt::LinkTransport Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_pairing_test)
add_test(NAME qindaqt.link-pairing COMMAND qindaqt_link_pairing_test)
set_tests_properties(qindaqt.link-pairing PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_identity_test tst_link_identity.cpp)
target_link_libraries(qindaqt_link_identity_test PRIVATE QindaQt::LinkTransport Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_identity_test)
add_test(NAME qindaqt.link-identity COMMAND qindaqt_link_identity_test)
set_tests_properties(qindaqt.link-identity PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_session_test tst_link_session.cpp)
target_link_libraries(qindaqt_link_session_test PRIVATE QindaQt::LinkTransport Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_session_test)
add_test(NAME qindaqt.link-session COMMAND qindaqt_link_session_test)
set_tests_properties(qindaqt.link-session PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_adb_test tst_link_adb.cpp)
target_link_libraries(qindaqt_link_adb_test PRIVATE QindaQt::LinkAdb Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_adb_test)
add_test(NAME qindaqt.link-adb COMMAND qindaqt_link_adb_test)
set_tests_properties(qindaqt.link-adb PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_client_test tst_link_client.cpp)
target_link_libraries(qindaqt_link_client_test PRIVATE QindaQt::LinkClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_client_test)
add_test(NAME qindaqt.link-client COMMAND qindaqt_link_client_test)
set_tests_properties(qindaqt.link-client PROPERTIES LABELS "service;link" TIMEOUT 30)

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session)
if(QINDAQT_DBUS_RUN_SESSION)
    qt_add_executable(qindaqt_link_service_smoke_test tst_link_service_smoke.cpp)
    target_link_libraries(
        qindaqt_link_service_smoke_test
        PRIVATE QindaQt::LinkClient QindaQt::LinkTransport Qt6::Test
    )
    target_compile_definitions(
        qindaqt_link_service_smoke_test
        PRIVATE QINDAQT_LINK_SERVICE_BINARY="$<TARGET_FILE:qindaqt-link-service>"
    )
    add_dependencies(qindaqt_link_service_smoke_test qindaqt-link-service)
    qindaqt_enable_warnings(qindaqt_link_service_smoke_test)
    add_test(
        NAME qindaqt.link-service-smoke
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_service_smoke_test>
    )
    set_tests_properties(qindaqt.link-service-smoke PROPERTIES LABELS "service;link" TIMEOUT 30)
endif()

qt_add_executable(qindaqt_link_bounds_test tst_link_bounds.cpp)
target_link_libraries(qindaqt_link_bounds_test PRIVATE QindaQt::LinkTransport Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_bounds_test)
add_test(NAME qindaqt.link-bounds COMMAND qindaqt_link_bounds_test)
set_tests_properties(qindaqt.link-bounds PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_presenter_test tst_link_presenter.cpp)
target_link_libraries(qindaqt_link_presenter_test PRIVATE QindaQt::LinkService Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_presenter_test)
add_test(NAME qindaqt.link-presenter COMMAND qindaqt_link_presenter_test)
set_tests_properties(qindaqt.link-presenter PROPERTIES LABELS "service;link" TIMEOUT 30)

qt_add_executable(qindaqt_link_notifications_test tst_link_notifications.cpp)
target_link_libraries(
    qindaqt_link_notifications_test
    PRIVATE QindaQt::LinkNotifications QindaQt::LinkTransport Qt6::Test
)
qindaqt_enable_warnings(qindaqt_link_notifications_test)
add_test(NAME qindaqt.link-notifications COMMAND qindaqt_link_notifications_test)
set_tests_properties(qindaqt.link-notifications PROPERTIES LABELS "service;link" TIMEOUT 60)
qt_add_executable(qindaqt_link_messages_test tst_link_messages.cpp)
target_link_libraries(qindaqt_link_messages_test PRIVATE QindaQt::LinkMessages Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_messages_test)
add_test(NAME qindaqt.link-messages COMMAND qindaqt_link_messages_test)
set_tests_properties(qindaqt.link-messages PROPERTIES LABELS "service;link" TIMEOUT 30)

if(QINDAQT_DBUS_RUN_SESSION)
    qt_add_executable(qindaqt_link_messages_dbus_test tst_link_messages_dbus.cpp)
    target_link_libraries(qindaqt_link_messages_dbus_test PRIVATE QindaQt::LinkMessages Qt6::Test)
    qindaqt_enable_warnings(qindaqt_link_messages_dbus_test)
    add_test(
        NAME qindaqt.link-messages-dbus
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_messages_dbus_test>
    )
    set_tests_properties(qindaqt.link-messages-dbus PROPERTIES LABELS "service;link" TIMEOUT 30)
endif()

if(QINDAQT_DBUS_RUN_SESSION)
    qt_add_executable(qindaqt_link_fdo_presenter_test tst_link_fdo_presenter.cpp)
    target_link_libraries(qindaqt_link_fdo_presenter_test PRIVATE QindaQt::LinkNotifications Qt6::Test)
    qindaqt_enable_warnings(qindaqt_link_fdo_presenter_test)
    add_test(
        NAME qindaqt.link-fdo-presenter
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_fdo_presenter_test>
    )
    set_tests_properties(qindaqt.link-fdo-presenter PROPERTIES LABELS "service;link" TIMEOUT 30)
endif()

# input.* and macro.* against a fake RemoteDesktop portal and fake InputActions1
# registered under unique names on the test bus; rows run under dbus-run-session
# so they never touch the user's bus. No row can reach the real portal.
if(QINDAQT_DBUS_RUN_SESSION)
    foreach(_t IN ITEMS input macros)
        qt_add_executable(
            qindaqt_link_${_t}_test
            tst_link_${_t}.cpp
            support/fake_portal.h
            support/fake_input_actions.h
            support/input_fixture.h
        )
        target_link_libraries(
            qindaqt_link_${_t}_test
            PRIVATE QindaQt::LinkInput QindaQt::LinkMacros Qt6::Test Qt6::DBus
        )
        qindaqt_enable_warnings(qindaqt_link_${_t}_test)
        add_test(
            NAME qindaqt.link-${_t}
            COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_${_t}_test>
        )
        set_tests_properties(qindaqt.link-${_t} PROPERTIES LABELS "service;link" TIMEOUT 60)
    endforeach()
endif()

qt_add_executable(qindaqt_link_caller_policy_test tst_link_caller_policy.cpp)
target_link_libraries(qindaqt_link_caller_policy_test PRIVATE QindaQt::LinkService Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_caller_policy_test)
add_test(NAME qindaqt.link-caller-policy COMMAND qindaqt_link_caller_policy_test)
set_tests_properties(qindaqt.link-caller-policy PROPERTIES LABELS "service;link" TIMEOUT 30)

if(QINDAQT_DBUS_RUN_SESSION)
    qt_add_executable(qindaqt_link_caller_dbus_test tst_link_caller_dbus.cpp)
    target_link_libraries(
        qindaqt_link_caller_dbus_test
        PRIVATE QindaQt::LinkTransport Qt6::DBus Qt6::Test
    )
    target_compile_definitions(
        qindaqt_link_caller_dbus_test
        PRIVATE QINDAQT_LINK_SERVICE_BINARY="$<TARGET_FILE:qindaqt-link-service>"
    )
    add_dependencies(qindaqt_link_caller_dbus_test qindaqt-link-service)
    qindaqt_enable_warnings(qindaqt_link_caller_dbus_test)
    add_test(
        NAME qindaqt.link-caller-dbus
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_caller_dbus_test>
    )
    set_tests_properties(qindaqt.link-caller-dbus PROPERTIES LABELS "service;link" TIMEOUT 40)

    qt_add_executable(qindaqt_link_caller_coverage_test tst_link_caller_coverage.cpp)
    target_link_libraries(
        qindaqt_link_caller_coverage_test
        PRIVATE QindaQt::LinkTransport Qt6::DBus Qt6::Test
    )
    target_compile_definitions(
        qindaqt_link_caller_coverage_test
        PRIVATE QINDAQT_LINK_SERVICE_BINARY="$<TARGET_FILE:qindaqt-link-service>"
    )
    add_dependencies(qindaqt_link_caller_coverage_test qindaqt-link-service)
    qindaqt_enable_warnings(qindaqt_link_caller_coverage_test)
    add_test(
        NAME qindaqt.link-caller-coverage
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_caller_coverage_test>
    )
    set_tests_properties(qindaqt.link-caller-coverage PROPERTIES LABELS "service;link" TIMEOUT 60)

    qt_add_executable(qindaqt_link_lan_service_test tst_link_lan_service.cpp)
    target_link_libraries(qindaqt_link_lan_service_test PRIVATE QindaQt::LinkTransport Qt6::DBus Qt6::Test)
    target_compile_definitions(
        qindaqt_link_lan_service_test
        PRIVATE QINDAQT_LINK_SERVICE_BINARY="$<TARGET_FILE:qindaqt-link-service>"
    )
    add_dependencies(qindaqt_link_lan_service_test qindaqt-link-service)
    qindaqt_enable_warnings(qindaqt_link_lan_service_test)
    add_test(
        NAME qindaqt.link-lan-service
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_lan_service_test>
    )
    set_tests_properties(qindaqt.link-lan-service PROPERTIES LABELS "service;link" TIMEOUT 40)
endif()

# LAN mode (ADR-0439): real Hub and TLS over a 127.0.0.2 "LAN" address, fake mDNS publisher.
qt_add_executable(qindaqt_link_lan_test tst_link_lan.cpp support/fake_publisher.h)
target_link_libraries(qindaqt_link_lan_test PRIVATE QindaQt::LinkService Qt6::Test)
qindaqt_enable_warnings(qindaqt_link_lan_test)
add_test(NAME qindaqt.link-lan COMMAND qindaqt_link_lan_test)
set_tests_properties(qindaqt.link-lan PROPERTIES LABELS "service;link" TIMEOUT 30)
