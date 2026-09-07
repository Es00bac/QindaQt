# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_executable(qindaqt_powerdevil_idle_tests tst_powerdevil_idle_adapter.cpp)
target_link_libraries(
    qindaqt_powerdevil_idle_tests
    PRIVATE
        QindaQt::PowerDevilIdle
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_powerdevil_idle_tests)

add_test(
    NAME qindaqt.session-powerdevil-idle
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_powerdevil_idle_tests>
)
set_tests_properties(
    qindaqt.session-powerdevil-idle
    PROPERTIES
        TIMEOUT 25
        LABELS "session;power;dbus"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)
