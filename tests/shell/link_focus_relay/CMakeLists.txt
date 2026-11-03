# SPDX-License-Identifier: GPL-3.0-or-later

# QindaLink focus relay (ADR-0440) against a fake Link1 on a private bus.
find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session)
if(QINDAQT_DBUS_RUN_SESSION)
    qt_add_executable(qindaqt_link_focus_relay_tests
        tst_link_focus_relay.cpp
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/linkfocusrelay.cpp"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/linkfocusrelay.h")
    target_include_directories(qindaqt_link_focus_relay_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/runtime")
    target_link_libraries(qindaqt_link_focus_relay_tests PRIVATE
        QindaQt::CompositorShellActions Qt6::Test Qt6::DBus)
    qindaqt_enable_warnings(qindaqt_link_focus_relay_tests)
    add_test(NAME qindaqt.shell-link-focus-relay
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_link_focus_relay_tests>)
    set_tests_properties(qindaqt.shell-link-focus-relay PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "shell")
endif()
