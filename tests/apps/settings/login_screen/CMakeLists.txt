# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_login_screen_test target)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    qt_add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(
        ${target} PRIVATE qindaqt_settings_login_screen Qt6::Core Qt6::Test
    )
    qindaqt_enable_warnings(${target})
endfunction()

qindaqt_login_screen_test(
    qindaqt_settings_login_screen_config_test
    SOURCES tst_sddm_owned_config.cpp
)
add_test(NAME qindaqt.settings-login-screen-config
         COMMAND qindaqt_settings_login_screen_config_test)
set_tests_properties(
    qindaqt.settings-login-screen-config PROPERTIES
    LABELS "settings;login-screen" TIMEOUT 60
)

qindaqt_login_screen_test(
    qindaqt_settings_login_screen_discovery_test
    SOURCES tst_sddm_discovery.cpp
)
add_test(NAME qindaqt.settings-login-screen-discovery
         COMMAND qindaqt_settings_login_screen_discovery_test)
set_tests_properties(
    qindaqt.settings-login-screen-discovery PROPERTIES
    LABELS "settings;login-screen" TIMEOUT 60
)

qindaqt_login_screen_test(
    qindaqt_settings_login_screen_store_test
    SOURCES tst_sddm_config_store.cpp
)
add_test(NAME qindaqt.settings-login-screen-store
         COMMAND qindaqt_settings_login_screen_store_test)
set_tests_properties(
    qindaqt.settings-login-screen-store PROPERTIES
    LABELS "settings;login-screen" TIMEOUT 60
)

qindaqt_login_screen_test(
    qindaqt_settings_login_screen_model_test
    SOURCES tst_login_screen_settings_model.cpp
)
add_test(NAME qindaqt.settings-login-screen-model
         COMMAND qindaqt_settings_login_screen_model_test)
set_tests_properties(
    qindaqt.settings-login-screen-model PROPERTIES
    # No bus at all: the model must project file truth and a dead-probe
    # read-only state without touching D-Bus.
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;login-screen" TIMEOUT 60
)
