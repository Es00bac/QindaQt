# SPDX-License-Identifier: GPL-3.0-or-later
# QindaLink app view-model tests. Rows are named qindaqt.link-app-*. They run over FakeLinkApi and
# FakeProcessRunner with QTemporaryDir state: no bus, no phone, no window, no real config.
find_package(Qt6 REQUIRED COMPONENTS Test)

function(qindaqt_link_app_test name source)
    qt_add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE QindaQt::LinkAppView Qt6::Test)
    qindaqt_enable_warnings(${name})
    string(REPLACE "qindaqt_" "qindaqt." row ${name})
    string(REPLACE "_" "-" row ${row})
    string(REPLACE "qindaqt.link-app-" "qindaqt.link-app-" row ${row})
    string(REGEX REPLACE "-test$" "" row ${row})
    add_test(NAME ${row} COMMAND ${name})
    set_tests_properties(${row} PROPERTIES LABELS "app;link" TIMEOUT 30)
endfunction()

qindaqt_link_app_test(qindaqt_link_app_conversation_test tst_conversation.cpp)
qindaqt_link_app_test(qindaqt_link_app_devices_test tst_devices.cpp)
qindaqt_link_app_test(qindaqt_link_app_phone_apps_test tst_phone_apps.cpp)
qindaqt_link_app_test(qindaqt_link_app_options_test tst_options.cpp)
