# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_power_client_tests
    tst_power_client.cpp
    support/fake_power_transport.h
)
target_link_libraries(
    qindaqt_power_client_tests PRIVATE QindaQt::PowerClient Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_client_tests)
add_test(NAME qindaqt.power-client COMMAND qindaqt_power_client_tests)
set_tests_properties(
    qindaqt.power-client PROPERTIES TIMEOUT 20 LABELS "service;power"
)

qt_add_executable(
    qindaqt_power_qt_transport_tests
    tst_qt_power_transport.cpp
    ../power_service/support/fake_power_collaborators.h
)
target_link_libraries(
    qindaqt_power_qt_transport_tests
    PRIVATE QindaQt::PowerClient QindaQt::PowerService Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_qt_transport_tests)
add_test(NAME qindaqt.power-qt-transport COMMAND qindaqt_power_qt_transport_tests)
set_tests_properties(
    qindaqt.power-qt-transport
    PROPERTIES TIMEOUT 30 LABELS "service;power;dbus" RUN_SERIAL TRUE
)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
qt_add_executable(qindaqt_power_activation_tests tst_power_activation.cpp)
target_link_libraries(
    qindaqt_power_activation_tests
    PRIVATE QindaQt::PowerClient Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_power_activation_tests
    PRIVATE
        QINDAQT_POWER_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-power-service>"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_power_activation_tests)
add_dependencies(qindaqt_power_activation_tests qindaqt-power-service)
add_test(NAME qindaqt.power-activation COMMAND qindaqt_power_activation_tests)
set_tests_properties(
    qindaqt.power-activation
    PROPERTIES LABELS "service;power;dbus;activation" TIMEOUT 60 RUN_SERIAL TRUE
)

add_test(
    NAME qindaqt.power-installed-package
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-power-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_power_consumer.cpp"
        "-DQINDAQT_POWER_CLIENT_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/power_client"
        "-DQINDAQT_POWER_SERVICE_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/power_service"
        "-DQINDAQT_POWER_PROTOCOL_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/power_protocol"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_POWER_CLIENT_LIBRARY=$<TARGET_FILE:qindaqt_power_client>"
        "-DQINDAQT_POWER_SERVICE_LIBRARY=$<TARGET_FILE:qindaqt_power_service>"
        "-DQINDAQT_POWER_PROTOCOL_LIBRARY=$<TARGET_FILE:qindaqt_power_protocol>"
        "-DQINDAQT_POWER_SERVICE_EXECUTABLE=$<TARGET_FILE:qindaqt-power-service>"
        "-DQINDAQT_EXPECTED_INSTALL_EXECUTABLE=${KDE_INSTALL_FULL_BINDIR}/qindaqt-power-service"
        "-DQINDAQT_DBUS_SERVICE_FILE=${CMAKE_BINARY_DIR}/src/services/power_service/org.qindaqt.Power1.service"
        "-DQINDAQT_SYSTEMD_UNIT_FILE=${CMAKE_BINARY_DIR}/src/services/power_service/qindaqt-power-service.service"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_power_package.cmake"
)
set_tests_properties(
    qindaqt.power-installed-package
    PROPERTIES LABELS "service;power;installed" TIMEOUT 120
)
