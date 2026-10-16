# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_executable(qindaqt_recovery_backoff_tests tst_recovery_backoff.cpp)
target_link_libraries(qindaqt_recovery_backoff_tests PRIVATE QindaQt::RecoveryBackoff Qt6::Test)
qindaqt_enable_warnings(qindaqt_recovery_backoff_tests)
add_test(NAME qindaqt.recovery-backoff COMMAND qindaqt_recovery_backoff_tests)
set_tests_properties(qindaqt.recovery-backoff PROPERTIES TIMEOUT 30 LABELS "platform;session")
