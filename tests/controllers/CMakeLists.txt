# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(qindaqt_controller_policy_test tst_controller_policy.cpp)
target_link_libraries(qindaqt_controller_policy_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.controller-policy COMMAND qindaqt_controller_policy_test)
set_tests_properties(qindaqt.controller-policy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

add_library(qindaqt_controller_settings_test_support STATIC
    ../../src/apps/settings/input/controllers_model.cpp
    ../../src/apps/settings/input/qt_controller_port.cpp
    ../../src/apps/settings/input/include/qindaqt/apps/settings_input/controllers_model.h
    ../../src/apps/settings/input/include/qindaqt/apps/settings_input/controller_port.h)
target_include_directories(qindaqt_controller_settings_test_support PUBLIC ../../src/apps/settings/input/include)
target_link_libraries(qindaqt_controller_settings_test_support PUBLIC qindaqt_controller_policy Qt6::DBus)
add_executable(qindaqt_controller_settings_test tst_controller_settings.cpp)
target_link_libraries(qindaqt_controller_settings_test PRIVATE qindaqt_controller_settings_test_support Qt6::Test)
add_test(NAME qindaqt.controller-settings COMMAND qindaqt_controller_settings_test)
set_tests_properties(qindaqt.controller-settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

if(TARGET qindaqt_controller_runtime)
    add_executable(qindaqt_controller_runtime_test tst_controller_runtime.cpp)
    target_link_libraries(qindaqt_controller_runtime_test PRIVATE qindaqt_controller_runtime Qt6::Test)
    add_test(NAME qindaqt.controller-runtime COMMAND qindaqt_controller_runtime_test)
    set_tests_properties(qindaqt.controller-runtime PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
endif()
