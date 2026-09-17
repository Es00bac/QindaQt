# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_wiz_model_tests tst_wiz_model.cpp)
target_link_libraries(qindaqt_wiz_model_tests PRIVATE QindaQt::WizModel Qt6::Test)
qindaqt_enable_warnings(qindaqt_wiz_model_tests)
add_test(NAME qindaqt.wiz-model COMMAND qindaqt_wiz_model_tests)
set_tests_properties(
    qindaqt.wiz-model PROPERTIES LABELS "service;smart-lights;pure"
)
