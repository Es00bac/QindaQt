# SPDX-License-Identifier: GPL-3.0-or-later
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
foreach(test IN ITEMS compositor_attachment compositor_attachment_recovery)
  qt_add_executable(qindaqt_${test}_tests tst_${test}.cpp attachment_test_support.h)
  target_link_libraries(qindaqt_${test}_tests PRIVATE QindaQt::CompositorAttachment
      QindaQt::CompositorNames QindaQt::RecoveryBackoff Qt6::Test)
  target_compile_definitions(qindaqt_${test}_tests PRIVATE
      QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}")
  qindaqt_enable_warnings(qindaqt_${test}_tests)
endforeach()
add_test(NAME qindaqt.compositor-attachment COMMAND qindaqt_compositor_attachment_tests)
add_test(NAME qindaqt.compositor-attachment-recovery
    COMMAND qindaqt_compositor_attachment_recovery_tests)
set_tests_properties(qindaqt.compositor-attachment-recovery PROPERTIES
    TIMEOUT 60 LABELS "platform;session;dbus")
