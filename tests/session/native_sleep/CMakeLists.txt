# SPDX-License-Identifier: GPL-3.0-or-later
foreach(test IN ITEMS logind_sleep_transport sleep_coordinator sleep_modes)
  qt_add_executable(qindaqt_${test}_tests tst_${test}.cpp sleep_test_support.h)
  target_link_libraries(qindaqt_${test}_tests PRIVATE QindaQt::NativeSleep
    QindaQt::NativeLockService QindaQt::SessionLockState QindaQt::CompositorAttachment
    QindaQt::CompositorNames qindaqt_settings_screensaver Qt6::Test)
  target_compile_definitions(qindaqt_${test}_tests PRIVATE
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}")
  qindaqt_enable_warnings(qindaqt_${test}_tests)
  add_test(NAME qindaqt.${test} COMMAND qindaqt_${test}_tests)
  set_tests_properties(qindaqt.${test} PROPERTIES TIMEOUT 45 LABELS "session;lock;power;dbus"
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")
endforeach()
