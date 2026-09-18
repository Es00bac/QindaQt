# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus WebSockets)
find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

# The obs-websocket v5 wire, as pure functions: no socket, no OBS.
qt_add_executable(qindaqt_obs_protocol_tests tst_obs_protocol.cpp)
target_link_libraries(qindaqt_obs_protocol_tests
    PRIVATE qindaqt_obs_client Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_protocol_tests)
add_test(NAME qindaqt.services-obs-protocol
    COMMAND qindaqt_obs_protocol_tests)
set_tests_properties(qindaqt.services-obs-protocol PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;obs;model")

# The client state machine over a fake transport: no socket, no OBS, no wait.
qt_add_executable(qindaqt_obs_client_tests tst_obs_client.cpp)
target_link_libraries(qindaqt_obs_client_tests
    PRIVATE qindaqt_obs_client Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_client_tests)
add_test(NAME qindaqt.services-obs-client
    COMMAND qindaqt_obs_client_tests)
set_tests_properties(qindaqt.services-obs-client PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;obs;model")

# The production QWebSocket transport against a miniature obs-websocket
# server on a private loopback port: the only row here that opens a socket.
qt_add_executable(qindaqt_qt_obs_transport_tests tst_qt_obs_transport.cpp)
target_link_libraries(qindaqt_qt_obs_transport_tests
    PRIVATE qindaqt_obs_client Qt6::Test Qt6::WebSockets)
qindaqt_enable_warnings(qindaqt_qt_obs_transport_tests)
add_test(NAME qindaqt.services-obs-transport
    COMMAND qindaqt_qt_obs_transport_tests)
set_tests_properties(qindaqt.services-obs-transport PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;obs;network" TIMEOUT 60 RUN_SERIAL TRUE)

# Provisioning documents and the writer, into a temporary root: no OBS, and
# nothing ever written near the user own configuration.
qt_add_executable(qindaqt_obs_provisioning_tests tst_obs_provisioning.cpp)
target_link_libraries(qindaqt_obs_provisioning_tests
    PRIVATE qindaqt_obs_client Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_provisioning_tests)
add_test(NAME qindaqt.services-obs-provisioning
    COMMAND qindaqt_obs_provisioning_tests)
set_tests_properties(qindaqt.services-obs-provisioning PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;obs;model")

# The scoped obs-websocket secret against a miniature org.freedesktop.secrets
# on a private bus: no row touches the user real keyring.
qt_add_executable(qindaqt_obs_secret_store_tests tst_obs_secret_store.cpp)
target_compile_definitions(qindaqt_obs_secret_store_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}")
target_link_libraries(qindaqt_obs_secret_store_tests
    PRIVATE qindaqt_obs_client Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_secret_store_tests)
add_test(NAME qindaqt.services-obs-secret-store
    COMMAND qindaqt_obs_secret_store_tests)
set_tests_properties(qindaqt.services-obs-secret-store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;obs;dbus" TIMEOUT 60 RUN_SERIAL TRUE)
