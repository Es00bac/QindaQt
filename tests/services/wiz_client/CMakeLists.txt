# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_wiz_client_tests tst_wiz_client.cpp
                                           support/fakewiztransport.h)
target_include_directories(qindaqt_wiz_client_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_wiz_client_tests PRIVATE QindaQt::WizClient Qt6::Test)
set_target_properties(qindaqt_wiz_client_tests PROPERTIES AUTOMOC ON)
qindaqt_enable_warnings(qindaqt_wiz_client_tests)
add_test(NAME qindaqt.wiz-client COMMAND qindaqt_wiz_client_tests)
set_tests_properties(
    qindaqt.wiz-client PROPERTIES LABELS "service;smart-lights;runtime"
)
