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
