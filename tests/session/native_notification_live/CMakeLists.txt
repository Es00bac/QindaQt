# SPDX-License-Identifier: GPL-3.0-or-later
if(TARGET QindaQt::SessionSupervisorSupport AND TARGET qindaqt-shell)
    qt_add_executable(qindaqt_native_notification_live_tests
        tst_native_notification_live.cpp native_popup_fixture.cpp native_popup_fixture.h
        ../compositorprobeclient.cpp ../compositorprobeclient.h
        ../notificationliveevidenceclient.cpp ../notificationliveevidenceclient.h
        ../notificationliveruntime.cpp ../notificationliveruntime.h
        ../notificationlivesurfaces.cpp ../notificationlivesurfaces.h)
    target_include_directories(qindaqt_native_notification_live_tests PRIVATE ..)
    target_link_libraries(qindaqt_native_notification_live_tests PRIVATE
        QindaQt::SessionSupervisorSupport QindaQt::CompositorAttachment
        QindaQt::SessionLockState QindaQt::CompositorNames Qt6::Test Qt6::Gui)
    qindaqt_enable_warnings(qindaqt_native_notification_live_tests)
    set(QINDAQT_NATIVE_POPUP_PRODUCTION_BUILD "${CMAKE_BINARY_DIR}" CACHE PATH
        "Immutable compatible production artifacts for native notification qualification")
    if(QINDAQT_KWIN_WAYLAND)
        add_test(NAME shell.notification-native-popup
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_native_popup.py"
                --probe "$<TARGET_FILE:qindaqt_native_notification_live_tests>"
                --compositor "${QINDAQT_KWIN_WAYLAND}"
                --launcher "${QINDAQT_NATIVE_POPUP_PRODUCTION_BUILD}/src/session/qindaqt-wm"
                --plugin-root "${QINDAQT_NATIVE_POPUP_PRODUCTION_BUILD}/plugins"
                --shell "${QINDAQT_NATIVE_POPUP_PRODUCTION_BUILD}/src/shell/qindaqt-shell"
                --host "${QINDAQT_NATIVE_POPUP_PRODUCTION_BUILD}/src/services/notification_host/qindaqt-notification-host"
                --settings "${QINDAQT_NATIVE_POPUP_PRODUCTION_BUILD}/src/services/settings_service/qindaqt-settings-service"
                --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/evidence")
        set_tests_properties(shell.notification-native-popup PROPERTIES
            TIMEOUT 120 RUN_SERIAL TRUE RESOURCE_LOCK qindaqt-private-session
            LABELS "integration;shell;notification;wayland;security;native;private-fixture")
    endif()
endif()
