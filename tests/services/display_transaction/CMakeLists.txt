# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_display_transaction_state_tests tst_transaction_state.cpp)
target_link_libraries(
    qindaqt_display_transaction_state_tests
    PRIVATE QindaQt::DisplayTransaction Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_transaction_state_tests)
add_test(NAME qindaqt.display-transaction-state COMMAND qindaqt_display_transaction_state_tests)
set_tests_properties(qindaqt.display-transaction-state PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_transaction_recovery_tests tst_transaction_recovery.cpp)
target_link_libraries(
    qindaqt_display_transaction_recovery_tests
    PRIVATE QindaQt::DisplayTransaction Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_transaction_recovery_tests)
add_test(NAME qindaqt.display-transaction-recovery COMMAND qindaqt_display_transaction_recovery_tests)
set_tests_properties(qindaqt.display-transaction-recovery PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_transaction_adversarial_tests tst_transaction_adversarial.cpp)
target_link_libraries(
    qindaqt_display_transaction_adversarial_tests
    PRIVATE QindaQt::DisplayTransaction Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_transaction_adversarial_tests)
add_test(NAME qindaqt.display-transaction-adversarial COMMAND qindaqt_display_transaction_adversarial_tests)
set_tests_properties(qindaqt.display-transaction-adversarial PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_transaction_invalid_ordering_tests tst_transaction_invalid_ordering.cpp)
target_link_libraries(
    qindaqt_display_transaction_invalid_ordering_tests
    PRIVATE QindaQt::DisplayTransaction Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_transaction_invalid_ordering_tests)
add_test(NAME qindaqt.display-transaction-invalid-ordering COMMAND qindaqt_display_transaction_invalid_ordering_tests)
set_tests_properties(qindaqt.display-transaction-invalid-ordering PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_transaction_journal_tests tst_transaction_journal.cpp)
target_link_libraries(
    qindaqt_display_transaction_journal_tests
    PRIVATE QindaQt::DisplayTransaction Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_transaction_journal_tests)
add_test(NAME qindaqt.display-transaction-journal COMMAND qindaqt_display_transaction_journal_tests)
set_tests_properties(qindaqt.display-transaction-journal PROPERTIES LABELS "service;display")
