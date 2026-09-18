# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_startup_test target)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    qt_add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE qindaqt_settings_startup Qt6::Core Qt6::Test)
    qindaqt_enable_warnings(${target})
endfunction()

qindaqt_startup_test(
    qindaqt_settings_startup_store_test
    SOURCES tst_xdg_autostart_store.cpp
)
add_test(NAME qindaqt.settings-startup-store
         COMMAND qindaqt_settings_startup_store_test)
set_tests_properties(
    qindaqt.settings-startup-store PROPERTIES
    LABELS "settings;startup" TIMEOUT 60
)

qindaqt_startup_test(
    qindaqt_settings_startup_model_test
    SOURCES tst_startup_settings_model.cpp
)
add_test(NAME qindaqt.settings-startup-model
         COMMAND qindaqt_settings_startup_model_test)
set_tests_properties(
    qindaqt.settings-startup-model PROPERTIES
    LABELS "settings;startup" TIMEOUT 60
)
