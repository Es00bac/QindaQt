# SPDX-License-Identifier: GPL-3.0-or-later
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
qt_add_executable(qindaqt_settings_client_tests tst_settings_client.cpp)
target_link_libraries(qindaqt_settings_client_tests PRIVATE QindaQt::SettingsClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_settings_client_tests)
add_test(NAME qindaqt.settings-client COMMAND qindaqt_settings_client_tests)

qt_add_executable(qindaqt_do_not_disturb_controller_tests
                  tst_do_not_disturb_controller.cpp)
target_link_libraries(qindaqt_do_not_disturb_controller_tests
                      PRIVATE QindaQt::SettingsClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_do_not_disturb_controller_tests)
add_test(NAME qindaqt.settings-dnd-controller
         COMMAND qindaqt_do_not_disturb_controller_tests)

qt_add_executable(qindaqt_qt_settings_transport_tests tst_qt_settings_transport.cpp)
target_link_libraries(qindaqt_qt_settings_transport_tests PRIVATE QindaQt::SettingsClient QindaQt::SettingsService Qt6::Test Qt6::DBus)
target_compile_definitions(
    qindaqt_qt_settings_transport_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_qt_settings_transport_tests)
add_test(NAME qindaqt.settings-qt-transport COMMAND qindaqt_qt_settings_transport_tests)

qt_add_executable(qindaqt_qt_settings_transport_adversarial_tests
                  tst_qt_settings_transport_adversarial.cpp)
target_link_libraries(qindaqt_qt_settings_transport_adversarial_tests
                      PRIVATE QindaQt::SettingsClient Qt6::Test Qt6::DBus)
target_compile_definitions(
    qindaqt_qt_settings_transport_adversarial_tests PRIVATE
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_qt_settings_transport_adversarial_tests)
add_test(NAME qindaqt.settings-qt-transport-adversarial
         COMMAND qindaqt_qt_settings_transport_adversarial_tests)
